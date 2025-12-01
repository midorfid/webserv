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

#define LISTEN_BACKLOG 50
#define MAX_EVENTS 10

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
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1) {
		fprintf(stderr, "epoll_ctl (DEL): %s\n", strerror(errno));
	}
	close(client_fd);
	_clients.erase(client_fd);
	std::cout << "Client on fd " << client_fd << " disconnected." << std::endl;
}


void Server::run(const std::string &cfg_file) {
	struct epoll_event		ev;

	_ConfigParser.parse(cfg_file, this->_config);
	_config.setLocCgi();

	if (this->_config.getPort("listen", _port)) {
		this->init_sockets(_port.c_str());
	} else {
		std::cerr << "Error: No valid port found." << std::endl;
		exit(EXIT_FAILURE);
	}

	if (listen(this->_listen_sock, LISTEN_BACKLOG) == -1) {
		fprintf(stderr, "listen: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	this->init_epoll(&ev);
	this->run_event_loop(&ev);
}

void	Server::init_sockets(const char *port) {
	struct addrinfo			*result, *rp;
	struct addrinfo			hints;
	int						s, optval_int;
	
	this->hints_init(&hints);
	s = getaddrinfo(NULL, port, &hints, &result);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	optval_int = 1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		this->_listen_sock = socket(rp->ai_family, rp->ai_socktype,
					rp->ai_protocol);
		if (this->_listen_sock == -1)
			continue;

		if (setsockopt(this->_listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval_int, sizeof(int)) == -1) {
			fprintf(stderr, "setsockopt: %s\n", strerror(errno));
			exit(EXIT_FAILURE);    
		}

		if (bind(this->_listen_sock, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		
		close(this->_listen_sock);
	}
	freeaddrinfo(result);

	if (rp == NULL) {
		fprintf(stderr, "Could not bind\n");
		exit(EXIT_FAILURE);
	}

}

void	Server::init_epoll(epoll_event *ev) {
	this->_epoll_fd = epoll_create1(0);
	if (this->_epoll_fd == -1) {
		fprintf(stderr, "epoll_create1: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	ev->events = EPOLLIN;
	ev->data.fd = this->_listen_sock;
	if (epoll_ctl(this->_epoll_fd, EPOLL_CTL_ADD, this->_listen_sock, ev) == -1) {
		fprintf(stderr, "epoll_ctl: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
}
void 
Server::handle_cgi_write(int write_fd) {
	// int &client_fd = _cgi_client[write_fd];
	std::cout << "cgi write_fd:" << write_fd << std::endl;

	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, write_fd, NULL);
	_cgi_client.erase(write_fd);
}
void
Server::handle_cgi_read(int read_fd) {
	int &client_fd = _cgi_client[read_fd];
	
	std::cout << "cgi read_fd:" << read_fd << std::endl;
	char buf[4096];
	ssize_t bytes_read = read(read_fd, buf, 4096);
	std::cout << "bytes read: " << bytes_read << std::endl;
	if (bytes_read == -1) {
		std::cout << "pupupu" << std::endl;
	}
	else {
		ssize_t total_sent;

		total_sent = 0;
		std::cout << "client fd: " << client_fd << std::endl;
		while (total_sent < bytes_read) {
			ssize_t sent = send(client_fd, buf + total_sent, bytes_read - total_sent, 0);
			if (sent == -1) {
				throw std::runtime_error("Error sending response");
			}
			total_sent += sent;
		}
		std::cout << "bytes sent: " << total_sent << std::endl;

		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, read_fd, NULL);
		_cgi_client.erase(read_fd);
		// disconnect_client(client_fd);
	}
}
void	Server::run_event_loop(epoll_event *ev) {
	int                 	conn_sock, nfds;
	struct epoll_event		events[MAX_EVENTS];
	
    std::cerr << "cerr into logs." << std::endl;

	for (;;) {
		nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);
		if (nfds == -1) {
			fprintf(stderr, "epoll_wait: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < nfds; ++i) {
			if (events[i].data.fd == this->_listen_sock) {
				struct	sockaddr_storage	client_addr;
				socklen_t	clientaddr_len = sizeof(client_addr);

				conn_sock = accept(this->_listen_sock, reinterpret_cast<sockaddr*>(&client_addr), &clientaddr_len);
				if (conn_sock == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK)
						continue;
					else {
						fprintf(stderr, "accept: %s\n", strerror(errno));
						continue;
					}

				}
				if (fcntl(conn_sock, F_SETFL, O_NONBLOCK) == -1) {
					fprintf(stderr, "fcntl: %s\n", strerror(errno));
					close(conn_sock);
					continue;
				}
				ev->events = EPOLLIN;
				ev->data.fd = conn_sock;
				if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, conn_sock, ev) == -1) {
					fprintf(stderr, "epoll_ctl (conn_sock): %s\n", strerror(errno));
					close(conn_sock);
				}
				else {
					std::pair<std::string, std::string> ipPort = getClientAddr(client_addr);
					_clients[conn_sock] = Client(ipPort.first, ipPort.second, conn_sock);
					std::cout << "New connection on fd " << conn_sock << std::endl;
				}
			}
			else if (_cgi_client[events[i].data.fd]) {
				if (events[i].events & EPOLLIN)
					handle_cgi_read(events[i].data.fd);
				if (events[i].events & EPOLLOUT)
					handle_cgi_write(events[i].data.fd);
				// clear?
			}
			else {
				handle_client_event(events[i].data.fd);
			}
		}
	}
}

bool
Server::epoll_add_cgi(std::pair<int,int> cgi_fds, int client_fd) {
	epoll_event	ev;

	ev.events = EPOLLIN;
	ev.data.fd = cgi_fds.first;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, cgi_fds.first, &ev) == -1) {
		fprintf(stderr, "epoll_ctl (cgi read sock): %s\n", strerror(errno));
		close(cgi_fds.first);
		return false;
	}

	ev.events = EPOLLOUT;
	ev.data.fd = cgi_fds.second;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, cgi_fds.second, &ev) == -1) {
		fprintf(stderr, "epoll_ctl (cgi write sock): %s\n", strerror(errno));
		close(cgi_fds.second);
		return false;
	}
	
	_cgi_client[cgi_fds.first] = client_fd;
	_cgi_client[cgi_fds.second] = client_fd;

	return true;
}

