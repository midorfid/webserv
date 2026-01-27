#include "RouteRequest.hpp"
#include "Environment.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "log.hpp"

const Location	*findBestLocationMatch(const Config &serv_cfg, const std::string &url) {
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

ResolvedAction	RouteRequest::resolveErrorAction(int error_code, const Config &serv_cfg) {
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

ResolvedAction RouteRequest::resolveFileAction(const std::string &path, struct stat *st) {
	ResolvedAction	action;

	action.st = *st;
	action.status_code = 200;
	action.type = ACTION_SERVE_FILE;
	action.target_path = path;

	return action;
}

bool	RouteRequest::findAccessibleIndex(ResolvedAction &action, const std::string &dir_path,
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

bool RouteRequest::NoSlash(const std::string &str) {
	return (str.back() == '/') ? false : true;
}

ResolvedAction RouteRequest::resolveRedirect(const std::string &dir_path, struct stat *st) {
	ResolvedAction	action;

	action.st = *st;
	action.status_code = 301;
	action.target_path = dir_path + '/';
	action.type = ACTION_REDIRECT;
	return action;
}

ResolvedAction RouteRequest::resolveDirAction(const std::string &dir_path, const Config &cfg,
												struct stat *st, const Location *location) {
	ResolvedAction				action;
	std::vector<std::string>	indexes;

	if (NoSlash(dir_path))
		return resolveRedirect(dir_path, st);
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


ResolvedAction	RouteRequest::checkReqPath(const std::string &path, const Config &cfg, const Location *location, struct stat *st) {
	if (stat(path.c_str(), st) != 0) {
		switch(errno) {
			case ENOENT:
				return resolveErrorAction(404, cfg);
			case EACCES:
				return resolveErrorAction(403, cfg);
			default:
				return resolveErrorAction(500, cfg);
		}
	}
	if (S_ISDIR(st->st_mode)) {
		return resolveDirAction(path, cfg, st, location);
	}
	else if (S_ISREG(st->st_mode))
		return resolveFileAction(path, st);
	//Fallback
	return resolveErrorAction(403, cfg);
}

ResolvedAction	RouteRequest::resolveRequestToHandler(const Config &serv_cfg, const HttpRequest &req, const std::string &client_ip) {
	std::string				phys_path;
	const std::string		&req_path = req.getPath();

	const Location *location = findBestLocationMatch(serv_cfg, req_path);
	if (location == NULL) {
		return resolveErrorAction(404, serv_cfg);
	}
	if (!location->checkLimExceptAccess(req.getMethod(), client_ip)) {
		return resolveErrorAction(403, serv_cfg);
	}

	return PathFinder(req, *location, serv_cfg);
}

const std::string &RouteRequest::catPathes(const std::string &reqPath, std::string &root_path, struct stat *st) {
	std::string full_path = root_path + reqPath; // query?? TODO

	if (stat(full_path.c_str(), st) != 0) {
		logTime(ERRLOG);
		std::cerr << "RouteRequest::catPathes() stat() failed :(" << std::endl;
	}

	return full_path;
}

ResolvedAction	RouteRequest::PathFinder(const HttpRequest &req, const Location &loc, const Config &serv_cfg) {
	std::string root_path;
	struct stat st;

	loc.getDirective("root", root_path);
	const std::string &full_path = catPathes(req.getPath(), root_path, &st);
	
	if (loc.isCgiRequest(full_path)) {
		return resolveCgiScript(&loc, serv_cfg, req, full_path, &st);
	}

	return checkReqPath(full_path, serv_cfg, &loc, &st);
}

ResolvedAction
RouteRequest::resolveCgiScript(const Location *loc, const Config &serv_cfg, const HttpRequest &req, const std::string &full_path, struct stat *st) {
	pid_t	cpid;
	int		serv_to_cgi[2];
	int		cgi_to_serv[2];

	if (pipe(serv_to_cgi) == -1 || pipe(cgi_to_serv) == -1) {
		logTime(ERRLOG);
		std::cerr << "pipe" << std::endl;
		resolveErrorAction(500, serv_cfg);
	}
	cpid = fork();
	if (cpid == -1) {
		logTime(ERRLOG);
		std::cerr << "fork" << std::endl;
		resolveErrorAction(500, serv_cfg);
	}
	if (cpid == 0) {
		close(cgi_to_serv[0]);
		close(serv_to_cgi[1]);

		if (dup2(serv_to_cgi[0], STDIN_FILENO) == -1) {
			logTime(ERRLOG);
			std::cerr << "dup2" << std::endl;
			resolveErrorAction(500, serv_cfg);
		}
		if (dup2(cgi_to_serv[1], STDOUT_FILENO) == -1) {
			logTime(ERRLOG);
			std::cerr << "dup2" << std::endl;
			resolveErrorAction(500, serv_cfg);
		}

		close(cgi_to_serv[1]);
		close(serv_to_cgi[0]);

		Environment	envp_builder(req);

		envp_builder.build();

		char *intep = const_cast<char*>("/usr/bin/python3"); // TODO hardcoded change!
		char *program = const_cast<char*>("../www/script.py");

		char *const argv[] = {intep, program, NULL};
		logTime(REGLOG);
		std::cerr << "script path: " << full_path << std::endl;
		if (execve(program, argv, envp_builder.getEnvp()) == -1) {
			logTime(ERRLOG);
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
				logTime(ERRLOG);
				std::cerr << "fcntl" << std::endl;
				resolveErrorAction(500, serv_cfg);
		}
		
		ResolvedAction	action;
		
		action.st = *st;
		action.type = ACTION_CGI;
		action.status_code = 200;
		action.target_path = full_path;
		action.cgi_fds.first = cgi_to_serv[0];
		action.cgi_fds.second = serv_to_cgi[1];

		// close(serv_to_cgi[1]);
		// close(cgi_to_serv[0]);
		return action;
	}
}
