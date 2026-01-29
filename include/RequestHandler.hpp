#pragma once

#include <string>
#include "httpRequest.hpp"
#include "Config.hpp"
#include "cgi.hpp"
#include "RouteRequest.hpp"

class Server;

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
		std::string			getHttpDate() const;
		void				sendString(int client_fd, const std::string &response) const;
		void				streamFileBody(int client_fd, const std::string &file_path) const;
								
		std::string			genAutoindexAction(const ResolvedAction &action) const;
		std::string			createDirListHtml(const std::string &physical_path,
									const std::string &logic_path) const;

		void			sendFile(const ResolvedAction &action, int client_fd) const;
		void			sendDir(const std::string &phys_path, int client_fd, const std::string &logic_path) const;

		const std::string		getStatusText(int code) const;
		const std::string		generic_error_response() const;
		const std::string		createSuccResponseHeaders(long int contentLen) const;
};