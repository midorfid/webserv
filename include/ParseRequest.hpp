#pragma once

#include <string>
#include <map>
#include "httpRequest.hpp"
#include "Config.hpp"

enum ParseResult {
	UrlTooLong,
	HeadersTooLarge,
	BodyTooLarge,
	RequestComplete,
	RequestIncomplete,
	Error,
	NothingToRead,
	Okay,
	BadRequest,
	UnsupportedVersion,
};

class ParseRequest {
	public:

        ParseRequest();
        ~ParseRequest();

		ParseRequest(const ParseRequest &other);
		ParseRequest &operator=(const ParseRequest &other);

		ParseResult						parseReqLineHeaders(std::string_view reqNoBody, HttpRequest &req);
		ParseResult						parseBody(std::string_view reqOnlyBody, HttpRequest &req);

	private:
		std::string_view				getNextLine(std::string_view &request);
		std::vector<std::string_view>	tokenizeFirstLine(std::string_view first_line);
		ParseResult						parseFirstLine(std::string_view current_line, HttpRequest &req);

		void							parseMethod(std::string_view first_line, HttpRequest &req);
		void							parsePathAndQuery(std::string_view line_remainder, HttpRequest &req);
		void							parseHttpVer(std::string_view line_remainder, HttpRequest &req);
		ParseResult						parseHeaders(std::string_view line, HttpRequest &req);
		bool							hasUnderscore(std::string_view s) const;
};	
