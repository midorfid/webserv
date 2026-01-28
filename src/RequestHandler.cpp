#include "RequestHandler.hpp"

#include <sys/stat.h>
#include <ctime>
#include <cstdlib>
// #include <limits.bool	h(const Config &serv_cfg, const std::string &method, const std::string &client_ip) {}
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
#include "StringUtils.hpp"

#define SBUF 4096

/*ERROR TEMPLATE*/
// append static http version and error msg
// calculate time
// static webserv/version
// content-length size/len of the file
// content-type get filetype and charset from the file?
// connection close/redirect/?

std::string		RequestHandler::getHttpDate() const{
	time_t now = time(NULL);

	struct tm *gmt_time = gmtime(&now);

	char buffer[128];
	// Thu, 09 Oct 2025 09:01:45 GMT
	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt_time);

	return std::string(buffer);
}

const std::string	RequestHandler::getStatusText(int code) const{
	switch (code) {
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 500: return "Internal Server Error";
	case 200: return "OK";
	case 400: return "Bad Request";
	case 405: return "Method Not Allowed";
	case 408: return "Request Timeout";
	case 413: return "Payload Too Large";
	case 414: return "URl Too Long";
	default: return "Error";
	}
}

const std::string RequestHandler::createSuccResponseHeaders(long int contentLen) const{
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

void	RequestHandler::sendString(int client_fd, const std::string &response) const{
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

void	RequestHandler::streamFileBody(int client_fd, const std::string &file_path) const{
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

void			RequestHandler::sendDefaultError(int status_code, int client_fd) const{
	const std::string &errorText = getStatusText(status_code);
	const std::string &codeAndText = std::to_string(status_code) + " " + errorText;
	const std::string body = "<!DOCTYPE html><html><body><h1>" + codeAndText + "</h1></body></html>";

	std::stringstream		response;

	response << "HTTP/1.1 " + codeAndText + "\r\n";
	response << "Date: " << getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	response << "Content-Type: text/html\r\n";
	response << "Content-Length: " << body.length() << "\r\n";
	response << "Connection: close\r\n";
	response << "\r\n";
	response << body;

	sendString(client_fd, response.str());
}

void			RequestHandler::sendFile(const ResolvedAction &action, int client_fd) const{
	long int file_size = action.st.st_size;
	
	const std::string response = createSuccResponseHeaders(file_size);
	sendString(client_fd, response);
	streamFileBody(client_fd, action.target_path);
}

std::string		RequestHandler::createDirListHtml(const std::string &physical_path, const std::string &logic_path) const{
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

void			RequestHandler::sendDir(const std::string &phys_path, int client_fd, const std::string &logic_path) const{
	const std::string html_body = createDirListHtml(phys_path, logic_path);

	sendString(client_fd, createSuccResponseHeaders(html_body.length()));
	sendString(client_fd, html_body);
}

void handlePost(const Config &serv_cfg, const HttpRequest &req, int client_fd) {
	//1) try to post
	
	//2) send response
}

void RequestHandler::redirect(int client_fd, const std::string &new_path) const{
	std::stringstream		response;

	response << "HTTP/1.1 301 Moved Permanently\r\n";
	response << "Date: " << getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	response << "Content-Type: " << new_path << "\r\n";
	response << "Content-Length: 0" << "\r\n";
	response << "Connection: keep-alive\r\n";
	response << "\r\n";

	sendString(client_fd, response.str());
}

void RequestHandler::handle(const Config &serv_cfg, const HttpRequest &req, int client_fd, CgiInfo &state, const ResolvedAction &action) const{
	if (req.getMethod() == "GET" || req.getMethod() == "POST") { // post?
		switch (action.type) {
			case ACTION_SERVE_FILE:
				return sendFile(action, client_fd);
			case ACTION_GENERATE_ERROR:
				return sendDefaultError(action.status_code, client_fd);
			case ACTION_AUTOINDEX:
				return sendDir(action.target_path, client_fd, req.getPath());
			case ACTION_CGI:
				return state.addFds(action.cgi_fds.first, action.cgi_fds.second); // return 200 status code?
			case ACTION_REDIRECT:
				return redirect(client_fd, action.target_path);
			default:
				return sendDefaultError(500, client_fd);
		}
	}
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
