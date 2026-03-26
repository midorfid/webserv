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

off_t
RequestHandler::calculateTargetSize(const std::string &target_path) const{
	struct stat	info;

	stat(target_path.c_str(), &info);
	return info.st_size;
}

void			RequestHandler::sendFile(const ResolvedAction &action, int client_fd) const{
	long int file_size = calculateTargetSize(action.target_path);
	
	ResponseState res(action.status_code);
	
	Response::finalizeResponse(res, action.req_path, file_size, action.keep_alive);

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

void			RequestHandler::sendDir(const ResolvedAction &action, int client_fd, const std::string &logic_path) const{
	long int dir_size = calculateTargetSize(action.target_path);

	ResponseState state = createDirListHtml(action.target_path, logic_path);
	
	Response::finalizeResponse(state, logic_path, dir_size, action.keep_alive);
	
	const std::string resp = Response::build(state);
	sendString(client_fd, resp);
}

std::string
RequestHandler::getParentDir(const std::string &child_url) const{
	size_t par_dir_pos = child_url.find_last_of('/');

	std::string parent_dir = (par_dir_pos == std::string::npos) ? "./" : child_url.substr(0, par_dir_pos);

	if (parent_dir.empty()) parent_dir = "/";

	return parent_dir;
}

int RequestHandler::executePut(const std::string &url_path, const HttpRequest &req, const std::string &req_path) const{
	struct stat		info;
	std::string		parent_dir;

	parent_dir = getParentDir(url_path);
	if (stat(parent_dir.c_str(), &info) != 0) {
		return 404;
	}
	if (!S_ISDIR(info.st_mode))
		return 409; // is not a dir
	if (!(info.st_mode & S_IWUSR))
		return 403; // no write permision

	if (stat(req_path.c_str(), &info) == 0) {
		if (S_ISDIR(info.st_mode))
			return 409;
		
		return 204; //(overwrite)
	}
	return uploadFile(req, url_path);
}

int	RequestHandler::uploadFile(const HttpRequest &req, const std::string &file_path) const{
	std::ofstream	file(file_path.c_str(),  std::ios::binary | std::ios::trunc);
	if (!file.is_open())
		return 500;

	if (file.good()) {
		file.write(req.getBody().c_str(), req.getBody().size());
	}
	else
		return 500;
	return 201;
}

std::string
RequestHandler::getExtensionFromMime(const std::string &mime_type) const{
	static std::map<std::string, std::string>	mime_map;

	if (mime_map.empty()) {
		mime_map["image/jpeg"] = ".jpg";
		mime_map["image/png"] = ".png";
		mime_map["image/gif"] = ".gif";
		mime_map["image/webp"] = ".webp";
		mime_map["text/plain"] = ".txt";
		mime_map["text/html"] = ".html";
		mime_map["application/json"] = ".json";
		mime_map["application/pdf"] = ".pdf";
	}
	std::map<std::string, std::string>::const_iterator it = mime_map.find(mime_type);

	if (it != mime_map.end())
		return it->second;
	
	return ".bin";
}

std::string RequestHandler::manageFileExtension(const HttpRequest &req, const std::string &phys_path) const{
	size_t extension_pos = phys_path.rfind('.');

	if (extension_pos != std::string::npos)
		return phys_path;

	std::string con_type_exten = req.getHeader("content-type");

	if (con_type_exten.empty())
		return phys_path + getExtensionFromMime(con_type_exten);

	return phys_path + ".bin";
}

void 
RequestHandler::putBinary(const HttpRequest &req, int client_fd, const ResolvedAction &action) const{
	const std::string &url_path = manageFileExtension(req, action.target_path);
	int status_code = executePut(url_path, req, action.target_path);
	
	ResponseState resp(status_code);

	if (status_code >= 400) {
		resp.body = generatePage(status_code, Response::getStatusText(status_code));
	}
	else {
		resp.body = "";
	}

	Response::finalizeResponse(resp, req.getPath(), resp.body.length(), action.keep_alive); // todo

	sendString(client_fd, Response::build(resp));
}

void RequestHandler::handlePut(const HttpRequest &req, int client_fd, const ResolvedAction &action) const{
	ResponseState status;
	
	std::string contentType = req.getHeader("content-type");
	if (contentType != "" && contentType.find("Multipart") == contentType.npos) {
		putBinary(req, client_fd, action);
	}
	// TODO create response struct as well as response builder to keep DRY
	//2) send response
}

void
RequestHandler::sendDefaultError(int status_code, int client_fd) const{
	ResponseState resp(status_code);

	resp.body = generatePage(status_code, Response::getStatusText(status_code));
	Response::finalizeResponse(resp, "", resp.body.length());
	std::string toSend = Response::build(resp);

	sendString(client_fd, toSend);
}

void
RequestHandler::sendDefaultError(const ResolvedAction &action, int client_fd) const{
	ResponseState resp(action.status_code);
	
	resp.body = generatePage(action.status_code, Response::getStatusText(action.status_code));
	Response::finalizeResponse(resp, "", resp.body.length(), action.keep_alive);
	std::string toSend = Response::build(resp);

	sendString(client_fd, toSend);
}

void
RequestHandler::redirect(int client_fd, const ResolvedAction &action) const{
	ResponseState resp(action.status_code);

	resp.body = generatePage(action.status_code, Response::getStatusText(action.status_code));

	Response::finalizeResponse(resp, action.target_path, resp.body.length(), action.keep_alive);
	std::string toSend = Response::build(resp);

	sendString(client_fd, toSend);
}

int
RequestHandler::deleteFileElseError(const ResolvedAction &action) const{
	std::string		par_dir;
	struct stat		info;

	if (stat(action.target_path.c_str(), &info) != 0)
		return 404;
	if (S_ISDIR(info.st_mode))
		return 403;
	if (access(action.target_path.c_str(), W_OK | X_OK) != 0)
		return 403;

	par_dir = getParentDir(action.target_path);

	if (stat(par_dir.c_str(), &info) != 0)
		return 404;
	if (!S_ISDIR(info.st_mode))
		return 403;
	if (access(par_dir.c_str(), W_OK | X_OK) != 0)
		return 403;
	std::remove(action.target_path.c_str());

	return 204;
}

void
RequestHandler::handleDelete(int client_fd, const ResolvedAction &action) const{
	int				status_code;

	status_code = deleteFileElseError(action);
	ResponseState resp(status_code);
	Response::finalizeResponse(resp, action.req_path, resp.body.length(), action.keep_alive);

	std::string toSend = Response::build(resp);
	sendString(client_fd, toSend);
}

void RequestHandler::handle(const HttpRequest &req, int client_fd, CgiInfo &state, const ResolvedAction &action) const{
	switch (action.type) {
		case ACTION_SERVE_FILE:
			return sendFile(action, client_fd);
		case ACTION_GENERATE_ERROR:
			return sendDefaultError(action, client_fd);
		case ACTION_AUTOINDEX:
			return sendDir(action, client_fd, req.getPath());
		case ACTION_CGI:
			return state.addFds(action); // return 200 status code?
		case ACTION_REDIRECT:
			return redirect(client_fd, action);
		case ACTION_UPLOAD_FILE:
			return handlePut(req, client_fd, action);
		case ACTION_DELETE_FILE:
			return handleDelete(client_fd, action);
		default:
			return sendDefaultError(action, client_fd);
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
