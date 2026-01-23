#include "StringUtils.hpp"
#include <sstream>

std::vector<std::string>    StringUtils::split(const std::string &s, char delimiter) {
    std::vector<std::string>    tokens;
    std::string                 token;
    std::istringstream          tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

std::vector<std::string>    StringUtils::extractSubVecOfStr(const std::vector<std::string> &src) {
	std::vector<std::string>    sub_vec;
	size_t                      open_braces;
	size_t                      close_braces;

	open_braces = 0;
	close_braces = 0;
	for (size_t i = 0; i < src.size(); i++) {
		if (src.at(i) == "{") {
			open_braces++;
		} else if (src.at(i) == "}") {
			close_braces++;
		}
		sub_vec.push_back(src.at(i));
		if (open_braces > 0 && open_braces == close_braces) {
			break ;
		}
	}
	return sub_vec;
}

std::string		StringUtils::myItoa(int input) {
    std::stringstream res;
    res << input;
    return res.str();
}

HttpMethod	StringUtils::stringToMethod(const std::string &str) {
	static std::map<std::string, HttpMethod> m;

    if (m.empty()) {
	    m["GET"] = M_GET;
	    m["PUT"] = M_PUT;
	    m["POST"] = M_POST;
	    m["DELETE"] = M_DELETE;
    }

    if (m.count(str))
        return m[str];
    
    return UNKNOWN;
}
