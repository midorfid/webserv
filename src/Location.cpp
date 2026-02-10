#include "Location.hpp"
#include "StringUtils.hpp"
#include <iostream>
#include "log.hpp"
#include <arpa/inet.h>
#include <sstream>

// void						Location::_setDirective(const std::string &key, const std::string &value) {
	// AConfigBlock::setDirective(key, value);
// }
// void						Location::_setMultiDirective(const std::string &key, const std::vector<std::string> &value) {
	// AConfigBlock::setMultiDirective(key, value);
// }
// void						Location::_setErrorPage(const std::string &error_code, const std::string &file) {
	// AConfigBlock::setErrorPage(error_code, file);
// }
// void						Location::_addLimitExceptRules(const std::string &key, const std::string &value) {
	// AConfigBlock::addLimitExceptRules(key, value);
// }	
// 


bool
Location::hasRedirect() const { return _hasRedirect;}

void
Location::setRedirect(int status_code, const std::string &url) {
	_hasRedirect = true;
	_redirectCode = status_code;
	_redirectURL = url;
}

std::pair<int, std::string>
Location::getRedirect() const {
	return std::make_pair(_redirectCode, _redirectURL);
}


void Location::addLimitExceptMethod(const HttpMethod &methBit) {
	_rules.methodMask |= methBit;
}

void		Location::addLimitExceptRule(const std::string &key, const std::string &value) {
	AccessRule	ar;
	ar.allow = (key == "allow") ? true : false;
	if (value == "all") {
		ar.ip = 0;
		ar.mask = 0;
	}
	else {
		std::vector<std::string>	vs = StringUtils::split(value, '/');
		ar.mask = inet_addr(vs.back().c_str());
		vs.pop_back();
		ar.ip = inet_addr(vs.back().c_str());
		vs.pop_back();
	}
	_rules.rules.emplace_back(ar);
	_rules.isActive = 1;
}

bool	Location::checkLimExceptAccess(const std::string &meth, const std::string &ip) const {
	if (!_rules.isActive)
		return true;
	HttpMethod hm = StringUtils::stringToMethod(meth);
	if (hm == UNKNOWN)
		return false;
	if (hm == (hm & _rules.methodMask))
		return true;
	else {
		for (std::vector<AccessRule>::const_iterator it = _rules.rules.begin(); it != _rules.rules.end(); ++it) {
			if ((inet_addr(ip.c_str()) & it->mask) == (it->mask & it->ip)) {
				return it->allow == true ? true : false;
			}
		}
	}
	return true;
}
