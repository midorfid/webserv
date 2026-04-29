#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

struct ResponseState {
	int													status_code;
	std::string											body;
	std::unordered_map<std::string, std::string>		headers;
	std::vector<std::string>							cookies;

	ResponseState(int status_code) : status_code(status_code) {}
	ResponseState() {}
	~ResponseState() {}

	void addHeader(const std::string &key, const std::string &val) {
		if (key == "set-cookie") {
			cookies.push_back(val);
			return;
		}
		if (key == "content-length" || key == "content-type") {
			headers[key] = val;
			return;
		}
		if (headers.find(key) != headers.end()) {
			headers[key] += ", " + val;
		} else {
			headers[key] = val;
		}
	}
};

namespace Response {
	std::string		build(const ResponseState &resp);
    void            finalizeResponse(ResponseState &resp, const std::string &path, size_t bodySize,
                                     bool isConKeepAlive = false, const std::string &session_cookie = "");
    void            finalizeResponseChunked(ResponseState &resp, const std::string &path,
                                            bool isConKeepAlive = false, const std::string &session_cookie = "");
    std::string     encodeChunked(const std::string &body);
    std::string     encodeChunk(const std::string &data); // one frame, no terminal chunk
	std::string		getHttpDate();
	std::string		getStatusText(int code);
};
