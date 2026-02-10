#pragma once

#include <string>
#include "httpRequest.hpp"
#include "Config.hpp"
#include "cgi.hpp"
#include "RouteRequest.hpp"

class Server;

struct ResponseState {
	int													status_code;
	std::string											body;
	std::vector<std::pair<std::string, std::string> >	headers;

	ResponseState(int status_code) : status_code(status_code) {}
	ResponseState();
	~ResponseState();

	void addHeader(const std::string &key, const std::string &val) {
		headers.push_back(make_pair(key, val));
	}
};

namespace Response {
	std::string		buildHeaders(const ResponseState &resp);
	std::string		getHttpDate();
	std::string		getStatusText(int code);
};

std::string
Response::buildHeaders(const ResponseState &resp) {
	std::stringstream		response;
	
	std::string status_text = Response::getStatusText(resp.status_code);
	response << "HTTP/1.1" << resp.status_code << status_text << "\r\n";
	response << "Date: " << Response::getHttpDate() << "\r\n";
	response << "Server: " << "Webserv/ver 1.0\r\n";
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = resp.headers.begin(); it != resp.headers.end(); ++it) {
		response << it->first << " " << it->second << "\r\n";
	}
	response << "\r\n";

	if (!resp.body.empty())
		response << resp.body;

	return response.str();
}

class RequestHandler {
	public:
		RequestHandler();
		~RequestHandler();
		
		RequestHandler(const RequestHandler &other);
		RequestHandler &operator=(const RequestHandler &other);
		
		void		handle(const HttpRequest &req, int client_fd, CgiInfo &state, const ResolvedAction &action) const;
		void		sendDefaultError(int status_code, int client_fd) const;
		
	private:

		void				redirect(int client_fd, const std::string &new_path) const;
		void				sendString(int client_fd, const std::string &response) const;
		void				streamFileBody(int client_fd, const std::string &file_path) const;
								
		std::string			genAutoindexAction(const ResolvedAction &action) const;
		ResponseState		createDirListHtml(const std::string &physical_path,
									const std::string &logic_path) const;

		void			sendFile(const ResolvedAction &action, int client_fd) const;
		void			sendDir(const std::string &phys_path, int client_fd, const std::string &logic_path) const;
		
		int						checkFileCreation(const std::string &url_path);
		const std::string		&checkFileExtension(const HttpRequest &req);
		void					handlePut(const Config &serv_cfg, const HttpRequest &req, int client_fd);
		ResponseState			&putBinary(const HttpRequest &req);
		std::string				generatePage(int error_code, const std::string &text, const std::string &details = "") const;
};
