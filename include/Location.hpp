#pragma once

#include <string>
#include <map>
#include "AConfigBlock.hpp"

class Location : public AConfigBlock {
	public:

		Location() : _path("") {}
		~Location() {}
		Location(const Location &other) : AConfigBlock(other), _path(other._path) {}
		Location &operator=(const Location &other) {
			if (this != &other) {
				AConfigBlock::operator=(other);
				this->_path = other._path;
			}
			return *this;
		}

		const std::string			&getPath() const { return this->_path; }
		void						setPath(const std::string &path) { this->_path = path; }

		void						addLimitExceptRules(const std::string &key, const std::string &value);

		// void						_setDirective(const std::string &key, const std::string &value);
        // void						_setMultiDirective(const std::string &key, const std::vector<std::string> &value);
		// void						_setErrorPage(const std::string &error_code, const std::string &file);
		// void						_addLimitExceptRules(const std::string &key, const std::string &value);

	private:

		std::string							_path;
		limitExceptRules					_rules;

};
