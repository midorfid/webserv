#include <cctype>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <cstdlib>
#include <cstdio>
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <errno.h>
#include <iostream>
#include <map>
#include "ParseConfig.hpp"
#include "Client.hpp"
#include "server.hpp"
#include <fcntl.h>
#include <arpa/inet.h>
#include <ctime>
#include <csignal>
#include <sys/wait.h>

#define LISTEN_BACKLOG 50
#define MAX_EVENTS 10

int Server::_signal_write_fd = -1;

void	Server::hints_init(struct addrinfo *hints) {
	memset(hints, 0, sizeof(*hints));
	hints->ai_family = AF_UNSPEC;
	hints->ai_socktype = SOCK_STREAM;
	hints->ai_flags = AI_PASSIVE;
	hints->ai_protocol = 0;
	hints->ai_canonname = NULL;
	hints->ai_next = NULL;
}

void	Server::disconnect_client(int client_fd) {
	auto it = _clients.find(client_fd);
	if (it != _clients.end())
		disconnectCgiFds(*it->second);
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1) {
		logTime(ERRLOG);
		fprintf(stderr, "epoll_ctl (DEL): %s\n", strerror(errno));
	}
	logTime(REGLOG);
	_clients.erase(client_fd);
	std::cout << "Client on fd " << client_fd << " disconnected." << std::endl;
}

std::map<int, std::unique_ptr<Client>>::iterator Server::disconnect_client(std::map<int, std::unique_ptr<Client>>::iterator &it, int client_fd) {
	disconnectCgiFds(*it->second);
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1) {
		logTime(ERRLOG);
		fprintf(stderr, "epoll_ctl (DEL): %s\n", strerror(errno));
	}
	logTime(REGLOG);
	std::cout << "Client on fd " << client_fd << " disconnected." << std::endl;
	return	_clients.erase(it);
}

