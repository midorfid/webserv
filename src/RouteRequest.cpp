#include "RouteRequest.hpp"

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

ResolvedAction RouteRequest::resolveDirAction(const std::string &dir_path, const Config &cfg,
												struct stat *st, const Location *location) {
	ResolvedAction				action;
	std::vector<std::string>	indexes;

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


ResolvedAction	RouteRequest::checkReqPath(const std::string &path, const Config &cfg, const Location *location) {
	struct stat st;

	if (stat(path.c_str(), &st) != 0) {
		switch(errno) {
			case ENOENT:
				return resolveErrorAction(404, cfg);
			case EACCES:
				return resolveErrorAction(403, cfg);
			default:
				return resolveErrorAction(500, cfg);
		}
	}
	if (S_ISDIR(st.st_mode)) {
		return resolveDirAction(path, cfg, &st, location);
	}
	else if (S_ISREG(st.st_mode))
		return resolveFileAction(path, &st);
	//Fallback
	return resolveErrorAction(403, cfg);
}

ResolvedAction	RouteRequest::resolveRequestToHandler(const Config &serv_cfg, const HttpRequest &req) {
	std::string				phys_path;
	const std::string		&req_path = req.getPath();

	const Location *location = findBestLocationMatch(serv_cfg, req_path);
	if (location == NULL) {
		return resolveErrorAction(404, serv_cfg);
	}
	if (location->getRulesMethods().find(req.getMethod()) == std::string::npos) {
		if (location.getRu)
	}
	if (location->getDirective("root", phys_path) == false) {
		return resolveErrorAction(500, serv_cfg); // maybe different error
	}
	if (location->isCgiAllowed()) {
		std::vector<std::string> req_path_tokens = StringUtils::split(req_path, '/');

		for (std::vector<std::string>::const_iterator it = req_path_tokens.begin(); it != req_path_tokens.end(); ++it) {
			if (it->length() > location->getCgiFormat().length() && it->substr(it->length() - 3) == location->getCgiFormat()) //TODO this is weird
				return resolveCgiScript(location, serv_cfg, req);
		}
	}

}

void RouteRequest::routePath(const std::string &req_path, ) {

	if (req_path == "/") {

		std::string index;

		if (!location->getIndex(index))

			return resolveErrorAction(404, serv_cfg);

		struct stat st;

		std::string physAndIdxPath = phys_path + index;

		if (stat(physAndIdxPath.c_str(), &st) == 0) {

			return resolveFileAction(physAndIdxPath, &st);

		} // handle if index not found, autoindex off TODO as well as multiple indexes

		return resolveErrorAction(404, serv_cfg);

	}

	if (normalizePath(phys_path) == false) {

		return resolveErrorAction(404, serv_cfg);

	}

	return checkReqPath(phys_path, serv_cfg, location);

}

ResolvedAction
RouteRequest::resolveCgiScript(const Location *loc, const Config &serv_cfg, const HttpRequest &req) {
	std::string root_path;

	loc->getDirective("root", root_path);
	std::string scriptPhysAddr = root_path + req.getPath(); // query?? TODO

	if (normalizePath(scriptPhysAddr) == false)
		resolveErrorAction(404, serv_cfg);

	struct stat st;

	if (stat(scriptPhysAddr.c_str(), &st) != 0) {
		std::cerr << "script stat() failed :(" << std::endl;
		resolveErrorAction(403, serv_cfg);
	}
	pid_t	cpid;
	int		serv_to_cgi[2];
	int		cgi_to_serv[2];

	if (pipe(serv_to_cgi) == -1 || pipe(cgi_to_serv) == -1) {
		std::cerr << "pipe" << std::endl;
		resolveErrorAction(500, serv_cfg);
	}
	cpid = fork();
	if (cpid == -1) {
		std::cerr << "fork" << std::endl;
		resolveErrorAction(500, serv_cfg);
	}
	if (cpid == 0) {
		close(cgi_to_serv[0]);
		close(serv_to_cgi[1]);

		if (dup2(serv_to_cgi[0], STDIN_FILENO) == -1) {
			std::cerr << "dup2" << std::endl;
			resolveErrorAction(500, serv_cfg);
		}
		if (dup2(cgi_to_serv[1], STDOUT_FILENO) == -1) {
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
		std::cerr << "script path: " << scriptPhysAddr << std::endl;
		if (execve(program, argv, envp_builder.getEnvp()) == -1) {
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
				std::cerr << "fcntl" << std::endl;
				resolveErrorAction(500, serv_cfg);
		}
		
		ResolvedAction	action;
		
		action.st = st;
		action.type = ACTION_CGI;
		action.status_code = 200;
		action.target_path = scriptPhysAddr;
		action.cgi_fds.first = cgi_to_serv[0];
		action.cgi_fds.second = serv_to_cgi[1];

		// close(serv_to_cgi[1]);
		// close(cgi_to_serv[0]);
		return action;
	}
}
