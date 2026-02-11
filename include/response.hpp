#pragma once

#include <string>
#include <sstream>
#include <vector>

struct ResponseState {
	int													status_code;
	std::string											body;
	std::vector<std::pair<std::string, std::string> >	headers;

	ResponseState(int status_code) : status_code(status_code) {}
	ResponseState() {}
	~ResponseState() {}

	void addHeader(const std::string &key, const std::string &val) {
		headers.push_back(make_pair(key, val));
	}
};

namespace Response {
	std::string		build(const ResponseState &resp);
    void            finalizeResponse(ResponseState &resp, const std::string &path);
	std::string		getHttpDate();
	std::string		getStatusText(int code);
};