void Server::run(const std::string &cfg_file) {
	struct epoll_event ev;

	_vhosts = _ConfigParser.parse(cfg_file);
	if (_vhosts.empty()) {
		logTime(ERRLOG);
		std::cerr << "Error: No server blocks in config." << std::endl;
		exit(EXIT_FAILURE);
	}

	// Build port → vhost index map; bind each unique port exactly once
	for (size_t i = 0; i < _vhosts.size(); ++i) {
		std::string port_str;
		if (!_vhosts[i].getPort("", port_str)) {
			logTime(ERRLOG);
			std::cerr << "Error: server block " << i << " has no listen port." << std::endl;
			exit(EXIT_FAILURE);
		}
		int port = std::stoi(port_str);
		_port_to_vhost_idx[port].push_back(i);
	}

	for (auto &[port, indices] : _port_to_vhost_idx) {
		(void)indices;
		int fd = bind_port(port);
		if (listen(fd, LISTEN_BACKLOG) == -1) {
			logTime(ERRLOG);
			fprintf(stderr, "listen: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		_sock_to_port[fd] = port;
		logTime(REGLOG);
		std::cout << "Listening on port " << port << " (fd " << fd << ")" << std::endl;
	}

	this->init_epoll(&ev);
	this->run_event_loop(&ev);
}

int	Server::bind_port(int port) {
	struct addrinfo		*result, *rp;
	struct addrinfo		hints;
	int					s, optval_int, listen_fd = -1;
	std::string			port_str = std::to_string(port);

	this->hints_init(&hints);
	s = getaddrinfo(NULL, port_str.c_str(), &hints, &result);
	if (s != 0) {
		logTime(ERRLOG);
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	optval_int = 1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (listen_fd == -1)
			continue;

		if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval_int, sizeof(int)) == -1) {
			logTime(ERRLOG);
			fprintf(stderr, "setsockopt: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(listen_fd);
		listen_fd = -1;
	}
	freeaddrinfo(result);

	if (listen_fd == -1) {
		logTime(ERRLOG);
		fprintf(stderr, "Could not bind port %d\n", port);
		exit(EXIT_FAILURE);
	}
	return listen_fd;
}

void	Server::init_epoll(epoll_event *ev) {
	this->_epoll_fd = epoll_create1(0);
	if (this->_epoll_fd == -1) {
		logTime(ERRLOG);
		fprintf(stderr, "epoll_create1: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	ev->events = EPOLLIN;
	for (auto &[fd, port] : _sock_to_port) {
		(void)port;
		ev->data.fd = fd;
		if (epoll_ctl(this->_epoll_fd, EPOLL_CTL_ADD, fd, ev) == -1) {
			logTime(ERRLOG);
			fprintf(stderr, "epoll_ctl (listen fd %d): %s\n", fd, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	ev->data.fd = this->_signal_read_fd;
	if (epoll_ctl(this->_epoll_fd, EPOLL_CTL_ADD, this->_signal_read_fd, ev) == -1) {
		logTime(ERRLOG);
		fprintf(stderr, "epoll_ctl: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void
Server::disconnectCgiFds(Client &client) {
	CgiInfo &cgi = client.cgi_state();

	if (cgi.read_fd != -1) {
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, cgi.read_fd, NULL);
		_cgi_client.erase(cgi.read_fd);
		close(cgi.read_fd);
		cgi.read_fd = -1;
	}
	if (cgi.write_fd != -1) {
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, cgi.write_fd, NULL);
		_cgi_client.erase(cgi.write_fd);
		close(cgi.write_fd);
		cgi.write_fd = -1;
	}
	if (cgi.child_pid != -1) {
		kill(cgi.child_pid, SIGKILL);
		waitpid(cgi.child_pid, NULL, WNOHANG);
		cgi.child_pid = -1;
	}
}

void 
Server::handle_cgi_write(int write_fd) {
	const int	&client_fd = _cgi_client[write_fd];
	Client		&client = *_clients[client_fd]; 
	const std::string &body = client.req().getBody();
	int			remaining;
	
	logTime(REGLOG);
	std::cout << "cgi write_fd:" << write_fd << std::endl;

	remaining = body.length() - client.bytes_written_to_cgi;
	ssize_t sent = write(write_fd, body.c_str() + client.bytes_written_to_cgi, remaining);
	if (sent > 0) {
		client.bytes_written_to_cgi += sent;
		if (body.length() == static_cast<size_t>(client.bytes_written_to_cgi)) {
			close(write_fd);
			epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, NULL);
		}
	}
	else if (sent == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// pipe full
			return;
		}
		else {
			disconnectCgiFds(client);
			terminateConnWithError(client_fd, 500);
		}
	}
}

void
Server::parseAndQoutputBuf(Client &client, int client_fd) {
	std::string		&out_buf = client.cgi_state().output_buf;
	ResponseState	&resp = client.resp_state;

	size_t head_end = out_buf.find("\r\n\r\n");
	if (head_end == std::string::npos)
		return;

	resp = ResponseState(200);  // reset and default to 200

	std::string_view headers_view(out_buf.data(), head_end);
	std::string      body = out_buf.substr(head_end + 4);

	size_t headers_offset = 0;

	while (headers_offset < head_end) {
		size_t end_line = headers_view.find("\r\n", headers_offset);
		if (end_line == std::string_view::npos)
			end_line = head_end;

		std::string_view line = headers_view.substr(headers_offset, end_line - headers_offset);
		size_t colon = line.find(':');

		if (colon != std::string_view::npos) {
			std::string key(line.substr(0, colon));
			std::string val(line.substr(colon + 1));

			StringUtils::trimWhitespaces(key);
			StringUtils::trimWhitespaces(val);

			for (auto &c : key)
				c = std::tolower(static_cast<unsigned char>(c));

			if (key == "status") {
				try { resp.status_code = std::stoi(val); }
				catch (...) {}
			} else {
				resp.addHeader(key, val);
			}
		}
		headers_offset = (end_line >= head_end) ? head_end : end_line + 2;
	}

	Response::finalizeResponse(resp, client.req().getPath(), body.length(), client.isKeepAliveConn());
	client.queueResponse(Response::build(resp) + body);

	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT;
	ev.data.fd = client_fd;
	epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
}

void
Server::handle_cgi_read(int &read_fd) {
	int				&client_fd = _cgi_client[read_fd];
	Client			&client = *_clients[client_fd];
	std::string		&out_buf = client.cgi_state().output_buf;

	logTime(REGLOG);
	std::cout << "cgi read_fd:" << read_fd << std::endl;
	
	char buf[4096];
	ssize_t bytes_read = read(read_fd, buf, 4096);
	
	logTime(REGLOG);
	std::cout << "bytes read: " << bytes_read << std::endl;
	if (bytes_read > 0) {
		out_buf.append(buf, bytes_read);
	}
	else if (bytes_read == 0) {
		close(read_fd);
		_cgi_client.erase(read_fd);
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, read_fd, NULL);
		read_fd = -1;

		pid_t &pid = client.cgi_state().child_pid;
		if (pid != -1) {
			waitpid(pid, NULL, WNOHANG);
			pid = -1;
		}

		parseAndQoutputBuf(client, client_fd);
	}
	else /*(bytes_read == -1)*/ {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			// pipe full
			return;
		}
		else {
			disconnectCgiFds(client);
			terminateConnWithError(client_fd, 500);
		}
	}
}

double
Server::diffTime(const time_t &client_tm) {
	time_t now = time(NULL);
	
	return static_cast<double>(now - client_tm);
}

void	Server::checkTimeouts() {
	for (auto it = _clients.begin(); it != _clients.end();) {
		double quiet_time = diffTime(it->second->getLastActivity());

		if (it->second->getClientState() != IDLE) {
			if (quiet_time > 60) {
				_handler.sendDefaultError(408, *it->second);
				it = disconnect_client(it, it->first);
			}
		}
		if (quiet_time > static_cast<double>(_vhosts.front().getKeepAliveTimer())) {
			it = disconnect_client(it, it->first);
		}
		if (_clients.empty())
			break;
		else
			++it;
	}
}

void
Server::cleanup() { // here
	std::cout << "cleanup" << std::endl;
	this->~Server();
	exit(EXIT_SUCCESS);
}

void
Server::handle_signal(int signum) {
	(void)signum;

	char c = 'X';

	write(_signal_write_fd, &c, 1);
}

void	Server::run_event_loop(epoll_event *ev) {
	int                 			conn_sock, nfds;
	struct epoll_event				events[MAX_EVENTS];
	// std::map<int, Client>			clients;

	signal(SIGINT, Server::handle_signal);

	for (;;) {
		nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, 1000);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			logTime(ERRLOG);
			fprintf(stderr, "epoll_wait: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < nfds; ++i) {
			int curr_fd = events[i].data.fd;
			if (curr_fd == _signal_read_fd)
				cleanup();
			else if (_sock_to_port.count(curr_fd)) {
				int server_port = _sock_to_port[curr_fd];
				struct sockaddr_storage client_addr;
				socklen_t clientaddr_len = sizeof(client_addr);

				conn_sock = accept(curr_fd, reinterpret_cast<sockaddr*>(&client_addr), &clientaddr_len);
				if (conn_sock == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						continue;
					else {
						logTime(ERRLOG);
						fprintf(stderr, "accept: %s\n", strerror(errno));
						continue;
					}
				}
				if (fcntl(conn_sock, F_SETFL, O_NONBLOCK) == -1) {
					logTime(ERRLOG);
					fprintf(stderr, "fcntl: %s\n", strerror(errno));
					close(conn_sock);
					continue;
				}
				ev->events = EPOLLIN;
				ev->data.fd = conn_sock;
				if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, conn_sock, ev) == -1) {
					logTime(ERRLOG);
					fprintf(stderr, "epoll_ctl (conn_sock): %s\n", strerror(errno));
					close(conn_sock);
				}
				else {
					std::pair<std::string, std::string> ipPort = getClientAddr(client_addr);
					_clients[conn_sock] = std::make_unique<Client>(ipPort.first, ipPort.second, conn_sock);
					_clients[conn_sock]->setServerPort(server_port);
					logTime(REGLOG);
					std::cout << "New connection on fd " << conn_sock << " (port " << server_port << ")" << std::endl;
				}
			}
			else if (_cgi_client.find(curr_fd) != _cgi_client.end()) {
				if (events[i].events & EPOLLIN)
					handle_cgi_read(curr_fd);
				if (events[i].events & EPOLLOUT)
					handle_cgi_write(curr_fd);
				// clear?
			}
			else if (_clients.find(curr_fd) != _clients.end()){
				handle_client_event(curr_fd); // add return value in case the client needs to be disconnected TODO
			}
			else {
				logTime(ERRLOG);
				std::cerr << "Received event on unknown fd.";
				epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, curr_fd, NULL);
				close(curr_fd);
			}
		}
		checkTimeouts();
	}
}


