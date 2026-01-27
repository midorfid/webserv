#pragma once

#include <string>
#include "httpRequest.hpp"
#include "ParseRequest.hpp"
#include "cgi.hpp"
#include <time.h>

class Server;

enum ClientState {
	IDLE,
	READING_HEADERS,
	READING_BODY,
	WRITING_RESPONSE,
	DONE,
};

class Client {
	public:
	
	Client(int sock_fd);
	Client();
	Client(std::string &, std::string &, int sock_fd);
	~Client();
	
	Client(const Client &other);
	Client &operator=(const Client &other);
	
	
	const HttpRequest &req() const;
	
	void processNewData(Server *server);
	
	std::string const	&ip() const;
	bool				ready() const;
	std::string const	&port() const;
	void				reset();
	CgiInfo				&cgi_state();

	private:
	
		time_t			_req_start_time;
		ClientState		_state;
		std::string		_ip_string;
		std::string		_port;
		int				_sock_fd;

		bool			_is_ready;
		std::string		_request_buffer;
        int				_keep_alive_timer;
        ParseRequest    _parser;
		HttpRequest     _req;
		CgiInfo			_cgi_state;
};
