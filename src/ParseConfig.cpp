#include "ParseConfig.hpp"

#include <sys/stat.h>

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <errno.h>

#include "log.hpp"
#include "StringUtils.hpp"
#include "lexer.hpp"

void
ParseConfig::parse(const std::string &path, Config &config) {}

void
ParseConfig::parseServerBlock(TokenStream &tokens, Config &config) {
	
}