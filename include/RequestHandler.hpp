#pragma once

#include <string>
#include "httpRequest.hpp"
#include "Config.hpp"
#include "cgi.hpp"
#include "RouteRequest.hpp"
#include "response.hpp"

class Server;

class RequestHandler {
	public:
		RequestHandler();
		~RequestHandler();
		
		RequestHandler(const RequestHandler &other);
		RequestHandler &operator=(const RequestHandler &other);
		
		void		handle(const HttpRequest &req, int client_fd, CgiInfo &state, const ResolvedAction &action) const;
		void		sendDefaultError(const ResolvedAction &action, int client_fd) const;
		void		sendDefaultError(int error_code, int client_fd) const;
	private:

		void				redirect(int client_fd, const ResolvedAction &action) const;
		void				sendString(int client_fd, const std::string &response) const;
		void				streamFileBody(int client_fd, const std::string &file_path) const;
								
		std::string			genAutoindexAction(const ResolvedAction &action) const;
		ResponseState		createDirListHtml(const std::string &physical_path, const std::string &logic_path) const;

		void			sendFile(const ResolvedAction &action, int client_fd) const;
		void			sendDir(const ResolvedAction &action, int client_fd, const std::string &logic_path) const;
		
		int						executePut(const std::string &url_path, const HttpRequest &req) const;
		std::string				manageFileExtension(const HttpRequest &req) const;
		void					handlePut(const HttpRequest &req, int client_fd, const ResolvedAction &action) const;
		void					putBinary(const HttpRequest &req, int client_fd, const ResolvedAction &action) const;
		std::string				generatePage(int error_code, const std::string &text, const std::string &details = "") const;
		int						uploadFile(const HttpRequest &req, const std::string &file_path) const;

		std::string				getExtensionFromMime(const std::string &mime_type) const;
};
