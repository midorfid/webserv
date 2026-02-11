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

void			RequestHandler::sendFile(const ResolvedAction &action, int client_fd) const{
	long int file_size = action.st.st_size;
	
	ResponseState res(action.status_code);
	
	res.addHeader("Content-Length", std::to_string(file_size));
	
	Response::finalizeResponse(res, action.target_path);

	const std::string response = Response::build(res);
	sendString(client_fd, response);
	streamFileBody(client_fd, action.target_path);
}

int	handleOpenDirFail() {
	if (errno == ENOENT || errno == ENOTDIR)
		return 404;
	else if (errno == EACCES)
		return 403;
	else
		return 500;
}

ResponseState	RequestHandler::createDirListHtml(const std::string &physical_path, const std::string &logic_path) const{
	ResponseState	res(200);

	DIR *directory = opendir(physical_path.c_str());
	if (!directory) {
		int status = handleOpenDirFail();
		res.status_code = status;
		res.body = generatePage(status, Response::getStatusText(status));
	}
	else {
		res.body = "<!DOCTYPE html>\r\n"
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
			res.body += "<li><a href=\"" + href_link + "\">" + name + "</a></li>\r\n";
		}
		res.body += "	</ul>\r\n"
					 "</body>\r\n"
					 "</html>\r\n";
		closedir(directory);
	}

	return res;
}

std::string		RequestHandler::generatePage(int error_code, const std::string &text, const std::string &details) const{
	std::stringstream ss;

	ss << "<!DOCTYPE html>\r\n";
	ss << "<html lang=en>\r\n";
	ss << "<head>\r\n";
	ss << "<meta charset=\"UTF-8\">\r\n";
	ss << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\r\n";
	ss << "<title>" << error_code << " " << text << "</title>\r\n";
	ss << "</head>\r\n";
	ss << "<body>\r\n";
	ss << "<center><h1>" << error_code << " " << text << "</h1></center>\r\n";
	if (!details.empty())
		ss << "<center>" << details << "</center>\r\n";
	ss << "<hr><center>webserv/1.0</center>\r\n";
	ss << "</body></html>";

	return ss.str();
}

void			RequestHandler::sendDir(const std::string &phys_path, int client_fd, const std::string &logic_path) const{
	ResponseState state = createDirListHtml(phys_path, logic_path);
	
	Response::finalizeResponse(state, logic_path);
	
	const std::string resp = Response::build(state);
	sendString(client_fd, resp);
}

int RequestHandler::checkFileCreation(const std::string &url_path, const HttpRequest &req) {
	struct stat info;
	
	size_t par_dir_pos = url_path.find_last_of('/');

	std::string parent_dir = (par_dir_pos == std::string::npos) ? "./" : url_path.substr(par_dir_pos);

	if (stat(parent_dir.c_str(), &info) != 0)
		return 404;
	if (!S_ISDIR(info.st_mode))
		return 409; // is not a dir
	if (!(info.st_mode & S_IWUSR))
		return 403; // no write permision

	if (stat(url_path.c_str(), &info) == 0) {
		if (S_ISDIR(info.st_mode))
			return 409;
		
		return 204; //(overwrite)
	}
	std::ofstream outfile(url_path, std::ios::binary | std::ios::trunc);
	outfile.write(req.getBody().c_str(), req.getBody().size());
	outfile.close();
	if (!outfile.is_open())
		return 500;
	return 201; //new file
}

std::string RequestHandler::checkFileExtension(const HttpRequest &req) {
	size_t extension_pos = req.getPath().rfind('.');

	if (extension_pos != std::string::npos)
		return req.getPath();

	std::string con_type_exten = req.getHeader("content-type");

	if (con_type_exten != "")
		return req.getPath() + con_type_exten;

	return req.getPath() + ".bin";
}

void 
RequestHandler::putBinary(const HttpRequest &req, int client_fd) {
	const std::string &url_path = checkFileExtension(req);
	int status_code = checkFileCreation(url_path, req);
	
	ResponseState resp(status_code);

	if (status_code >= 400) {
		resp.body = generatePage(status_code, Response::getStatusText(status_code));
	}
	else {
		resp.body = "";
	}

	Response::finalizeResponse(resp, req.getPath());

	sendString(client_fd, Response::build(resp));
}

void RequestHandler::handlePut(const Config &serv_cfg, const HttpRequest &req, int client_fd) {
	ResponseState status;
	
	std::string contentType = req.getHeader("content-type");
	if (contentType != "" && contentType.find("Multipart") == contentType.npos) {
		putBinary(req, client_fd);
	}
	// TODO create response struct as well as response builder to keep DRY
	//2) send response
	(void)serv_cfg;
}

void RequestHandler::handle(const HttpRequest &req, int client_fd, CgiInfo &state, const ResolvedAction &action) const{
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

RequestHandler::RequestHandler() {}

RequestHandler::~RequestHandler() {}

RequestHandler::RequestHandler(const RequestHandler &other) {
	*this = other;
}

RequestHandler &RequestHandler::operator=(const RequestHandler &other) {
	(void)other;
	return *this;
}
