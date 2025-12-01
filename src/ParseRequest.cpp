#include "ParseRequest.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <stdlib.h>
#include <sstream>
#include <algorithm>
#include "log.hpp"

#define	EOL	"\r\n"
#define	EOH	"\r\n\r\n"
#define SPACE ' '

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

void		ParseRequest::parseFirstLine(std::string &_current_line, HttpRequest &req) {
	std::vector<std::string> first_line = tokenizeFirstLine(_current_line);

	parseMethod(first_line[0], req);
	parsePathAndQuery(first_line[1], req);
	parseHttpVer(first_line[2], req);
}

ParseRequest::BodyState	ParseRequest::parseBody(size_t eoh_pos, const std::string &raw_request, HttpRequest &req) {
	size_t	body_size = 0;
	size_t	headers_size = eoh_pos + 4;
	const std::string &method = req.getMethod();

	if (method == "GET" || method == "DELETE" || method == "HEAD") {
		return BodyNotSent;
	}
	logTime(REGLOG);
	std::cout << "parsebodyqq\n" << std::endl;
	try
	{
		body_size = std::atoi(req.getHeader("content-length").c_str());
	}
	catch(const std::exception& e)
	{
		std::cerr << e.what() << '\n';
		return BodyError;
	}
	if (body_size > 0 && raw_request.size() < headers_size + body_size) {
		return BodyIncomplete;
	}
	req.setBody(raw_request.substr(headers_size, body_size));// +1 ?
	return BodySent;
}

ParseRequest::ParseResult ParseRequest::parse(const std::string &raw_request, HttpRequest &req) {
	std::string				_current_line;
	std::string				reqNoBody;
	size_t					eoh_pos;

	eoh_pos = raw_request.find(EOH);
	if (eoh_pos == raw_request.npos)
		return ParsingIncomplete;
	reqNoBody = raw_request.substr(0, eoh_pos + 2);
	
	_current_line = trimToken(reqNoBody, EOL);
	parseFirstLine(_current_line, req);
	
	parseHeaders(reqNoBody, req);

	if (parseBody(eoh_pos, raw_request, req) == BodyIncomplete)
		return ParsingIncomplete;
	
	return ParsingComplete;
}

ParseRequest::ParseRequest() {}

ParseRequest::~ParseRequest() {}

ParseRequest::ParseRequest(const ParseRequest &other) { *this = other; }
ParseRequest &ParseRequest::operator=(const ParseRequest &other) {
	(void)other;
	return *this;
}
