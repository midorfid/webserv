#include "RouteRequest.hpp"
#include "Environment.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include "log.hpp"
#include <string_view>
#include <algorithm>
#include <vector>
#include <cerrno>
#include <csignal>
#include <sys/wait.h>

const Location *
RouteRequest::findBestLocationMatch(const Config &serv_cfg, std::string_view url) {
    const Location *best = nullptr;
    size_t          best_len = 0;

    for (const auto &loc : serv_cfg.getLocations()) {
        const std::string &path = loc.getPath();
        if (url.substr(0, path.size()) == path && path.size() > best_len) {
            best     = &loc;
            best_len = path.size();
        }
    }
    return best;
}

ResolvedAction	RouteRequest::resolveErrorAction(int error_code, const Config &serv_cfg, ResolvedAction &action) {
	const auto &error_pages = serv_cfg.getSharedCtx().error_pages;
	auto it = error_pages.find(error_code);

	if (it != error_pages.end()) {
		const std::string &error_url = it->second;
		const Location *location = findBestLocationMatch(serv_cfg, error_url);
		
		std::string root = location ? location->getSharedCtx().root : serv_cfg.getSharedCtx().root;
		
		std::string file_path = root + error_url;
		
		struct stat st;
		if (stat(file_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
			action.status_code = error_code;
			action.target_path = file_path;
			action.type = ACTION_SERVE_FILE;
			return action;
		}
	}
	action.status_code = error_code;
	action.type = ACTION_GENERATE_ERROR;
	return action;
}

ResolvedAction RouteRequest::resolveFileAction(ResolvedAction &action) {
	action.status_code = 200;
	action.type = ACTION_SERVE_FILE;
	return action;
}

bool	RouteRequest::findAccessibleIndex(ResolvedAction &action, const std::string &dir_path,
											const std::vector<std::string> &indexes) {
	for (const auto &index : indexes) {
		std::string full_path = dir_path + index;
		struct stat st;

		if (stat(full_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
			action.status_code = 200;
			action.target_path = full_path;
			action.type = ACTION_SERVE_FILE;
			return true;
		}
	}
	return false;
}

bool RouteRequest::NoSlash(std::string_view str) {
	return !str.empty() && str.back() != '/';
}

ResolvedAction RouteRequest::resolveRedirect(const std::string &dir_path, ResolvedAction &action, int status_code) {
	action.status_code = status_code;
	action.target_path = dir_path;
	action.type = ACTION_REDIRECT;
	return action;
}

ResolvedAction RouteRequest::resolveDirAction(const std::string &dir_path, const Config &cfg,
												const Location *location, ResolvedAction &action) {
	if (NoSlash(dir_path))
		return resolveRedirect(action.req_path + '/', action);

	const auto &loc_ctx = location->getSharedCtx();
	if (!loc_ctx.index_files.empty() && findAccessibleIndex(action, dir_path, loc_ctx.index_files))
		return action;

	if (loc_ctx.autoindex) {
		action.status_code = 200;
		action.target_path = dir_path;
		action.type = ACTION_AUTOINDEX;
		return action;
	}
	return resolveErrorAction(403, cfg, action);
}

ResolvedAction
RouteRequest::resolvePutUpload(ResolvedAction &action) {
	return action;
}

ResolvedAction
RouteRequest::resolveDeleteAction(ResolvedAction &action) {
	return action;
}

ResolvedAction	RouteRequest::checkReqPath(const Config &cfg, const Location *location, ResolvedAction &action) {
	struct stat info;
	
	if (action.type == ACTION_UPLOAD_FILE || action.type == ACTION_POST_UPLOAD)
		return resolvePutUpload(action);
	if (stat(action.target_path.c_str(), &info) != 0) {
		switch(errno) {
			case ENOENT:
				return resolveErrorAction(404, cfg, action);
			case EACCES:
				return resolveErrorAction(403, cfg, action);
			default:
				return resolveErrorAction(500, cfg, action);
		}
	}
	if (action.type == ACTION_DELETE_FILE)
		return resolveDeleteAction(action);

	if (S_ISDIR(info.st_mode)) {
		return resolveDirAction(action.target_path, cfg, location, action);
	}
	else if (S_ISREG(info.st_mode)) {
		return resolveFileAction(action);
	}
	return resolveErrorAction(403, cfg, action);
}

bool RouteRequest::checkLimitExcept(const std::string &method, const Location &loc) {
	const auto &limit_opt = loc.getLimitExcept();
	if (!limit_opt.has_value()) return true;
	return (StringUtils::stringToMethod(method) & *limit_opt) != 0;
}

ResolvedAction	RouteRequest::resolveRequestToHandler(const Config &serv_cfg, const HttpRequest &req, const std::string &client_ip) {
	ResolvedAction			action;
	const std::string		&req_path = req.getPath();
	
	action.keep_alive = false;
	action.req_path = req_path;
	action.type = ACTION_NONE;

	const Location *location = findBestLocationMatch(serv_cfg, req_path);
	if (!location) {
		if (req.isKeepAlive() && serv_cfg.getSharedCtx()._keepalive_timer > 0)
			action.keep_alive = true;
		return resolveErrorAction(404, serv_cfg, action);
	}

	const auto &ctx = location->getSharedCtx();

	// Determine keep-alive BEFORE any early return so all response paths inherit it
	if (req.isKeepAlive() && ctx._keepalive_timer > 0)
		action.keep_alive = true;

	if (ctx.redirect.has_value()) {
		return resolveRedirect(ctx.redirect->second, action, ctx.redirect->first);
	}

	if (!checkLimitExcept(req.getMethod(), *location)) {
		return resolveErrorAction(405, serv_cfg, action);
	}
	
	setActionType(action, req.getMethod());

	return PathFinder(req, *location, serv_cfg, action, client_ip);
}

void 
RouteRequest::setActionType(ResolvedAction &action, const std::string &met) {
	if (met == "PUT")
		action.type = ACTION_UPLOAD_FILE;
	else if (met == "DELETE")
		action.type = ACTION_DELETE_FILE;
	else if (met == "POST")
		action.type = ACTION_POST_UPLOAD;
}

std::string RouteRequest::catPathes(const std::string &reqPath, std::string &root_path, ActionType at) {
	std::string		full_path;
	struct stat		info;

	if (reqPath.find(root_path) != std::string::npos) {
		full_path = reqPath;
	}
	else {
		if (!root_path.empty() && root_path.back() == '/' && !reqPath.empty() && reqPath.front() == '/')
			root_path.pop_back();
		full_path = root_path + reqPath;
	}

	if (at != ACTION_UPLOAD_FILE && stat(full_path.c_str(), &info) != 0)
		logTime(ERRLOG, "catPathes stat failed: " + full_path);
	return full_path;
}

ResolvedAction	RouteRequest::PathFinder(const HttpRequest &req, const Location &loc, const Config &serv_cfg, ResolvedAction &action, const std::string &client_ip) {
	std::string root_path = loc.getSharedCtx().root;
	if (root_path.empty()) root_path = serv_cfg.getSharedCtx().root;

	action.target_path = catPathes(req.getPath(), root_path, action.type);

	auto ends_with = [](std::string_view str, std::string_view suffix) {
		return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), std::string_view::npos, suffix) == 0;
	};

	if (loc.getSharedCtx().allow_cgi && (ends_with(action.target_path, ".py") || ends_with(action.target_path, ".cgi"))) {
		return resolveCgiScript(serv_cfg, req, loc, action, client_ip);
	}

	return checkReqPath(serv_cfg, &loc, action);
}

