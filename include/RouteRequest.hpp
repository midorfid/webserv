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
};

struct ResolvedAction {
	ActionType			type;
	std::string			target_path;
	int					status_code;
	struct stat 		st;
	std::pair<int,int>	cgi_fds;
};

class RouteRequest {
    public:
        ResolvedAction	resolveRequestToHandler(const Config &serv_cfg, const HttpRequest &req, const std::string &client_ip);
    

	private:

		std::string			catPathes(const std::string &reqPath, std::string &root_path, struct stat *st);
		ResolvedAction		resolveRedirect(const std::string &dir_path, struct stat *st, int status_code = 301);
		bool				NoSlash(const std::string &str);
		ResolvedAction		PathFinder(const HttpRequest &req, const Location &loc, const Config &serv_cfg, struct stat *st);
		bool				checkLimitExcept(const std::string &method, const std::string &client_ip);
        const Location      *findBestLocationMatch(const Config &serv_cfg, const std::string &url);
        std::string			handlePath(const Config &serv_cfg, HttpRequest &req);
		ResolvedAction		resolveErrorAction(int error_code, const Config &serv_cfg);
		ResolvedAction		resolveFileAction(const std::string &path, struct stat *st);
		ResolvedAction		resolveDirAction(const std::string &path, const Config &cfg, struct stat *st,
								const Location *location);
		bool				findAccessibleIndex(ResolvedAction &action, const std::string &dir_path,
								const std::vector<std::string> &indexes);
        ResolvedAction      checkReqPath(const std::string &path, const Config &cfg, const Location *location, struct stat *st);
        ResolvedAction      resolveCgiScript(const Config &serv_cfg, const HttpRequest &req, const std::string &full_path, struct stat *st);
};
