#pragma once

#include "Config.hpp"
#include "httpRequest.hpp"
#include <string>
#include <sys/stat.h>
#include <iostream>
#include "StringUtils.hpp"

enum ActionType {
	ACTION_SERVE_FILE,
	ACTION_AUTOINDEX,
	ACTION_GENERATE_ERROR,
	ACTION_CGI,
	ACTION_REDIRECT,
	ACTION_RESOLVE_TO_HANDLER,
	ACTION_UPLOAD_FILE,
	ACTION_DELETE_FILE,
	ACTION_POST_UPLOAD,
	ACTION_NONE,
};

struct ResolvedAction {
	ActionType			type;
	bool				keep_alive;
	std::string			target_path;
	std::string			req_path; // for no slash redirect
	int					status_code;
	std::pair<int,int>	cgi_fds;
	pid_t				child_pid;
};

class RouteRequest {
    public:
        ResolvedAction	    resolveRequestToHandler(const Config &serv_cfg, const HttpRequest &req, const std::string &client_ip);

        // Pure-logic helpers exposed for unit testing
        const Location      *findBestLocationMatch(const Config &serv_cfg, std::string_view url);
        bool                checkLimitExcept(const std::string &method, const Location &loc);
        void                setActionType(ResolvedAction &action, const std::string &met);

	private:

		std::string			catPathes(const std::string &reqPath, std::string &root_path, ActionType at);
		ResolvedAction		resolveRedirect(const std::string &dir_path, ResolvedAction &action, int status_code = 301);
		bool				NoSlash(std::string_view str);
		ResolvedAction		PathFinder(const HttpRequest &req, const Location &loc, const Config &serv_cfg, ResolvedAction &action, const std::string &client_ip);
        std::string			handlePath(const Config &serv_cfg, HttpRequest &req);
		ResolvedAction		resolveErrorAction(int error_code, const Config &serv_cfg, ResolvedAction &action);
		ResolvedAction		resolveFileAction(ResolvedAction &action);
		ResolvedAction		resolveDirAction(const std::string &path, const Config &cfg,
								const Location *location, ResolvedAction &action);
		ResolvedAction		resolvePutUpload(ResolvedAction &action);
		bool				findAccessibleIndex(ResolvedAction &action, const std::string &dir_path,
								const std::vector<std::string> &indexes);
        ResolvedAction      checkReqPath(const Config &cfg, const Location *location, ResolvedAction &action);
        ResolvedAction      resolveCgiScript(const Config &serv_cfg, const HttpRequest &req, const Location &loc, ResolvedAction &action, const std::string &client_ip);
		ResolvedAction		resolveDeleteAction(ResolvedAction &action);
};
