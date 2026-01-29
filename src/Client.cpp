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

Client::Client() : _req_start_time(0), _state(IDLE), _request_buffer(""), _last_activity(time(NULL)), _parser(), _req(), _cgi_state(false) {}

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


RequestStatus
Client::processNewData() {
    ssize_t				bytes_read;
	char				temp_buf[BUF_SIZE];

	bytes_read = recv(_sock_fd, temp_buf, BUF_SIZE, 0);
	if (bytes_read > 0) {
		_request_buffer.reserve(sizeof(temp_buf));
		_request_buffer.append(temp_buf);
		if (getClientState() == READING_HEADERS) {
			size_t endofheaders = _request_buffer.find("\r\n\r\n");

			if (endofheaders == std::string::npos) {
				if (_request_buffer.length() > MAX_HEADERS_SIZE) {
					// ... 
					return HeadersTooLarge;
				}
			}
			else {
				_state = READING_BODY;
			}
		}
		// add "\r\n\r\n" check and max_header_size -> error 431
		// TODO potentially dynamically allocate memore if keep_alive
		ParseRequest::ParseResult status = _parser.parse(_request_buffer, _req);
		// error 413  for too large body, check config
		if (_req.getHeader("connection") == "close")
			_last_activity = 0;
		switch(status) {
			case ParseRequest::ParseResult::ParsingComplete:
				return RequestReceived;
			case ParseRequest::ParseResult::ParsingIncomplete:
				return RequestIncomplete;
			case ParseRequest::ParseResult::ParsingError:
				return UrlTooLong;
		}
	}
	else if (bytes_read == 0) {
		logTime(REGLOG);
		std::cout << "bytes_read == 0" << std::endl;
		return NothingToRead;
		// handle timed_conns if disconnect here TODO
		// rather reset if keep_alive
	}
	else {
		logTime(ERRLOG);
		fprintf(stderr, "Client: %s port\n", strerror(errno));
		return Error;
	}
}

const ClientState &
Client::getClientState() const {
	return _state;
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


Client::Client(std::string &ip, std::string &port, int sock_fd) : _ip_string(ip), _port(port), _sock_fd(sock_fd),
		_req_start_time(0), _state(IDLE), _request_buffer(""), _last_activity(time(NULL)), _parser(), _req(), _cgi_state(false) {
	logTime(REGLOG);
	std::cout << "CLient constructor, ip: " << _ip_string << ", port: " << _port << std::endl;
}

Client::Client(int sock_fd) : _sock_fd(sock_fd) {}

CgiInfo &
Client::cgi_state() {
	return _cgi_state;
}
