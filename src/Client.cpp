#include "Client.hpp"
#include <iostream>
#include <map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include "server.hpp"

#define BUF_SIZE 4096
#define MAX_REQUEST_SIZE 8192

Client::Client() : _request_buffer(""), _keep_alive_timer(0), _parser(), _req(), _cgi_state(false) {}

Client::~Client() {}

Client::Client(const Client &other) { *this = other; }

Client &Client::operator=(const Client &other) {
    if (this != &other) {
        this->_request_buffer = other._request_buffer;
        this->_keep_alive_timer = other._keep_alive_timer;
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


void
Client::processNewData(Server *server) {
    ssize_t				bytes_read;
	char				temp_buf[BUF_SIZE];

	bytes_read = recv(_sock_fd, temp_buf, BUF_SIZE, 0);
	if (bytes_read > 0) {
		if (bytes_read > MAX_REQUEST_SIZE) {
			logTime(REGLOG);
			std::cout << "HTTP/1.1 413 Payload Too Large\n" << std::endl;
			send(_sock_fd, "HTTP/1.1 413 Payload Too Large\r\n\r\n", 32, 0); // TODO
			server->disconnect_client(_sock_fd);
			return;
		}
		else {
    		_request_buffer.reserve(sizeof(temp_buf));
    		_request_buffer.append(temp_buf);
    		// TODO potentially dynamically allocate memore if keep_alive
    		if (_parser.parse(_request_buffer, _req) == ParseRequest::ParsingComplete) {
							
				_is_ready = true;
			}
			return;
		}
	}
	else if (bytes_read == 0) {
		logTime(REGLOG);
		std::cout << "bytes_read == 0" << std::endl;
        server->disconnect_client(_sock_fd);
		// handle timed_conns if disconnect here TODO
		// rather reset if keep_alive
	}
	else {
		logTime(ERRLOG);
		fprintf(stderr, "Client: %s port\n", strerror(errno));
        server->disconnect_client(_sock_fd);
	}
}


std::string const &
Client::ip() const {
    return _ip_string;
}

std::string const &
Client::port() const {
    return _port;
}

bool
Client::ready() const {
	return _is_ready;
}

void
Client::reset() {
	_request_buffer.clear();
	_req = HttpRequest();
	_is_ready = false;
	_cgi_state = CgiInfo(false);
}

Client::Client(std::string &ip, std::string &port, int sock_fd) : _ip_string(ip), _port(port), _sock_fd(sock_fd), _is_ready(false) {
	logTime(REGLOG);
	std::cout << "CLient constructor, ip: " << _ip_string << ", port: " << _port << std::endl;
}

Client::Client(int sock_fd) : _sock_fd(sock_fd) {}

CgiInfo &
Client::cgi_state() {
	return _cgi_state;
}
