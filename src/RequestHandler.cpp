#include "RequestHandler.hpp"

#include <sys/stat.h>
#include <ctime>
#include <cstdlib>
// #include <limits.h>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <sys/socket.h>
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "server.hpp"
#include "Environment.hpp"

#define SBUF 4096

/*ERROR TEMPLATE*/
// append static http version and error msg
// calculate time
// static webserv/version
// content-length size/len of the file
// content-type get filetype and charset from the file?
// connection close/redirect/?

std::string		RequestHandler::getHttpDate() {
	time_t now = time(NULL);

	struct tm *gmt_time = gmtime(&now);

	char buffer[128];
	// Thu, 09 Oct 2025 09:01:45 GMT
	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt_time);

	return std::string(buffer);
}

const std::string	RequestHandler::create_404_response() {
	const std::string		body = "<!DOCTYPE html><html><body><h1>404 Not Found</h1></body></html>";

	std::stringstream		response;

	response << "HTTP/1.1 404 not Found\r\n";
	response << "Date: " << getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	return response.str();
}

const std::string	RequestHandler::create_403_response() {
	const std::string		body = "<!DOCTYPE html><html><body><h1>403 Forbidden</h1></body></html>";
	
	std::stringstream		response;

	response << "HTTP/1.1 403 Access Forbidden\r\n";
	response << "Date: " << getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	return response.str();
}

const std::string	RequestHandler::create_500_response() {
	const std::string		body = "<!DOCTYPE html><html><body><h1>500 Internal Server Error</h1></body></html>";
	
	std::stringstream		response;

	response << "HTTP/1.1 500 Internal Server Error\r\n";
	response << "Date: " << getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	return response.str();
}

const std::string RequestHandler::generic_error_response() {
	const std::string body = "<!DOCTYPE html><html><body><h1>Error</h1></body></html>";
		
	std::stringstream		response;

	response << "HTTP/1.1 Error\r\n";
	response << "Date: " << getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	return response.str();
}

const std::string	RequestHandler::getDefaultError(int status_code) {
	switch(status_code) {
		case 404:
			return create_404_response();
		case 403:
			return create_403_response();
		case 500:
			return create_500_response();
		default:
			return generic_error_response();
	}
}

const Location	*RequestHandler::findBestLocationMatch(const Config &serv_cfg, const std::string &url) {
	const Location	*best_match = NULL;
	size_t			longest_len = 0;
	
	const std::vector<Location>	&locations = serv_cfg.getLocations();
	for (size_t i = 0; i < locations.size(); ++i) {
		const std::string &loc_path = locations[i].getPath();
		if (url.find(loc_path, 0) != url.npos) {
			if (loc_path.length() > longest_len) {
				longest_len = loc_path.length();
				best_match = &locations[i];
			}
		}
	}
	return best_match;
}

bool RequestHandler::normalizePath(std::string &phys_path) {
	char actual_path[PATH_MAX];
	
	std::cout << "input_path:" << phys_path << "q" << std::endl;
	if (realpath(phys_path.c_str(), actual_path) == NULL) {
		std::cout << "errno: " << strerror(errno) << std::endl;
		return false;
	}
	phys_path.erase(phys_path.length()-1); // double "//" change later TODO
	std::cout << "normalized path:" << phys_path << "q" << std::endl;
	// handle if request is outside of var/www/ directory TODO
	phys_path = actual_path;
	return true;
}

const std::string RequestHandler::createSuccResponseHeaders(long int contentLen) {
	std::stringstream		response;

	response << "HTTP/1.1 200 Success\r\n";
	response << "Date: " << getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << contentLen << "\r\n";
	response << "Connection: keep-alive\r\n";
	response << "\r\n";

	return response.str();
}

void	RequestHandler::sendString(int client_fd, const std::string &response) {
	ssize_t total_sent;
	ssize_t to_send;

	total_sent = 0;
	to_send = response.size();
	while (total_sent < to_send) {
		ssize_t sent = send(client_fd, response.c_str() + total_sent, to_send - total_sent, 0);
		if (sent == -1) {
			throw std::runtime_error("Error sending response");
		}
		total_sent += sent;
	}
}

void	RequestHandler::streamFileBody(int client_fd, const std::string &file_path) {
	std::ifstream	file(file_path.c_str(), std::ios::binary);

	char	buf[SBUF];
	while (file.good()) {
		file.read(buf, sizeof(buf));

		std::streamsize	bytes_to_send = file.gcount();
		if (bytes_to_send > 0) {
			ssize_t sent = send(client_fd, buf, bytes_to_send, 0);
			if (sent == -1) {
				throw std::runtime_error("Error sending response file");
			}
		}
	}
}

void			RequestHandler::sendDefaultError(int status_code, int client_fd) {
	const std::string error_str = getDefaultError(status_code);
	sendString(client_fd, error_str);
}

void			RequestHandler::sendFile(const ResolvedAction &action, int client_fd) {
	long int file_size = action.st.st_size;
	
	const std::string response = createSuccResponseHeaders(file_size);
	sendString(client_fd, response);
	streamFileBody(client_fd, action.target_path);
}

