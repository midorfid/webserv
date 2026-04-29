#pragma once

#include <string>
#include "httpRequest.hpp"
#include "Config.hpp"
#include "cgi.hpp"
#include "RouteRequest.hpp"
#include "response.hpp"

class Server;
class Client;

class RequestHandler {
	public:
		RequestHandler();
		~RequestHandler();
		
		RequestHandler(const RequestHandler &other);
		RequestHandler &operator=(const RequestHandler &other);
		
		void		handle(const HttpRequest &req, Client &client, CgiInfo &state, const ResolvedAction &action) const;
		void		sendDefaultError(const ResolvedAction &action, Client &client) const;
		void		sendDefaultError(int error_code, Client &client) const;
	private:

		void				redirect(Client &client, const ResolvedAction &action) const;
		void				sendString(Client &client, const std::string &response) const;
		void				streamFileBody(Client &client, const std::string &file_path) const;
								
		std::string			genAutoindexAction(const ResolvedAction &action) const;
		ResponseState		createDirListHtml(const std::string &physical_path, const std::string &logic_path) const;

		void			sendFile(const ResolvedAction &action, Client &client) const;
		void			sendDir(const ResolvedAction &action, Client &client, const std::string &logic_path) const;
		
		int						executePut(const std::string &url_path, const HttpRequest &req, const std::string &req_path) const;
		std::string				manageFileExtension(const HttpRequest &req, const std::string &phys_path) const;
		void					handlePut(const HttpRequest &req, Client &client, const ResolvedAction &action) const;
		void					handlePost(const HttpRequest &req, Client &client, const ResolvedAction &action) const;
		void					handleDelete(Client &client, const ResolvedAction &action) const;
		void					putBinary(const HttpRequest &req, Client &client, const ResolvedAction &action) const;
		std::string				generatePage(int error_code, const std::string &text, const std::string &details = "") const;
		int						uploadFile(const HttpRequest &req, const std::string &file_path) const;

		int						deleteFileElseError(const ResolvedAction &action) const;

		std::string				getParentDir(const std::string &child_url) const;			
		std::string				getExtensionFromMime(const std::string &mime_type) const;

		off_t					calculateTargetSize(const std::string &target_path) const;

};
