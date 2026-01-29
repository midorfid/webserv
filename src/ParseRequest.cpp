#include "ParseRequest.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include "log.hpp"
#include <limits.h>
#include <string.h>

#define	EOL	"\r\n"
#define	EOH	"\r\n\r\n"
#define SPACE ' '
#define MAX_URL_LENGTH 2048

/* Finds token in the src, if it's not there returns the src. */
template <typename T>
std::string		ParseRequest::trimToken(std::string &src, T token) {
	size_t			ele_pos;
	std::string		res;

	ele_pos = src.find(token);
	
	if (ele_pos != src.npos) {
		res = src.substr(0, ele_pos + 1);
		src.erase(0, ele_pos + 1);
		return	res;
	}
	return src;
}

std::vector<std::string>    ParseRequest::tokenizeFirstLine(const std::string &first_line) {
	std::vector<std::string>    tokens;
	std::string                 token;
	std::stringstream          	tokenStream;

	tokenStream.str(first_line);
	
	while (tokenStream >> token) {
		tokens.push_back(token);
	}
	return tokens;
}

void		ParseRequest::parseMethod(std::string &method, HttpRequest &req) {
	req.setMethod(method);
}

void		ParseRequest::parsePathAndQuery(std::string &pathAndQuery, HttpRequest &req) {
	std::string		_substring;

	_substring = trimToken(pathAndQuery, '?');
	if (_substring != pathAndQuery) {
		req.setPath(_substring);
		req.setQuery(pathAndQuery.substr(_substring.length()));
	}
	else {
		req.setPath(pathAndQuery);
		req.setQuery("");
	}
}


void		ParseRequest::parseHttpVer(std::string &httpVer, HttpRequest &req) {
	req.setVersion(httpVer);
}

std::string		ParseRequest::getNextLine(std::string &request) {
	return trimToken(request, EOL);
}

bool	ParseRequest::hasUnderscore(const std::string &s) const {
	return s.find('_') != s.npos;
}

void		ParseRequest::parseHeaders(std::string &request, HttpRequest &req) {
	std::string		header_line;
	std::string		key;
	std::string		value;
	size_t			delim_pos;

	while(!request.empty()) {
		header_line = getNextLine(request);
		if (header_line == EOL)
			break;
		delim_pos = header_line.find(':');
		if (delim_pos == header_line.npos)
			break;
		key = header_line.substr(0, delim_pos);
		if (hasUnderscore(key))
			continue;
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);
		value = header_line.substr(delim_pos + 1);
		trimLeftWhitespace(key);
		trimLeftWhitespace(value);
		req.addHeader(key, value);
	}
}

void		ParseRequest::trimLeftWhitespace(std::string &to_trim) {
	const std::string WHITESPACE = " \r\t\n\f\v";
	size_t start = to_trim.find_first_not_of(WHITESPACE);

	if (start == to_trim.npos)
		to_trim.clear();
	else {
		to_trim.erase(0, start);
	}
}

ParseResult		ParseRequest::parseFirstLine(std::string &_current_line, HttpRequest &req) {
	std::vector<std::string> first_line = tokenizeFirstLine(_current_line);

	parseMethod(first_line[0], req);
	if (first_line[1].length() > MAX_URL_LENGTH)
		return UrlTooLong;
	parsePathAndQuery(first_line[1], req);
	parseHttpVer(first_line[2], req);
	return Okay;
}

ParseResult	ParseRequest::parseBody(std::string &reqOnlyBody, HttpRequest &req) {
	size_t	body_size = 0;
	try
	{
		body_size = std::atoi(req.getHeader("content-length").c_str());
	}
	catch(const std::exception& e){}
	if (body_size > 0 && reqOnlyBody.size() < body_size) {
		return 	RequestIncomplete;
	}
	req.setBody(reqOnlyBody);

	return RequestComplete;
}

std::string	normalizePath(const std::string &req_path) {
	std::vector<std::string>	stack;
	std::stringstream			ss(req_path);
	std::string					token;

	while (std::getline(ss, token, '/')) {
		if (token == "." || token == "") {
			continue;
		}
		if (token == "..") {
			if (stack.empty()) {
				continue;
			}
			stack.pop_back();
		}
		else
			stack.push_back(token);
	}
	std::string norm_path = "";
	if (req_path[0] == '/') norm_path == "/";

	for (size_t i = 0; i < stack.size(); ++i) {
		norm_path += stack[i];
		if (i < stack.size() - 1)
			norm_path += '/';
	}

	return norm_path;
}

ParseResult ParseRequest::parseReqLineHeaders(std::string &reqNoBody, HttpRequest &req) {
	std::string				_current_line;

	
	_current_line = trimToken(reqNoBody, EOL);
	if (parseFirstLine(_current_line, req) == UrlTooLong)
		return UrlTooLong;
	
	parseHeaders(reqNoBody, req);
	req.setPath(normalizePath(req.getPath()));
	return Okay;
}

ParseRequest::ParseRequest() {}

ParseRequest::~ParseRequest() {}

ParseRequest::ParseRequest(const ParseRequest &other) { *this = other; }
ParseRequest &ParseRequest::operator=(const ParseRequest &other) {
	(void)other;
	return *this;
}
