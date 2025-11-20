#pragma once

#include <string>
#include <map>
#include <vector>

struct limitExceptRules {
	std::vector<std::string>	_methods;
	std::vector<std::string>	_allow;
	std::vector<std::string>	_deny;
};

class AConfigBlock {
    public:
		virtual ~AConfigBlock() {}

		bool						getErrorPage(int error_code, std::string &out_val) const;
		bool						getIndexes(std::vector<std::string> &out_val) const;
		bool						getIndex(std::string &out_val) const;
		bool						isAutoindexOn() const;

		bool						getDirective(const std::string &key, std::string &out_val) const;
		bool						getMultiDirective(const std::string &key, std::vector<std::string> &out_val) const;

		void								setCgi();
		bool								isCgiAllowed() const;
		const std::string					&getCgiFormat() const;

		virtual void						setDirective(const std::string &key, const std::string &value);
        virtual void						setMultiDirective(const std::string &key, const std::vector<std::string> &value);
		virtual void						setErrorPage(const std::string &error_code, const std::string &file);

	protected:

		AConfigBlock() {}
		AConfigBlock(const AConfigBlock &other) {	
			this->_error_pages = other._error_pages;
			this->_directives = other._directives;
			this->_multiDirectives = other._multiDirectives;
		}
		AConfigBlock &operator=(const AConfigBlock &other) { (void)
			other; return *this; }

		
		bool												_allow_cgi;
		std::string											_cgi_format;
		std::map<std::string, std::string>					_error_pages;
		std::map<std::string, std::string>					_directives;
		std::map<std::string, std::vector<std::string> >	_multiDirectives;
	};