bool
Server::epoll_add_cgi(std::pair<int,int> cgi_fds, int client_fd) {
	epoll_event	ev;

	ev.events = EPOLLIN;
	ev.data.fd = cgi_fds.first;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, cgi_fds.first, &ev) == -1) {
		logTime(ERRLOG);
		fprintf(stderr, "epoll_ctl (cgi read sock): %s\n", strerror(errno));
		close(cgi_fds.first);
		return false;
	}

	ev.events = EPOLLOUT;
	ev.data.fd = cgi_fds.second;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, cgi_fds.second, &ev) == -1) {
		logTime(ERRLOG);
		fprintf(stderr, "epoll_ctl (cgi write sock): %s\n", strerror(errno));
		close(cgi_fds.second);
		return false;
	}
	
	_cgi_client[cgi_fds.first] = client_fd;
	_cgi_client[cgi_fds.second] = client_fd;

	return true;
}

Server::Server() : _epoll_fd(-1), _clients() {
	int signal_fds[2];
	if (pipe(signal_fds) == 0) {
		_signal_read_fd = signal_fds[0];
		_signal_write_fd = signal_fds[1];
	}
}

Server::~Server() {
	for (auto &[fd, port] : _sock_to_port) {
		(void)port;
		close(fd);
	}
	if (this->_epoll_fd != -1)
		close(this->_epoll_fd);
}

