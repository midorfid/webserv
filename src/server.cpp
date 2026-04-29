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
#include <cerrno>
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
#define MAX_EVENTS 64
#define CGI_TIMEOUT 30  // seconds before a hanging CGI script is killed

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
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
		logTime(ERRLOG, std::string("epoll_ctl (DEL): ") + strerror(errno));
	logTime(REGLOG, "Client on fd " + std::to_string(client_fd) + " disconnected.");
	_clients.erase(client_fd);
}

std::map<int, std::unique_ptr<Client>>::iterator Server::disconnect_client(std::map<int, std::unique_ptr<Client>>::iterator &it, int client_fd) {
	disconnectCgiFds(*it->second);
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) == -1)
		logTime(ERRLOG, std::string("epoll_ctl (DEL): ") + strerror(errno));
	logTime(REGLOG, "Client on fd " + std::to_string(client_fd) + " disconnected.");
	return	_clients.erase(it);
}

void Server::run(const std::string &cfg_file) {
	struct epoll_event ev;

	_vhosts = _ConfigParser.parse(cfg_file);
	if (_vhosts.empty()) {
		logTime(ERRLOG, "Error: No server blocks in config.");
		exit(EXIT_FAILURE);
	}

	// Build port → vhost index map; bind each unique port exactly once
	for (size_t i = 0; i < _vhosts.size(); ++i) {
		std::string port_str;
		if (!_vhosts[i].getPort("", port_str)) {
			logTime(ERRLOG, "Error: server block " + std::to_string(i) + " has no listen port.");
			exit(EXIT_FAILURE);
		}
		int port = std::stoi(port_str);
		_port_to_vhost_idx[port].push_back(i);
	}

	for (auto &[port, indices] : _port_to_vhost_idx) {
		(void)indices;
		int fd = bind_port(port);
		if (listen(fd, LISTEN_BACKLOG) == -1) {
			logTime(ERRLOG, std::string("listen: ") + strerror(errno));
			exit(EXIT_FAILURE);
		}
		_sock_to_port[fd] = port;
		logTime(REGLOG, "Listening on port " + std::to_string(port) + " (fd " + std::to_string(fd) + ")");
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
		logTime(ERRLOG, std::string("getaddrinfo: ") + gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	optval_int = 1;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (listen_fd == -1)
			continue;

		if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval_int, sizeof(int)) == -1) {
			logTime(ERRLOG, std::string("setsockopt: ") + strerror(errno));
			exit(EXIT_FAILURE);
		}

		if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(listen_fd);
		listen_fd = -1;
	}
	freeaddrinfo(result);

	if (listen_fd == -1) {
		logTime(ERRLOG, "Could not bind port " + std::to_string(port));
		exit(EXIT_FAILURE);
	}
	return listen_fd;
}

