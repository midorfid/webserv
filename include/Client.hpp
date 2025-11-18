#pragma once

#include <string>
#include "httpRequest.hpp"
#include "ParseRequest.hpp"
#include "cgi.hpp"

class Server;

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

	private:
	
		
		std::string		_ip_string;
		std::string		_port;
		int				_sock_fd;

		bool			_is_ready;
		std::string		_request_buffer;
        int				_keep_alive_timer;
        ParseRequest    _parser;
		HttpRequest     _req;
};
