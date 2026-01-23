#pragma once

#include <string>
#include <vector>
#include <map>

enum HttpMethod {
    M_GET    = 1,  // 0001
    M_POST   = 2,  // 0010
    M_PUT    = 4,  // 0100
    M_DELETE = 8,   // 1000
	UNKNOWN  = 0
};

namespace StringUtils {

    std::vector<std::string>    split(const std::string &s, char delimiter);
    std::vector<std::string>    extractSubVecOfStr(const std::vector<std::string> &src);
    std::string                 myItoa(int input);
    HttpMethod                  stringToMethod(const std::string &str);
};
