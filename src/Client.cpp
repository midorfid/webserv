#include "Client.hpp"
#include <iostream>
#include <map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include "server.hpp"

#define BUF_SIZE 4096
#define MAX_HEADERS_SIZE 8192

Client::Client() : bytes_written_to_cgi(0), _req_start_time(0), _last_activity(time(NULL)), _state(IDLE), _sock_fd(-1), _response_offset(0), _request_buffer(""), _parser(), _req(), _cgi_state(false) {}

Client::~Client() {
	if (_sock_fd != -1) {
		close(_sock_fd);
	}
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
		_request_buffer.reserve(sizeof(temp_buf));
		_request_buffer.append(temp_buf);
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
					int len = std::strtoul(len_str.c_str(), NULL, 10);
					int max_body_size = server.getConfig().getMaxBodySize();
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
		logTime(ERRLOG);
		fprintf(stderr, "Client: %s port\n", strerror(errno));
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
	_state = IDLE;
	bytes_written_to_cgi = 0;
	_response_queue.clear();
	_response_offset = 0;
}

bool
Client::isKeepAliveConn() const {
	return _last_activity != 0;
}

Client::Client(std::string &ip, std::string &port, int sock_fd) : bytes_written_to_cgi(0), _req_start_time(0), _last_activity(time(NULL)), _state(IDLE),
		_ip_string(ip), _port(port), _sock_fd(sock_fd), _response_offset(0), _request_buffer(""), _parser(), _req(), _cgi_state(false) {
	logTime(REGLOG);
	std::cout << "CLient constructor, ip: " << _ip_string << ", port: " << _port << std::endl;
}

Client::Client(int sock_fd) : bytes_written_to_cgi(0), _req_start_time(0), _last_activity(time(NULL)), _state(IDLE), _sock_fd(sock_fd), _response_offset(0), _request_buffer(""), _parser(), _req(), _cgi_state(false) {}

CgiInfo &
Client::getCgi_state() {
	return _cgi_state;
}

void Client::queueResponse(const std::string &response) {
	_response_queue.append(response);
	_state = WRITING_RESPONSE;
}

bool Client::writeResponseChunk() {
	if (_response_offset >= _response_queue.size()) return true;

	size_t remaining = _response_queue.size() - _response_offset;
	size_t chunk_size = std::min(remaining, static_cast<size_t>(8192)); // Strict 8KB limits

	ssize_t sent = send(_sock_fd, _response_queue.data() + _response_offset, chunk_size, 0);
	if (sent > 0) {
		_response_offset += sent;
		updateLastActivity();
		if (_response_offset >= _response_queue.size()) {
			_state = DONE;
			return true;
		}
	} else if (sent == -1) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			throw std::runtime_error("Write error on client socket");
		}
	}
	return false;
}
