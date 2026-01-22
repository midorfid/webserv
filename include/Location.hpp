#pragma once

#include <string>
#include <map>
#include "AConfigBlock.hpp"

class Location : public AConfigBlock {
	public:

		Location() : _path(""), _rules() {}
		~Location() {}
		Location(const Location &other) : AConfigBlock(other), _path(other._path), _rules(other._rules) {}
		Location &operator=(const Location &other) {
			if (this != &other) {
				AConfigBlock::operator=(other);
				this->_path = other._path;
				_rules = other._rules;
			}
			return *this;
		}

		const std::string			&getPath() const { return this->_path; }
		void						setPath(const std::string &path) { this->_path = path; }

		void						addLimitExceptRule(const std::string &key, const std::string &value);
		void						addLimitExceptMethod(const std::string &method);


		const std::string			&getRulesMethods() const { return _rules._methods; }
		const std::string			&getRulesAllowed() const { return _rules._allow; }
		const std::string			&getRulesDenied() const { return _rules._deny; }

		// void						_setDirective(const std::string &key, const std::string &value);
        // void						_setMultiDirective(const std::string &key, const std::vector<std::string> &value);
		// void						_setErrorPage(const std::string &error_code, const std::string &file);
		// void						_addLimitExceptRules(const std::string &key, const std::string &value);

	private:

		std::string							_path;
		limitExceptRules					_rules;

};