std::pair<std::string, std::string>
Server::getClientAddr(struct sockaddr_storage &client_addr) {
	sockaddr								*sock_addr = reinterpret_cast<sockaddr *>(&client_addr);
	std::pair<std::string, std::string>		ipPort;
	char									ip_cstr[INET6_ADDRSTRLEN];

	if (sock_addr->sa_family == AF_INET) {
		sockaddr_in *sa_in = reinterpret_cast<sockaddr_in*>(sock_addr);
		if (inet_ntop(AF_INET, &sa_in->sin_addr, ip_cstr, sizeof(ip_cstr)) == NULL) {
			logTime(ERRLOG);
			std::cerr << "inet_ntop ipv4" << "\n";
		}
		ipPort.second = std::to_string(ntohs(sa_in->sin_port));
	}
	else {
		sockaddr_in6 *sa_in6 = reinterpret_cast<sockaddr_in6*>(sock_addr);
		if (inet_ntop(AF_INET6, &sa_in6->sin6_addr, ip_cstr, sizeof(ip_cstr)) == NULL) {
			logTime(ERRLOG);
			std::cerr << "inet_ntop ipv6" << "\n";
		}
		ipPort.second = std::to_string(ntohs(sa_in6->sin6_port));
	}
	ipPort.first = ip_cstr;

	return ipPort;
}

void
Server::handleDefault(Client &client, int client_fd) {
	const std::string host = client.req().getHeader("host");
	const Config &config = selectVhost(client.getServerPort(), host);
	ResolvedAction action = _route_reslvr.resolveRequestToHandler(config, client.req(), client.ip());
	_handler.handle(client.req(), client, client.cgi_state(), action);

	client.updateLastActivity();

	const CgiInfo &cgi = client.cgi_state();

	if (cgi.isCgi()) {
		epoll_add_cgi(std::make_pair(cgi.getReadfd(), cgi.getWritefd()), client_fd);
	}
	else {
		struct epoll_event ev;
		ev.events = EPOLLIN | EPOLLOUT;
		ev.data.fd = client_fd;
		epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
	}
}

void
Server::terminateConnWithError(int client_fd, int error_code) {
	Client &client = *_clients[client_fd];
	_handler.sendDefaultError(error_code, client);
	client.disableKeepAlive();
	
	struct epoll_event ev;
	ev.events = EPOLLOUT;
	ev.data.fd = client_fd;
	epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
}

void
Server::handle_client_event(int client_fd) {
	try {
		Client &client = *_clients.at(client_fd);

		if (client.getClientState() == WRITING_RESPONSE) {
			bool done = client.writeResponseChunk();
			if (done) {
				if (client.isKeepAliveConn()) {
					client.reset();
					struct epoll_event ev;
					ev.events = EPOLLIN;
					ev.data.fd = client_fd;
					epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
				} else {
					disconnect_client(client_fd);
				}
			}
			return;
		}

		ParseResult status = client.processNewData(*this);
		switch (status) {
			case UrlTooLong:
				return terminateConnWithError(client_fd, 414);
			case BodyTooLarge:
				return terminateConnWithError(client_fd, 413);
			case HeadersTooLarge:
				return terminateConnWithError(client_fd, 431);
			case RequestComplete:
				return handleDefault(client, client_fd);
			case RequestIncomplete:
				return;
			case Error:
				return disconnect_client(client_fd);
			case NothingToRead:
				return disconnect_client(client_fd);
			default:
				return;
		}
	}
	catch(const std::exception& e)
	{
		logTime(ERRLOG);
		std::cerr << e.what() << '\n';
		disconnect_client(client_fd);
	}
}

const Config &
Server::getConfig() const {
	return _vhosts.front(); // first vhost as server-wide default
}

const Config &
Server::selectVhost(int port, const std::string &host) const {
	auto it = _port_to_vhost_idx.find(port);
	if (it == _port_to_vhost_idx.end())
		return _vhosts.front();

	const auto &indices = it->second;

	// Strip optional port suffix from Host header (e.g. "example.com:8080")
	std::string hostname = host;
	auto colon = hostname.rfind(':');
	if (colon != std::string::npos)
		hostname = hostname.substr(0, colon);

	// Exact server_name match
	for (size_t idx : indices) {
		for (const auto &name : _vhosts[idx].getServerNames()) {
			if (name == hostname)
				return _vhosts[idx];
		}
	}
	// Default: first config registered for this port
	return _vhosts[indices[0]];
}
