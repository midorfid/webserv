#pragma once

#include <string>
#include "Config.hpp"
#include "ServerConfig.hpp"
#include "AParser.hpp"

class ParseConfig : public AParser {
	public:

		ParseConfig();
		~ParseConfig();

		ParseConfig(const ParseConfig &other);
		ParseConfig &operator=(const ParseConfig &other);

		void parse(const std::string &path, Config &config);

	private:
	
		void										finalizeLocations(std::vector<Location> &loc);
		void										parseServers();
		void										parseBlock(AConfigBlock &block);
		int											checkPath(const std::string &path); // return TODO
		void										setErrorPage(std::string &error_code, const std::string &file);
		void										syntaxCheck();
		std::pair<std::string, std::string>			parseLocDirectives(std::vector<std::string> &tokens);
		void										parseLocationBlock(Config &config);
};
