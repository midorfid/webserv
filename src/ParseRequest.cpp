#include "ParseRequest.hpp"
#include "StringUtils.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <climits>
#include <cstring>
#include "log.hpp"

#define	EOL	"\r\n"
#define	EOH	"\r\n\r\n"
#define SPACE ' '
#define MAX_URL_LENGTH 2048

static inline std::string_view trimLeftWhitespaceSV(std::string_view s) {
    size_t start = s.find_first_not_of(" \r\t\n\f\v");
    return (start == std::string_view::npos) ? "" : s.substr(start);
}

static inline std::string_view trimRightWhitespaceSV(std::string_view s) {
    size_t end = s.find_last_not_of(" \r\t\n\f\v");
    return (end == std::string_view::npos) ? "" : s.substr(0, end + 1);
}

static inline std::string_view trimWhitespacesSV(std::string_view s) {
    return trimRightWhitespaceSV(trimLeftWhitespaceSV(s));
}

std::vector<std::string_view> ParseRequest::tokenizeFirstLine(std::string_view first_line) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    while (start < first_line.length()) {
        while (start < first_line.length() && first_line[start] == ' ') start++;
        if (start >= first_line.length()) break;
        size_t end = first_line.find(' ', start);
        if (end == std::string_view::npos) end = first_line.length();
        tokens.push_back(first_line.substr(start, end - start));
        start = end;
    }
    return tokens;
}

void		ParseRequest::parseMethod(std::string_view method, HttpRequest &req) {
	req.setMethod(std::string(method));
}

void		ParseRequest::parsePathAndQuery(std::string_view pathAndQuery, HttpRequest &req) {
    size_t pos = pathAndQuery.find('?');
    if (pos != std::string_view::npos) {
        req.setPath(std::string(pathAndQuery.substr(0, pos)));
        req.setQuery(std::string(pathAndQuery.substr(pos + 1)));
    } else {
        req.setPath(std::string(pathAndQuery));
        req.setQuery("");
    }
}

void		ParseRequest::parseHttpVer(std::string_view httpVer, HttpRequest &req) {
	req.setVersion(std::string(httpVer));
}

std::string_view	ParseRequest::getNextLine(std::string_view &request) {
    size_t pos = request.find("\r\n");
    if (pos == std::string_view::npos) {
        std::string_view line = request;
        request = std::string_view();
        return line;
    }
    std::string_view line = request.substr(0, pos);
    request.remove_prefix(pos + 2); // advance past \r\n
    return line;
}

bool	ParseRequest::hasUnderscore(std::string_view s) const {
	return s.find('_') != std::string_view::npos;
}

ParseResult		ParseRequest::parseHeaders(std::string_view request, HttpRequest &req) {
	while(!request.empty()) {
		std::string_view header_line = getNextLine(request);
		if (header_line.empty())
			break;
		size_t delim_pos = header_line.find(':');
		if (delim_pos == std::string_view::npos)
			break;
		std::string_view key_view = header_line.substr(0, delim_pos);
		if (hasUnderscore(key_view))
			continue;
		std::string key(trimWhitespacesSV(key_view));
		std::transform(key.begin(), key.end(), key.begin(), ::tolower);
		std::string value(trimWhitespacesSV(header_line.substr(delim_pos + 1)));
		if (!req.addHeader(key, value))
			return BadRequest;
	}
	return Okay;
}

ParseResult		ParseRequest::parseFirstLine(std::string_view _current_line, HttpRequest &req) {
	std::vector<std::string_view> first_line = tokenizeFirstLine(_current_line);

    if (first_line.size() < 3) return Error;

	parseMethod(first_line[0], req);
	if (first_line[1].length() > MAX_URL_LENGTH)
		return UrlTooLong;
	parsePathAndQuery(first_line[1], req);
	parseHttpVer(first_line[2], req);
	return Okay;
}

ParseResult	ParseRequest::parseBody(std::string_view reqOnlyBody, HttpRequest &req) {
    if (req.getHeader("transfer-encoding") == "chunked") {
        std::string decoded_body;
        size_t pos = 0;
        
        while (pos < reqOnlyBody.length()) {
            size_t crlf = reqOnlyBody.find("\r\n", pos);
            if (crlf == std::string_view::npos) return RequestIncomplete;
            
            std::string_view hex_len = reqOnlyBody.substr(pos, crlf - pos);
            size_t chunk_len = 0;
            try { 
                chunk_len = std::stoull(std::string(hex_len), nullptr, 16); 
            } catch(...) { 
                return Error; 
            }
            
            if (chunk_len == 0) { // Terminal chunk
                req.setBody(decoded_body);
                return RequestComplete;
            }
            
            if (crlf + 2 + chunk_len + 2 > reqOnlyBody.length()) return RequestIncomplete;
            
            decoded_body.append(reqOnlyBody.substr(crlf + 2, chunk_len));
            pos = crlf + 2 + chunk_len + 2;
        }
        return RequestIncomplete;
    }

    size_t body_size = 0;
    try { body_size = std::strtoul(req.getHeader("content-length").c_str(), NULL, 10); } catch(...) {}
    
    if (body_size == 0) return RequestComplete;
    if (reqOnlyBody.empty()) return RequestIncomplete;
    
    if (body_size > 0 && reqOnlyBody.length() < body_size) {
        return RequestIncomplete; // Wait for full buffer to arrive
    }
    
    req.setBody(std::string(reqOnlyBody));

	return RequestComplete;
}


ParseResult ParseRequest::parseReqLineHeaders(std::string_view reqNoBody, HttpRequest &req) {
	std::string_view		_current_line;

	
	_current_line = getNextLine(reqNoBody);
	if (parseFirstLine(_current_line, req) == UrlTooLong)
		return UrlTooLong;
	return parseHeaders(reqNoBody, req);
}

ParseRequest::ParseRequest() {}

ParseRequest::~ParseRequest() {}

ParseRequest::ParseRequest(const ParseRequest &other) { *this = other; }
ParseRequest &ParseRequest::operator=(const ParseRequest &other) {
	(void)other;
	return *this;
}
