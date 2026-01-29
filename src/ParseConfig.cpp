#include "ParseConfig.hpp"
#include "StringUtils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <sys/stat.h>
#include <cstring>
#include "log.hpp"

void	ParseConfig::syntaxCheck() {
	if (getNextToken().value == "server") {
		if (getNextToken().value == "{") {
			return;
		} else {
			std::cerr << "Error: Expected '{' after 'server' directive. On line " << peekNextToken().line << std::endl;
			throw std::runtime_error("wrong syntax in configuration file.");
		}
	} else {
		std::cerr << "Error: Configuration file must start with 'server' directive. On line " << peekNextToken().line << std::endl;
		throw std::runtime_error("wrong syntax in configuration file.");
	}
}

void ParseConfig::parse(const std::string &cfg_path, Config &config) {
	tokenize(cfg_path);
	
	syntaxCheck();
	parseBlock(config);
}

// void		ParseConfig::parseServers() {
// 	Config	new_server_cfg;


// 	// TODO pushback to server cfgs vector
// }

void	ParseConfig::parseLimitExceptB(Location	&loc) {
	for (std::string meth = getNextToken().value; meth != "{"; meth = getNextToken().value) {
		HttpMethod hm = StringUtils::stringToMethod(meth);
		if (StringUtils::stringToMethod(meth) != UNKNOWN)
			loc.addLimitExceptMethod(hm);
		else {
			logTime(ERRLOG);
			std::cerr << "Wrong method in limit_except block in the config. Ignoring '" << meth << "'." << std::endl; 
		}
	}
	for (std::string access = getNextToken().value; access != "{"; access = getNextToken().value) {
		std::string accessVal = getNextToken().value;
		accessVal.erase(--accessVal.end()); // erase semicolon
		loc.addLimitExceptRule(access, accessVal);
	}
}
void	ParseConfig::parseLocationBlock(Config &config) {
	std::string key;
	Location	&loc = config.getNewLocation();
	
	loc.setPath(getNextToken().value);
	if (getNextToken().value != "{")
		throw std::runtime_error("wrong syntax in configuration file.");
	for (;;) {
		key = getNextToken().value;

		if (key == "}")
			break;
		else if (key == "error_page") {
			std::vector<std::string>	error_code;
			while (1) {
				error_code.push_back(getNextToken().value);
				if (error_code.back()[error_code.back().length()-1] == ';') {
					error_code.back().erase(error_code.back().length()-1);
					break;
				}
			}
			std::string		error_file = error_code.back();
			error_code.pop_back();
			while (!error_code.empty()) {
				loc.setErrorPage(error_code.back(), error_file);
				error_code.pop_back();
			}
		}
		else if (key == "limit_except") {
			parseLimitExceptB(loc);
		}
		else {
			std::vector<std::string>	value;
			while (1) {
				value.push_back(getNextToken().value);
				if (value.back()[value.back().length()-1] == ';') {
					value.back().erase(value.back().length()-1);
					break;
				}
			}
			if (value.size() != 1)
				loc.setMultiDirective(key, value);
			else
				loc.setDirective(key, value[0]);
		}
	}
}

void ParseConfig::parseBlock(AConfigBlock &block) {
	std::string							key;

	for (;;) {
		int i = 0;

		key = getNextToken().value;

		if (key == "}")
			return;
		if (key == "location") {
			Config	*serv_cfg = dynamic_cast<Config*>(&block);
			if (serv_cfg == NULL) {
				throw std::runtime_error("config error. location block not in the server directive.");
			}
			std::string loc_path = peekNextToken().value;
			if (serv_cfg->checkIfDuplicate(loc_path) == true) {
				throw std::runtime_error("config error. duplicate location.");
			}
			
			parseLocationBlock(*serv_cfg);
		}
		else if (key == "limit_except")
			throw std::runtime_error("limit_except in the server block");
		else if (key == "error_page") {
			std::vector<std::string>	error_code;
			while (1) {
				error_code.push_back(getNextToken().value);
				if (error_code.back()[error_code.back().length()-1] == ';') {
					error_code.back().erase(error_code.back().length()-1);
					break;
				}
			}
			std::string		error_file = error_code.back();
			error_code.pop_back();
			while (!error_code.empty()) {
				block.setErrorPage(error_code.back(), error_file);
				error_code.pop_back();
			}
		}
		else if (key == "keepalive_timeout") {
			std::string val = getNextToken().value;
			static_cast<Config&>(block).setKeepAliveTimer(static_cast<int>(std::strtol(val.erase(val.length()-1).c_str(), NULL, 10)));
		}
		else if (key == "client_max_body_size") {
			std::string val = getNextToken().value;
			static_cast<Config&>(block).setMaxBodySize(static_cast<int>(std::strtol(val.erase(val.length()-1).c_str(), NULL, 10)));
		}
		else {
			// std::cout << key << std::endl;
			std::vector<std::string>	value;
			while (1) {
				value.push_back(getNextToken().value);
				if (value.back()[value.back().length()-1] == ';') {
					value.back().erase(value.back().length()-1);
					break;
				}
			}
			if (value.size() != 1)
				block.setMultiDirective(key, value);
			else
				block.setDirective(key, value[0]);
		}
		++i;
	}
}

ParseConfig::ParseConfig() {}

ParseConfig::~ParseConfig() {}

ParseConfig::ParseConfig(const ParseConfig &other) {
	*this = other;
}

ParseConfig &ParseConfig::operator=(const ParseConfig &other) {
	if (this != &other) {
		AParser::operator=(other);
	}
	return *this;
}
