#include "Location.hpp"
#include "StringUtils.hpp"
#include <iostream>
#include "log.hpp"

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

void Location::addLimitExceptMethod(const std::string &method) {
	_rules.
}

void		Location::addLimitExceptRules(const std::string &key, const std::string &value) {
	if (key == "method")
		this->_rules._methods += ' ' + value;
	else if (key == "allow")
		this->_rules._allow += ' ' + value;
	else if (key == "deny")
		this->_rules._deny += ' ' + value;
	else {
		logTime(ERRLOG);
		std::cerr << "WARNING.Unknown rule in limit_except. Continuing...\n";
	}
	return ;
}