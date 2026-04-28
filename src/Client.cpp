#include "Client.hpp"
#include <iostream>
#include <map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include "server.hpp"

#define BUF_SIZE 4096
#define MAX_HEADERS_SIZE 8192

Client::Client() : bytes_written_to_cgi(0), _req_start_time(0), _last_activity(time(NULL)), _state(IDLE), _sock_fd(-1), _response_offset(0), _server_port(0), _request_buffer(""), _parser(), _req(), _cgi_state(false), _stream_fd(-1), _chunk_offset(0), _stream_done(false) {}

Client::~Client() {
	if (_sock_fd != -1)
		close(_sock_fd);
	if (_stream_fd != -1)
		close(_stream_fd);
}

const HttpRequest &
Client::req() const {
    return _req;
}

void
Client::updateLastActivity() {
	_last_activity = time(NULL);
}

ParseResult
Client::processNewData(Server &server) {
    ssize_t				bytes_read;
	char				temp_buf[BUF_SIZE];

	bytes_read = recv(_sock_fd, temp_buf, BUF_SIZE, 0);
	if (bytes_read > 0) {
		if (getClientState() == IDLE) {
			_state = READING_HEADERS;
			updateLastActivity();
			_req_start_time = time(NULL);
		}
		_request_buffer.reserve(_request_buffer.size() + bytes_read);
		_request_buffer.append(temp_buf, bytes_read);
		if (getClientState() == READING_HEADERS) {
			size_t endofheaders = _request_buffer.find("\r\n\r\n");

			if (endofheaders == std::string::npos) {
				if (_request_buffer.length() > MAX_HEADERS_SIZE) {
					// ... 
					return HeadersTooLarge;
				}
				return RequestIncomplete;
			}
			else {
				ParseResult status = _parser.parseReqLineHeaders(_request_buffer, _req);
				if (status != Okay)
					return status;
				std::string len_str = _req.getHeader("content-length");
				if (!len_str.empty()) {
					size_t len = std::strtoul(len_str.c_str(), NULL, 10);
					size_t max_body_size = server.getConfig().getSharedCtx().client_max_body_size;
					if (len > max_body_size)
						return BodyTooLarge;
				}
				if (_req.getHeader("connection") == "close")
					_last_activity = 0;

				updateLastActivity();
				_state = READING_BODY;
				endofheaders = _request_buffer.find("\r\n\r\n");
				
				_request_buffer.erase(0, endofheaders + 4);
			}
		}
		if (getClientState() == READING_BODY) {
			if (_req.getHeader("transfer-encoding") == "chunked") {
				size_t max_body = server.getConfig().getSharedCtx().client_max_body_size;
				if (_request_buffer.size() > max_body)
					return BodyTooLarge;
			}
			ParseResult status = _parser.parseBody(_request_buffer, _req);
			if (status == NothingToRead)
				return NothingToRead;
			else {
				updateLastActivity();
				if (status == RequestComplete) {
					_state = WRITING_RESPONSE;
					return RequestComplete;
				}
			}
			return RequestIncomplete;
		}
	}
	else if (bytes_read == 0) {
		return NothingToRead;
	}
	else {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return RequestIncomplete;
		logTime(ERRLOG);
		fprintf(stderr, "Client recv: %s\n", strerror(errno));
		return Error;
	}
	return Okay;
}

const ClientState &
Client::getClientState() const {
	return _state;
}

const time_t &
Client::getLastActivity() const {
	return _last_activity;
}

std::string const &
Client::ip() const {
    return _ip_string;
}

std::string const &
Client::port() const {
    return _port;
}

void
Client::reset() {
	_request_buffer.clear();
	_req = HttpRequest();
	_last_activity = time(NULL);
	_req_start_time = 0;
	_cgi_state = CgiInfo(false);
	resp_state = ResponseState();
	_state = IDLE;
	bytes_written_to_cgi = 0;
	_response_queue.clear();
	_response_offset = 0;
	if (_stream_fd != -1) { close(_stream_fd); _stream_fd = -1; }
	_chunk_buf.clear();
	_chunk_offset = 0;
	_stream_done = false;
}

bool
Client::isKeepAliveConn() const {
	return _last_activity != 0;
}

Client::Client(std::string &ip, std::string &port, int sock_fd) : bytes_written_to_cgi(0), _req_start_time(0), _last_activity(time(NULL)), _state(IDLE),
		_ip_string(ip), _port(port), _sock_fd(sock_fd), _response_offset(0), _server_port(0), _request_buffer(""), _parser(), _req(), _cgi_state(false), _stream_fd(-1), _chunk_offset(0), _stream_done(false) {
	logTime(REGLOG);
	std::cout << "CLient constructor, ip: " << _ip_string << ", port: " << _port << std::endl;
}

Client::Client(int sock_fd) : bytes_written_to_cgi(0), _req_start_time(0), _last_activity(time(NULL)), _state(IDLE), _sock_fd(sock_fd), _response_offset(0), _server_port(0), _request_buffer(""), _parser(), _req(), _cgi_state(false), _stream_fd(-1), _chunk_offset(0), _stream_done(false) {}

void Client::queueResponse(const std::string &response) {
	_response_queue.append(response);
	_state = WRITING_RESPONSE;
}

bool Client::writeResponseChunk() {
	// Phase 1: send buffered headers (and body for non-streaming responses)
	if (_response_offset < _response_queue.size()) {
		size_t remaining = _response_queue.size() - _response_offset;
		size_t to_send = std::min(remaining, static_cast<size_t>(8192));
		ssize_t sent = send(_sock_fd, _response_queue.data() + _response_offset, to_send, 0);
		if (sent > 0) {
			_response_offset += sent;
			updateLastActivity();
		} else if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
			throw std::runtime_error("Write error on client socket");
		}
		if (_response_offset < _response_queue.size())
			return false;
		// Buffer fully sent; done if no file streaming is active
		if (_stream_fd == -1 && _chunk_buf.empty()) {
			_state = DONE;
			return true;
		}
		return false;
	}

	// Phase 2: chunked file streaming
	// If no pending chunk, read the next block from the file
	if (_chunk_buf.empty() && !_stream_done) {
		char read_buf[8192];
		ssize_t n = (_stream_fd != -1) ? read(_stream_fd, read_buf, sizeof(read_buf)) : 0;
		if (n > 0) {
			char hex[20];
			snprintf(hex, sizeof(hex), "%zx\r\n", static_cast<size_t>(n));
			_chunk_buf  = hex;
			_chunk_buf.append(read_buf, n);
			_chunk_buf += "\r\n";
		} else {
			// EOF or read error: close file and queue terminal chunk
			if (_stream_fd != -1) { close(_stream_fd); _stream_fd = -1; }
			_chunk_buf   = "0\r\n\r\n";
			_stream_done = true;
		}
		_chunk_offset = 0;
	}

	// Send whatever is pending in _chunk_buf
	if (!_chunk_buf.empty()) {
		ssize_t sent = send(_sock_fd, _chunk_buf.data() + _chunk_offset,
		                    _chunk_buf.size() - _chunk_offset, 0);
		if (sent > 0) {
			_chunk_offset += static_cast<size_t>(sent);
			updateLastActivity();
			if (_chunk_offset >= _chunk_buf.size()) {
				_chunk_buf.clear();
				_chunk_offset = 0;
				if (_stream_done) {
					_state = DONE;
					return true;
				}
			}
		} else if (sent == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
			throw std::runtime_error("Write error on client socket");
		}
	}

	return false;
}
