#include "StringUtils.hpp"
#include <sstream>

template <>
std::vector<std::string>    StringUtils::split<char>(const std::string &s, char delimiter) {
    std::vector<std::string>    tokens;
    std::string                 token;
    std::istringstream          tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

template <>
std::vector<std::string>    StringUtils::split<std::string>(const std::string &s, std::string delimiter) {
    std::vector<std::string>    tokens;
    std::string                 token;
    std::istringstream          tokenStream(s);

    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }

    return tokens;
}

void		StringUtils::trimLeftWhitespace(std::string &to_trim) {
	const std::string WHITESPACE = " \r\t\n\f\v";
	size_t start = to_trim.find_first_not_of(WHITESPACE);

	if (start == to_trim.npos)
		to_trim.clear();
	else {
		to_trim.erase(0, start);
	}
}


void		StringUtils::trimRightWhitespace(std::string &to_trim) {
	const std::string WHITESPACE = " \r\t\n\f\v";
	size_t start = to_trim.find_last_not_of(WHITESPACE);

	if (start == to_trim.npos)
		to_trim.clear();
	else {
		to_trim.erase(start);
	}
}

void		StringUtils::trimWhitespaces(std::string &to_trim) {
	StringUtils::trimLeftWhitespace(to_trim);
	StringUtils::trimRightWhitespace(to_trim);
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
