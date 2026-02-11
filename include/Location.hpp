#pragma once

#include <string>
#include <map>
#include "AConfigBlock.hpp"
#include "StringUtils.hpp"

class Location : public AConfigBlock {
	public:

		Location() : _path(""), _rules(), _hasRedirect(false), _redirectCode(0) {}
		~Location() {}
		Location(const Location &other) : AConfigBlock(other), _path(other._path), _rules(other._rules),
				_hasRedirect(other._hasRedirect), _redirectCode(other._redirectCode), _redirectURL(other._redirectURL) {}
		Location &operator=(const Location &other) {
			if (this != &other) {
				AConfigBlock::operator=(other);
				_path = other._path;
				_rules = other._rules;
				_hasRedirect = other._hasRedirect;
				_redirectCode = other._redirectCode;
				_redirectURL = other._redirectURL;
			}
			return *this;
		}

		const std::string			&getPath() const { return this->_path; }
		void						setPath(const std::string &path) { this->_path = path; }

		void						addLimitExceptRule(const std::string &key, const std::string &value);
		void						addLimitExceptMethod(const HttpMethod &methBit);


		bool						checkLimExceptAccess(const std::string &meth, const std::string &ip) const;

		bool							hasRedirect() const;
		void							setRedirect(int, const std::string &);
		std::pair<int, std::string>		getRedirect() const;
		// void						_setDirective(const std::string &key, const std::string &value);
        // void						_setMultiDirective(const std::string &key, const std::vector<std::string> &value);
		// void						_setErrorPage(const std::string &error_code, const std::string &file);
		// void						_addLimitExceptRules(const std::string &key, const std::string &value);

	private:

		std::string							_path;
		limitExceptRules					_rules;

		bool								_hasRedirect;
		int									_redirectCode;
		std::string							_redirectURL;
};
