#pragma once

#include <string>
#include "httpRequest.hpp"
#include "ParseRequest.hpp"
#include "cgi.hpp"
#include <time.h>
#include "response.hpp"

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
	
	Client(const Client &other) = delete;
	Client &operator=(const Client &other) = delete;
	
	
	const HttpRequest &req() const;
	
	ParseResult processNewData(Server &server);
	
	const ClientState	&getClientState() const;
	const time_t		&getLastActivity() const;
	void				updateLastActivity();
	bool				isKeepAliveConn() const;
	std::string const	&ip() const;
	std::string const	&port() const;
	void				reset();
	CgiInfo				&cgi_state() { return _cgi_state; }

	void				setServerPort(int port) { _server_port = port; }
	int					getServerPort() const   { return _server_port; }

	void				disableKeepAlive() { _last_activity = 0; }
	void				queueResponse(const std::string &response);
	bool				writeResponseChunk();

	int						bytes_written_to_cgi;
	ResponseState			resp_state;
	// create offset var for send
	private:
	
		time_t			_req_start_time;
        time_t			_last_activity;
		ClientState		_state;
		std::string		_ip_string;
		std::string		_port;
		int				_sock_fd;
		std::string		_response_queue;
		size_t			_response_offset;

		int				_server_port;
		std::string		_request_buffer;
        ParseRequest    _parser;
		HttpRequest     _req;
		CgiInfo			_cgi_state;
};