std::string		RequestHandler::createDirListHtml(const std::string &physical_path, const std::string &logic_path) {
	std::string html_body = "<!DOCTYPE html>\r\n"
						"<html lang=en>\r\n"
						"<head>\r\n"
						"	<meta charset=\"UTF-8\">\r\n"
						"	<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n"
						"	<title>Index of " + logic_path + "</title>\r\n"
						"</head>\r\n"
						"<body>\r\n"
						"	<h1>Index of " + logic_path + "</h1>\r\n"
						"	<hr>\r\n"
						"	<ul>\r\n";
	
	DIR *directory = opendir(physical_path.c_str());
	if (!directory)
		throw std::runtime_error("could not open dir");

	for (dirent *entry = readdir(directory); entry != NULL; entry = readdir(directory)) {
		std::string name = entry->d_name;
		if (name == ".." || name == ".")
			continue;
		std::string href_link = logic_path;
		if (href_link[href_link.length() - 1] != '/')
			href_link += "/";
		struct stat st;
		if (stat((physical_path + "/" + href_link).c_str(), &st) == 0 &&
				S_ISDIR(st.st_mode))
		{
			name += "/";
		}
		html_body += "		<li><a href=\"" + href_link + "\">" + name + "</a></li>\r\n";
	}
	html_body += "	</ul>\r\n"
				 "</body>\r\n"
				 "</html>\r\n";
	closedir(directory);
	return html_body;
}

void			RequestHandler::sendDir(const std::string &phys_path, int client_fd, const std::string &logic_path) {
	const std::string html_body = createDirListHtml(phys_path, logic_path);

	sendString(client_fd, createSuccResponseHeaders(html_body.length()));
	sendString(client_fd, html_body);
}

void RequestHandler::handle(const Config &serv_cfg, const HttpRequest &req, int client_fd, Server *server) {
	ResolvedAction	action;
	// if (req.getMethod() == "GET") {
	// 	action = resolveRequestToAction(serv_cfg, req.getPath());
	// 	switch (action.type) {
	// 		case ACTION_SERVE_FILE:
	// 			return sendFile(action, client_fd);
	// 		case ACTION_GENERATE_ERROR:
	// 			return sendDefaultError(action.status_code, client_fd);
	// 		case ACTION_AUTOINDEX:
	// 			return sendDir(action.target_path, client_fd, req.getPath());
	// 		default:
	// 			return sendDefaultError(500, client_fd);
	// 	}
	// }
	// else if (req.getMethod() == "POST" && req.getHeader("content-type") == "image/png") {
		// for now only does CGI, TODO
	{
		const Location *location = findBestLocationMatch(serv_cfg, req.getPath());
		if (location == NULL) {
			resolveErrorAction(404, serv_cfg);
		}
		std::string script_type;
		location->getDirective("allow_cgi", script_type);
		if (script_type != ".py")
			resolveErrorAction(404, serv_cfg);
		std::string root_path;
		location->getDirective("root", root_path);
		std::string scriptPhysAddr = root_path + req.getPath(); // query?? TODO
		if (normalizePath(scriptPhysAddr) == false)
			resolveErrorAction(404, serv_cfg);
		struct stat st;
		if (stat(scriptPhysAddr.c_str(), &st) != 0) {
			std::cout << "script stat() failed :(" << std::endl;
			resolveErrorAction(403, serv_cfg);
		}
		pid_t	cpid;
		int		serv_to_cgi[2];
		int		cgi_to_serv[2];

		if (pipe(serv_to_cgi) == -1 || pipe(cgi_to_serv) == -1) {
			std::cout << "pipe" << std::endl;
			resolveErrorAction(500, serv_cfg);
		}
		cpid = fork();
		if (cpid == -1) {
			std::cout << "fork" << std::endl;
			resolveErrorAction(500, serv_cfg);
		}
		if (cpid == 0) {
			close(cgi_to_serv[0]);
			close(serv_to_cgi[1]);

			if (dup2(serv_to_cgi[0], STDIN_FILENO) == -1) {
				std::cout << "dup2" << std::endl;
				resolveErrorAction(500, serv_cfg);
			}
			if (dup2(cgi_to_serv[1], STDOUT_FILENO) == -1) {
				std::cout << "dup2" << std::endl;
				resolveErrorAction(500, serv_cfg);
			}

			close(cgi_to_serv[1]);
			close(serv_to_cgi[0]);

			Environment	envp_builder(req, *server);

			envp_builder.build();

			char *intep = const_cast<char*>("/usr/bin/python3");
			char *program = const_cast<char*>("../www/script.py");

			char *const argv[] = {intep, program, NULL};
			std::cerr << "script path: " << scriptPhysAddr << std::endl;
			if (execve(program, argv, envp_builder.getEnvp()) == -1) {
				std::cerr << "execve error" << std::endl;
				std::cerr << strerror(errno) << std::endl;
			}
			exit(1);
		}
		else {
			close(serv_to_cgi[0]);
			close(cgi_to_serv[1]);
			
			if (fcntl(serv_to_cgi[1], F_SETFL, O_NONBLOCK) == -1 ||
				fcntl(cgi_to_serv[0], F_SETFL, O_NONBLOCK) == -1) {
					std::cout << "fcntl" << std::endl;
					resolveErrorAction(500, serv_cfg);
				}
			std::pair<int, int> cgi_fds = std::make_pair(cgi_to_serv[0], serv_to_cgi[1]);
			if (!server->epoll_add_cgi(cgi_fds, client_fd)) {
					std::cout << "epoll_add_cgi" << std::endl;
					resolveErrorAction(500, serv_cfg);
				}
			
			close(serv_to_cgi[1]);
			// close(cgi_to_serv[0]);
		}
	}
}