void	Server::init_epoll(epoll_event *ev) {
	this->_epoll_fd = epoll_create1(0);
	if (this->_epoll_fd == -1) {
		logTime(ERRLOG, std::string("epoll_create1: ") + strerror(errno));
		exit(EXIT_FAILURE);
	}

	ev->events = EPOLLIN;
	for (auto &[fd, port] : _sock_to_port) {
		(void)port;
		ev->data.fd = fd;
		if (epoll_ctl(this->_epoll_fd, EPOLL_CTL_ADD, fd, ev) == -1) {
			logTime(ERRLOG, "epoll_ctl (listen fd " + std::to_string(fd) + "): " + strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	ev->data.fd = this->_signal_read_fd;
	if (epoll_ctl(this->_epoll_fd, EPOLL_CTL_ADD, this->_signal_read_fd, ev) == -1) {
		logTime(ERRLOG, std::string("epoll_ctl: ") + strerror(errno));
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

static void close_cgi_write_fd(int epoll_fd, std::map<int, int> &cgi_client, CgiInfo &cgi, int write_fd) {
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, write_fd, NULL);
	cgi_client.erase(write_fd);
	close(write_fd);
	cgi.write_fd = -1;
}

void
Server::handle_cgi_write(int write_fd) {
	auto cgi_it = _cgi_client.find(write_fd);
	if (cgi_it == _cgi_client.end()) return;
	int client_fd = cgi_it->second;

	auto client_it = _clients.find(client_fd);
	if (client_it == _clients.end()) {
		// Client gone — clean up orphaned write pipe
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, write_fd, NULL);
		_cgi_client.erase(write_fd);
		close(write_fd);
		return;
	}
	Client &client = *client_it->second;
	const std::string &body = client.req().getBody();

	size_t remaining = body.length() - static_cast<size_t>(client.bytes_written_to_cgi);

	// No body to send (GET, HEAD, or fully written): signal EOF to the script
	if (remaining == 0) {
		close_cgi_write_fd(_epoll_fd, _cgi_client, client.cgi_state(), write_fd);
		return;
	}

	ssize_t sent = write(write_fd, body.c_str() + client.bytes_written_to_cgi, remaining);
	if (sent > 0) {
		client.bytes_written_to_cgi += sent;
		if (static_cast<size_t>(client.bytes_written_to_cgi) >= body.length())
			close_cgi_write_fd(_epoll_fd, _cgi_client, client.cgi_state(), write_fd);
	}
	else if (sent == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return; // pipe full, wait for next EPOLLOUT
		disconnectCgiFds(client);
		terminateConnWithError(client_fd, 500);
	}
}

// Parse CGI response headers from out_buf[0..head_end] into resp.
// Returns true if CGI set Content-Length.
static bool parseCgiHeaders(const std::string &out_buf, size_t head_end, ResponseState &resp) {
	std::string_view headers_view(out_buf.data(), head_end);
	bool has_content_length = false;
	size_t offset = 0;

	while (offset < head_end) {
		size_t end_line = headers_view.find("\r\n", offset);
		if (end_line == std::string_view::npos)
			end_line = head_end;

		std::string_view line = headers_view.substr(offset, end_line - offset);
		size_t colon = line.find(':');

		if (colon != std::string_view::npos) {
			std::string key(line.substr(0, colon));
			std::string val(line.substr(colon + 1));
			StringUtils::trimWhitespaces(key);
			StringUtils::trimWhitespaces(val);
			for (auto &c : key) c = std::tolower(static_cast<unsigned char>(c));

			if (key == "status") {
				try { resp.status_code = std::stoi(val); } catch (...) {}
			} else {
				if (key == "content-length") has_content_length = true;
				resp.addHeader(key, val);
			}
		}
		offset = (end_line >= head_end) ? head_end : end_line + 2;
	}
	return has_content_length;
}

// Called when headers boundary found mid-stream AND CGI gave no Content-Length.
// Sends HTTP headers with Transfer-Encoding: chunked, queues any body received so far
// as the first chunk, and arms the client fd for writing.
void
Server::startChunkedCgiStream(Client &client, int client_fd) {
	std::string &out_buf = client.cgi_state().output_buf;
	size_t head_end = out_buf.find("\r\n\r\n");

	ResponseState resp(200);
	parseCgiHeaders(out_buf, head_end, resp);

	std::string body_so_far = out_buf.substr(head_end + 4);
	out_buf.clear();

	Response::finalizeResponseChunked(resp, client.req().getPath(), client.isKeepAliveConn(), client.session_cookie);
	std::string response = Response::build(resp);
	if (!body_so_far.empty())
		response += Response::encodeChunk(body_so_far);
	client.queueResponse(response);

	client.cgi_state().headers_sent = true;
	client.cgi_state().is_chunked   = true;

	struct epoll_event ev;
	ev.events  = EPOLLOUT;
	ev.data.fd = client_fd;
	epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
}

// Called at pipe EOF for the two buffered cases:
//   1. Headers never found mid-stream (!headers_sent) — full output in out_buf.
//   2. Content-Length mode (headers_sent && !is_chunked) — full output in out_buf.
void
Server::parseAndQoutputBuf(Client &client, int client_fd) {
	std::string &out_buf = client.cgi_state().output_buf;

	size_t head_end = out_buf.find("\r\n\r\n");
	if (head_end == std::string::npos) {
		terminateConnWithError(client_fd, 502);
		return;
	}

	ResponseState resp(200);
	bool has_content_length = parseCgiHeaders(out_buf, head_end, resp);

	std::string body = out_buf.substr(head_end + 4);
	out_buf.clear();

	if (has_content_length) {
		Response::finalizeResponse(resp, client.req().getPath(), body.length(), client.isKeepAliveConn(), client.session_cookie);
		client.queueResponse(Response::build(resp) + body);
	} else {
		Response::finalizeResponseChunked(resp, client.req().getPath(), client.isKeepAliveConn(), client.session_cookie);
		client.queueResponse(Response::build(resp) + Response::encodeChunked(body));
	}

	struct epoll_event ev;
	ev.events  = EPOLLIN | EPOLLOUT;
	ev.data.fd = client_fd;
	epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
}

void
Server::handle_cgi_read(int read_fd) {
	auto cgi_it = _cgi_client.find(read_fd);
	if (cgi_it == _cgi_client.end()) return;
	int client_fd = cgi_it->second;

	auto client_it = _clients.find(client_fd);
	if (client_it == _clients.end()) {
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, read_fd, NULL);
		_cgi_client.erase(read_fd);
		close(read_fd);
		return;
	}
	Client  &client = *client_it->second;
	CgiInfo &cgi    = client.cgi_state();

	char buf[4096];
	bool need_client_wake = false;

	// Drain the pipe in one shot rather than relying on repeated epoll wakeups.
	// Each iteration reads up to 4 KiB; the loop exits on EAGAIN (no more data
	// right now), EOF (child exited), or a hard read error.
	while (true) {
		ssize_t n = read(read_fd, buf, sizeof(buf));

		if (n > 0) {
			if (!cgi.headers_sent) {
				cgi.output_buf.append(buf, n);
				size_t head_end = cgi.output_buf.find("\r\n\r\n");
				if (head_end == std::string::npos)
					continue; // header block not yet complete — read more

				ResponseState probe(200);
				bool has_cl = parseCgiHeaders(cgi.output_buf, head_end, probe);
				if (has_cl) {
					cgi.headers_sent = true;
					cgi.is_chunked   = false;
				} else {
					startChunkedCgiStream(client, client_fd);
					// sets headers_sent=true, is_chunked=true; already arms EPOLLOUT
				}
			} else if (cgi.is_chunked) {
				client.queueResponse(Response::encodeChunk(std::string(buf, n)));
				need_client_wake = true;
			} else {
				cgi.output_buf.append(buf, n);
			}
		}
		else if (n == 0) {
			// Pipe EOF — child closed its stdout
			epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, read_fd, NULL);
			_cgi_client.erase(read_fd);
			close(read_fd);
			cgi.read_fd = -1;

			pid_t &pid = cgi.child_pid;
			if (pid != -1) {
				pid_t reaped = waitpid(pid, NULL, WNOHANG);
				if (reaped > 0 || reaped == -1)
					pid = -1;
			}

			if (cgi.is_chunked) {
				client.queueResponse("0\r\n\r\n");
				need_client_wake = true;
			} else {
				parseAndQoutputBuf(client, client_fd); // arms client fd internally
			}
			break;
		}
		else {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break; // pipe empty for now — epoll will wake us when there's more
			disconnectCgiFds(client);
			terminateConnWithError(client_fd, 500);
			return; // client state is gone; skip the wake-up below
		}
	}

	if (need_client_wake) {
		struct epoll_event ev;
		ev.events  = EPOLLOUT;
		ev.data.fd = client_fd;
		epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
	}
}

double
Server::diffTime(const time_t &client_tm) {
	time_t now = time(NULL);
	
	return static_cast<double>(now - client_tm);
}

void	Server::checkTimeouts() {
	_sessions.purgeExpired();
	for (auto it = _clients.begin(); it != _clients.end();) {
		Client &client   = *it->second;
		int     client_fd = it->first;

		// Kill CGI scripts that exceed the hard timeout; reply 504 and keep
		// the connection alive long enough to flush the error response.
		if (client.cgi_state().isCgi() &&
		    diffTime(client.cgi_state().cgi_start) > CGI_TIMEOUT) {
			disconnectCgiFds(client);
			terminateConnWithError(client_fd, 504);
			++it;
			continue;
		}

		double quiet_time = diffTime(client.getLastActivity());
		bool disconnected = false;

		if (client.getClientState() != IDLE && quiet_time > 60) {
			_handler.sendDefaultError(408, client);
			it = disconnect_client(it, client_fd);
			disconnected = true;
		}
		if (!disconnected) {
			if (quiet_time > static_cast<double>(_vhosts.front().getKeepAliveTimer()))
				it = disconnect_client(it, client_fd);
			else
				++it;
		}
		if (_clients.empty())
			break;
	}
}

void
Server::cleanup() {
	logTime(REGLOG, "Shutting down.");
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
	signal(SIGCHLD, SIG_IGN); // auto-reap children; prevents zombies if waitpid(WNOHANG) races

	for (;;) {
		nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, 1000);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			logTime(ERRLOG, std::string("epoll_wait: ") + strerror(errno));
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
						logTime(ERRLOG, std::string("accept: ") + strerror(errno));
						continue;
					}
				}
				if (fcntl(conn_sock, F_SETFL, O_NONBLOCK) == -1) {
					logTime(ERRLOG, std::string("fcntl: ") + strerror(errno));
					close(conn_sock);
					continue;
				}
				ev->events = EPOLLIN | EPOLLRDHUP;
				ev->data.fd = conn_sock;
				if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, conn_sock, ev) == -1) {
					logTime(ERRLOG, std::string("epoll_ctl (conn_sock): ") + strerror(errno));
					close(conn_sock);
				}
				else {
					std::pair<std::string, std::string> ipPort = getClientAddr(client_addr);
					_clients[conn_sock] = std::make_unique<Client>(ipPort.first, ipPort.second, conn_sock);
					_clients[conn_sock]->setServerPort(server_port);
					logTime(REGLOG, "New connection on fd " + std::to_string(conn_sock) + " (port " + std::to_string(server_port) + ")");
				}
			}
			else if (_cgi_client.find(curr_fd) != _cgi_client.end()) {
				if (events[i].events & EPOLLIN) {
					handle_cgi_read(curr_fd);
				} else if (events[i].events & EPOLLHUP) {
					// EPOLLHUP without EPOLLIN: two sub-cases:
					//   read_fd: pipe empty and write-end closed → drain (bytes_read==0 = EOF)
					//   write_fd: child closed its stdin → close our write end
					int cgi_client_fd = _cgi_client.count(curr_fd) ? _cgi_client[curr_fd] : -1;
					if (cgi_client_fd != -1 && _clients.count(cgi_client_fd)) {
						CgiInfo &cgi = _clients[cgi_client_fd]->cgi_state();
						if (cgi.read_fd == curr_fd)
							handle_cgi_read(curr_fd);
						else if (cgi.write_fd == curr_fd)
							handle_cgi_write(curr_fd);
					}
				}
				if (events[i].events & EPOLLOUT)
					handle_cgi_write(curr_fd);
			}
			else if (_clients.find(curr_fd) != _clients.end()) {
				uint32_t mask = events[i].events;
				if (mask & (EPOLLERR | EPOLLHUP)) {
					disconnect_client(curr_fd);
				} else if (mask & EPOLLIN) {
					// Handles EPOLLRDHUP+EPOLLIN too: recv()→0 → NothingToRead → disconnect
					handle_client_read(curr_fd);
				} else if (mask & EPOLLOUT) {
					handle_client_write(curr_fd);
				} else if (mask & EPOLLRDHUP) {
					disconnect_client(curr_fd);
				}
			}
			else {
				logTime(ERRLOG, "Received event on unknown fd.");
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
		logTime(ERRLOG, std::string("epoll_ctl (cgi read sock): ") + strerror(errno));
		close(cgi_fds.first);
		return false;
	}

	ev.events = EPOLLOUT;
	ev.data.fd = cgi_fds.second;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, cgi_fds.second, &ev) == -1) {
		logTime(ERRLOG, std::string("epoll_ctl (cgi write sock): ") + strerror(errno));
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
		if (inet_ntop(AF_INET, &sa_in->sin_addr, ip_cstr, sizeof(ip_cstr)) == NULL)
			logTime(ERRLOG, "inet_ntop ipv4");
		ipPort.second = std::to_string(ntohs(sa_in->sin_port));
	}
	else {
		sockaddr_in6 *sa_in6 = reinterpret_cast<sockaddr_in6*>(sock_addr);
		if (inet_ntop(AF_INET6, &sa_in6->sin6_addr, ip_cstr, sizeof(ip_cstr)) == NULL)
			logTime(ERRLOG, "inet_ntop ipv6");
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

	bool is_new = false;
	std::string sid = _sessions.getOrCreate(client.req().getCookie("session_id"), is_new);
	if (is_new)
		client.session_cookie = "session_id=" + sid
			+ "; HttpOnly; SameSite=Lax; Path=/; Max-Age="
			+ std::to_string(SessionStore::TTL);

	_handler.handle(client.req(), client, client.cgi_state(), action);

	client.updateLastActivity();

	const CgiInfo &cgi = client.cgi_state();

	if (cgi.isCgi()) {
		epoll_add_cgi(std::make_pair(cgi.getReadfd(), cgi.getWritefd()), client_fd);
		// Disarm the client fd while waiting for CGI output so that a reload
		// or peer-close doesn't trigger handle_client_event with an empty queue.
		// parseAndQoutputBuf re-arms it with EPOLLIN|EPOLLOUT when done.
		struct epoll_event ev = {};
		ev.data.fd = client_fd;
		epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
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
Server::handle_client_read(int client_fd) {
	Client &client = *_clients.at(client_fd);

	ParseResult status = client.processNewData(*this);
	switch (status) {
		case BadRequest:          return terminateConnWithError(client_fd, 400);
		case UrlTooLong:          return terminateConnWithError(client_fd, 414);
		case BodyTooLarge:        return terminateConnWithError(client_fd, 413);
		case HeadersTooLarge:     return terminateConnWithError(client_fd, 431);
		case UnsupportedVersion:  return terminateConnWithError(client_fd, 505);
		case RequestComplete:   return handleDefault(client, client_fd);
		case RequestIncomplete: return;
		case Error:
		case NothingToRead:     return disconnect_client(client_fd);
		default:                return;
	}
}

void
Server::handle_client_write(int client_fd) {
	try {
		Client &client = *_clients.at(client_fd);

		bool done = client.writeResponseChunk();
		if (!done)
			return;

		if (client.cgi_state().isCgi()) {
			// Queue drained but CGI pipe still open: disarm and wait for the
			// next chunk. handle_cgi_read re-arms with EPOLLOUT when it queues one.
			struct epoll_event ev = {};
			ev.data.fd = client_fd;
			epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
			return;
		}
		if (client.isKeepAliveConn()) {
			client.reset();
			struct epoll_event ev;
			ev.events = EPOLLIN | EPOLLRDHUP;
			ev.data.fd = client_fd;
			epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, client_fd, &ev);
		} else {
			disconnect_client(client_fd);
		}
	}
	catch (const std::exception &e) {
		logTime(ERRLOG, e.what());
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
