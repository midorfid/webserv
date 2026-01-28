#pragma once

#include <string>
#include "httpRequest.hpp"
#include "ParseRequest.hpp"
#include "cgi.hpp"
#include <time.h>

class Server;

enum RequestStatus {
	UrlTooLong,
	RequestReceived,
	RequestIncomplete,
	Error,
	NothingToRead,
};

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
	
	RequestStatus processNewData();
	
	const ClientState	&getClientState() const;
	const time_t		&getLastActivity() const;
	void				updateLastActivity();
	bool				isKeepAliveConn() const;
	std::string const	&ip() const;
	std::string const	&port() const;
	void				reset();
	CgiInfo				&cgi_state();

	private:
	
		time_t			_req_start_time;
        time_t			_last_activity;
		ClientState		_state;
		std::string		_ip_string;
		std::string		_port;
		int				_sock_fd;

		std::string		_request_buffer;
        ParseRequest    _parser;
		HttpRequest     _req;
		CgiInfo			_cgi_state;
};