Server::Server() : _listen_sock(-1), _epoll_fd(-1) {}

Server::~Server() {
	if (this->_listen_sock != -1)
		close(this->_listen_sock);
	if (this->_epoll_fd != -1)
		close(this->_epoll_fd);
	// std::map<int, Client>::iterator it = _clients.begin();
	// for (;it != _clients.end(); ++it) {
	// 	delete &it->second;
	// }
}

Server::Server(const Server &other) { *this = other; }

Server &Server::operator=(const Server &other) {
	if (this != &other) {
		this->_listen_sock = other._listen_sock;
		this->_epoll_fd = other._epoll_fd;
		this->_clients = other._clients;
		this->_config = other._config;
		this->_ConfigParser = other._ConfigParser;
		this->_handler = other._handler;
	}
	return *this;
}

const std::string &
Server::port() const {
	return _port;
}

std::pair<std::string, std::string>
Server::getClientAddr(struct sockaddr_storage &client_addr) {
	sockaddr								*sock_addr = reinterpret_cast<sockaddr *>(&client_addr);
	std::pair<std::string, std::string>		ipPort;
	char									ip_cstr[INET6_ADDRSTRLEN];

	if (sock_addr->sa_family == AF_INET) {
		sockaddr_in *sa_in = reinterpret_cast<sockaddr_in*>(sock_addr);
		if (inet_ntop(AF_INET, &sa_in->sin_addr, ip_cstr, sizeof(ip_cstr)) == NULL) {
			std::cerr << "inet_ntop ipv4" << "\n";
		}
		ipPort.second = std::to_string(ntohs(sa_in->sin_port));
	}
	else {
		sockaddr_in6 *sa_in6 = reinterpret_cast<sockaddr_in6*>(sock_addr);
		if (inet_ntop(AF_INET6, &sa_in6->sin6_addr, ip_cstr, sizeof(ip_cstr)) == NULL) {
			std::cerr << "inet_ntop ipv6" << "\n";
		}
		ipPort.second = std::to_string(ntohs(sa_in6->sin6_port));
	}
	ipPort.first = ip_cstr;

	return ipPort;
}

void
Server::handle_client_event(int client_fd) {
	try {
		Client &client = _clients.at(client_fd);

		client.processNewData(this);
		
		if (client.ready()) {
			_handler.handle(_config, client.req(), client_fd, client.cgi_state());
			const CgiInfo &cgi = client.cgi_state();
			if (cgi.isCgi()) {
				epoll_add_cgi(std::make_pair(cgi.getReadfd(), cgi.getWritefd()), client_fd);
			}
			else
				client.reset(); // dunno when to reset yet TODO
		}
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		std::cout << "client cyka" << std::endl;
	}
}
