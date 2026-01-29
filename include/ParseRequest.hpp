#pragma once

#include <string>
#include <map>
#include "httpRequest.hpp"
#include "Config.hpp"
#include "AParser.hpp"

enum ParseResult {
	UrlTooLong,
	HeadersTooLarge,
	BodyTooLarge,
	RequestComplete,
	RequestIncomplete,
	Error,
	NothingToRead,
	Okay,
};

class ParseRequest {
	public:

        ParseRequest();
        ~ParseRequest();

		ParseRequest(const ParseRequest &other);
		ParseRequest &operator=(const ParseRequest &other);

		ParseResult						parseReqLineHeaders(std::string &reqNoBody, HttpRequest &req);
		ParseResult						parseBody(std::string &reqOnlyBody, HttpRequest &req);

	private:
		std::string						getNextLine(std::string &request);
		std::vector<std::string>    	tokenizeFirstLine(const std::string &first_line);
		ParseResult						parseFirstLine(std::string &_current_line, HttpRequest &req);
		template <typename T>
		std::string						trimToken(std::string &src, T token);
		void							trimLeftWhitespace(std::string &to_trim);
		
		void							parseMethod(std::string &first_line, HttpRequest &req);
		void							parsePathAndQuery(std::string &line_remainder, HttpRequest &req);
		void							parseHttpVer(std::string &line_remainder, HttpRequest &req);
		void							parseHeaders(std::string &line, HttpRequest &req);
		bool							hasUnderscore(const std::string &s) const;
};	