ResolvedAction
RouteRequest::resolveCgiScript(const Config &serv_cfg, const HttpRequest &req, const Location &loc, ResolvedAction &action, const std::string &client_ip) {
	int serv_to_cgi[2];
	int cgi_to_serv[2];

	if (pipe(serv_to_cgi) == -1) {
		logTime(ERRLOG, std::string("pipe(serv_to_cgi): ") + strerror(errno));
		return resolveErrorAction(500, serv_cfg, action);
	}
	if (pipe(cgi_to_serv) == -1) {
		logTime(ERRLOG, std::string("pipe(cgi_to_serv): ") + strerror(errno));
		close(serv_to_cgi[0]); close(serv_to_cgi[1]);
		return resolveErrorAction(500, serv_cfg, action);
	}

	pid_t cpid = fork();
	if (cpid == -1) {
		logTime(ERRLOG, std::string("fork: ") + strerror(errno));
		close(serv_to_cgi[0]); close(serv_to_cgi[1]);
		close(cgi_to_serv[0]); close(cgi_to_serv[1]);
		return resolveErrorAction(500, serv_cfg, action);
	}

	if (cpid == 0) {
		// ── child ──────────────────────────────────────────────────────
		close(cgi_to_serv[0]);
		close(serv_to_cgi[1]);

		if (dup2(serv_to_cgi[0], STDIN_FILENO) == -1 ||
		    dup2(cgi_to_serv[1], STDOUT_FILENO) == -1) {
			perror("dup2");
			exit(EXIT_FAILURE);
		}
		close(serv_to_cgi[0]);
		close(cgi_to_serv[1]);

		Environment envp_builder(req, loc, client_ip);
		envp_builder.build(action.target_path);

		const char *interp = "/usr/bin/python3";
		char *argv[] = {
			const_cast<char *>(interp),
			const_cast<char *>(action.target_path.c_str()),
			nullptr
		};
		execve(interp, argv, envp_builder.getEnvp());
		perror("execve");
		exit(EXIT_FAILURE);
	}

	// ── parent ─────────────────────────────────────────────────────────
	close(serv_to_cgi[0]);
	close(cgi_to_serv[1]);

	if (fcntl(serv_to_cgi[1], F_SETFL, O_NONBLOCK) == -1 ||
	    fcntl(cgi_to_serv[0], F_SETFL, O_NONBLOCK) == -1) {
		logTime(ERRLOG, std::string("fcntl: ") + strerror(errno));
		close(serv_to_cgi[1]);
		close(cgi_to_serv[0]);
		kill(cpid, SIGKILL);
		waitpid(cpid, nullptr, WNOHANG);
		return resolveErrorAction(500, serv_cfg, action);
	}

	action.type            = ACTION_CGI;
	action.status_code     = 200;
	action.cgi_fds.first   = cgi_to_serv[0];   // server reads CGI stdout
	action.cgi_fds.second  = serv_to_cgi[1];   // server writes CGI stdin
	action.child_pid       = cpid;
	return action;
}
