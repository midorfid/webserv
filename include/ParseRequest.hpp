#pragma once

#include <string>
#include <map>
#include "httpRequest.hpp"
#include "Config.hpp"
#include "AParser.hpp"

class ParseRequest {
	public:

		enum ParseResult {
			ParsingError,
			ParsingComplete,
			ParsingIncomplete,
		};

		enum FirstLineState {
			UrlTooLong,
			AllGood,
		};

		enum BodyState {
			BodyNotSent,
			BodySent,
			BodyIncomplete,
			BodyError, // no content-length header
		};

        ParseRequest();
        ~ParseRequest();

		ParseRequest(const ParseRequest &other);
		ParseRequest &operator=(const ParseRequest &other);

		ParseResult		parse(const std::string &raw_request, HttpRequest &req);

	private:
		std::string						getNextLine(std::string &request);
		std::vector<std::string>    	tokenizeFirstLine(const std::string &first_line);
		FirstLineState					parseFirstLine(std::string &_current_line, HttpRequest &req);
		template <typename T>
		std::string						trimToken(std::string &src, T token);
		void							trimLeftWhitespace(std::string &to_trim);
		
		void							parseMethod(std::string &first_line, HttpRequest &req);
		void							parsePathAndQuery(std::string &line_remainder, HttpRequest &req);
		void							parseHttpVer(std::string &line_remainder, HttpRequest &req);
		void							parseHeaders(std::string &line, HttpRequest &req);
		BodyState						parseBody(size_t eoh_pos, const std::string &raw_request, HttpRequest &req);
		bool							hasUnderscore(const std::string &s) const;
};	