ResolvedAction	RequestHandler::resolveErrorAction(int error_code, const Config &serv_cfg) {
	std::string		error_url;

	if (serv_cfg.getErrorPage(error_code, error_url)) {
		const Location *location = findBestLocationMatch(serv_cfg, error_url);

		std::string root = location->getPath();
		std::string file_path = root + error_url;
		
		struct stat st;
		if (stat(file_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
			ResolvedAction	action;
			
			action.st = st;
			action.status_code = error_code;
			action.target_path = file_path;
			action.type = ACTION_SERVE_FILE;
			
			return action;
		}
	}
	ResolvedAction	default_action;

	default_action.status_code = error_code;
	default_action.type = ACTION_GENERATE_ERROR;
	
	return default_action;
}

ResolvedAction	RequestHandler::resolveRequestToAction(const Config &serv_cfg, const std::string &req_path) {
	std::string		phys_path;
	
	const Location *location = findBestLocationMatch(serv_cfg, req_path);
	if (location == NULL) {
		return resolveErrorAction(404, serv_cfg);
	}
	if (location->getDirective("root", phys_path) == false) {
		return resolveErrorAction(500, serv_cfg); // maybe different error
	}
	if (req_path == "/") {
		std::string index;
		if (!location->getIndex(index))
			return resolveErrorAction(404, serv_cfg);
		struct stat st;
		std::string physAndIdxPath = phys_path + index;
		if (stat(physAndIdxPath.c_str(), &st) == 0) {
			return resolveFileAction(physAndIdxPath, &st);
		} // handle if index not found, autoindex off TODO as well as multiple indexes
		return resolveErrorAction(404, serv_cfg);
	}
	if (normalizePath(phys_path) == false) {
		return resolveErrorAction(404, serv_cfg);
	}
	return checkReqPath(phys_path, serv_cfg, location);
}

ResolvedAction RequestHandler::resolveFileAction(const std::string &path, struct stat *st) {
	ResolvedAction	action;

	action.st = *st;
	action.status_code = 200;
	action.type = ACTION_SERVE_FILE;
	action.target_path = path;

	return action;
}

bool	RequestHandler::findAccessibleIndex(ResolvedAction &action, const std::string &dir_path,
											const std::vector<std::string> &indexes) {
	for (size_t i = 0; i < indexes.size(); ++i) {
		std::string full_path = dir_path + "/" + indexes[i];
		
		struct stat st;

		if (stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
			action.st = st;
			action.status_code = 200;
			action.target_path = full_path;
			action.type = ACTION_SERVE_FILE;
			return true;
		}
	}
	return false;
}

ResolvedAction RequestHandler::resolveDirAction(const std::string &dir_path, const Config &cfg,
												struct stat *st, const Location *location) {
	ResolvedAction				action;
	std::vector<std::string>	indexes;

	if (location->getIndexes(indexes) && findAccessibleIndex(action, dir_path, indexes))
		return action;
	if (cfg.getIndexes(indexes) && findAccessibleIndex(action, dir_path, indexes))
		return action;
	std::string	autoindex;
	if (location->isAutoindexOn()) {
		action.st = *st;
		action.status_code = 200;
		action.target_path = dir_path;
		action.type = ACTION_AUTOINDEX;
		return action;
	}
	return resolveErrorAction(403, cfg);
}


ResolvedAction	RequestHandler::checkReqPath(const std::string &path, const Config &cfg, const Location *location) {
	struct stat st;

	std::cout << path << std::endl;
	if (stat(path.c_str(), &st) != 0) {
		switch(errno) {
			case ENOENT:
				return resolveErrorAction(404, cfg);
			case EACCES:
				return resolveErrorAction(403, cfg);
			default:
				return resolveErrorAction(500, cfg);
		}
	}
	if (S_ISDIR(st.st_mode)) {
		return resolveDirAction(path, cfg, &st, location);
	}
	else if (S_ISREG(st.st_mode))
		return resolveFileAction(path, &st);
	//Fallback
	return resolveErrorAction(403, cfg);
}

RequestHandler::RequestHandler() {}

RequestHandler::~RequestHandler() {}

RequestHandler::RequestHandler(const RequestHandler &other) {
	*this = other;
}

RequestHandler &RequestHandler::operator=(const RequestHandler &other) {
	(void)other;
	return *this;
}
