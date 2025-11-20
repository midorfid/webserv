#include "AConfigBlock.hpp"
#include "StringUtils.hpp"
#include <iostream>

bool AConfigBlock::getDirective(const std::string &key, std::string &out_val) const {
	std::map<std::string, std::string>::const_iterator	it;

	it = _directives.find(key);
	if (it == _directives.end())
		return false;
	out_val = it->second;
	return true;
}

void AConfigBlock::setDirective(const std::string &key, const std::string &value) {
	if (_directives.find(key) != _directives.end())
		return ;
	_directives[key] = value;
}

bool AConfigBlock::getMultiDirective(const std::string &key, std::vector<std::string> &out_val) const {
	std::map<std::string, std::vector<std::string> >::const_iterator	it;

	it = _multiDirectives.find(key);
	if (it == _multiDirectives.end())
		return false;
	out_val = it->second;
	return true;
}

bool AConfigBlock::isAutoindexOn() const {
	std::string Boolean;

	getDirective("autoindex", Boolean);
	return (Boolean == "on") ? true : false; 
}

bool AConfigBlock::getIndexes(std::vector<std::string> &out_val) const {
	return getMultiDirective("index", out_val);
}

bool AConfigBlock::getIndex(std::string &out_val) const {
	return getDirective("index", out_val);
}

void AConfigBlock::setMultiDirective(const std::string &key, const std::vector<std::string> &value) {
	if (_multiDirectives.find(key) != _multiDirectives.end())
		return ;
	_multiDirectives[key] = value;
}

void		AConfigBlock::setErrorPage(const std::string &error_code, const std::string &file) {
	if (_error_pages.find(error_code) != _error_pages.end())
		return ;
	_error_pages[error_code] = file;
}

bool		AConfigBlock::getErrorPage(int error_code, std::string &out_val) const {
	std::map<std::string, std::string>::const_iterator	it;

	std::string error_code_str = StringUtils::myItoa(error_code);
	it = _error_pages.find(error_code_str);
	if (it == _error_pages.end())
		return false;
	out_val = it->second;
	return true;
}

void
AConfigBlock::setCgi() {
	std::map<std::string, std::string>::const_iterator	it;

	it = _directives.find("allow_cgi");
	if (it == _directives.end()) {
		_allow_cgi = false;
		_cgi_format = "";
	}
	else {
		_allow_cgi = true;
		_cgi_format = it->second;
	}
}

bool
AConfigBlock::isCgiAllowed() const {
	return _allow_cgi;
}

const std::string &
AConfigBlock::getCgiFormat() const{
	return _cgi_format;
}
