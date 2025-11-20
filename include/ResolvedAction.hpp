#pragma once

#include <string>
#include <sys/stat.h>

enum ActionType {
	ACTION_SERVE_FILE,
	ACTION_AUTOINDEX,
	ACTION_GENERATE_ERROR,
	ACTION_CGI,
	ACTION_REDIRECT,
};

struct ResolvedAction {
	ActionType			type;
	std::string			target_path;
	int					status_code;
	struct stat 		st;
	std::pair<int,int>	cgi_fds;
};