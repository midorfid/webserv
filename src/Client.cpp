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

Client::Client() : _req_start_time(0), _last_activity(time(NULL)), _state(IDLE), _request_buffer(""), _parser(), _req(), _cgi_state(false) {}

Client::~Client() {}

Client::Client(const Client &other) { *this = other; }

Client &Client::operator=(const Client &other) {
    if (this != &other) {
        this->_request_buffer = other._request_buffer;
        this->_last_activity = other._last_activity;
        this->_parser = other._parser;
        this->_req = other._req;
		this->_ip_string = other._ip_string;
		this->_port = other._port;
		this->_sock_fd = other._sock_fd;
		this->_cgi_state = other._cgi_state;
    }
    return *this;
}

const HttpRequest &
Client::req() const {
    return _req;
}


ParseResult
Client::processNewData(Server &server) {
    ssize_t				bytes_read;
	char				temp_buf[BUF_SIZE];

	bytes_read = recv(_sock_fd, temp_buf, BUF_SIZE, 0);
	if (bytes_read > 0) {
		if (getClientState() == IDLE) {
			_state = READING_HEADERS;
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
					size_t len = std::strtoul(len_str.c_str(), NULL, 10); 
					if (len > static_cast<size_t>(server.getConfig().getMaxBodySize()))
						return BodyTooLarge;
				}
				if (_req.getHeader("connection") == "close")
					_last_activity = 0;

				_state = READING_BODY;
			
				_request_buffer.erase(0, endofheaders + 4);
			}
		}
		if (getClientState() == READING_BODY) {
			if (_parser.parseBody(_request_buffer, _req) == RequestComplete) {
				_state = WRITING_RESPONSE;
				return RequestComplete;
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
}

bool
Client::isKeepAliveConn() const {
	return _last_activity != 0;
}

Client::Client(std::string &ip, std::string &port, int sock_fd) : _req_start_time(0), _last_activity(time(NULL)), _state(IDLE),
		_ip_string(ip), _port(port), _sock_fd(sock_fd), _request_buffer(""), _parser(), _req(), _cgi_state(false) {
	logTime(REGLOG);
	std::cout << "CLient constructor, ip: " << _ip_string << ", port: " << _port << std::endl;
}

Client::Client(int sock_fd) : _sock_fd(sock_fd) {}

CgiInfo &
Client::cgi_state() {
	return _cgi_state;
}
