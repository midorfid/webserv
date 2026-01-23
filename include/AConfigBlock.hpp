#pragma once

#include <string>
#include <map>
#include <vector>
#include <netinet/in.h>

struct AccessRule {
	bool	allow;

	in_addr_t	ip;
	in_addr_t	mask;
};

/*
	@param methodMask: @param GET 1 @param POST 2 @param PUT 4 @param DELETE 8
*/
struct limitExceptRules {
	unsigned int isActive : 1;
	unsigned int methodMask : 4;

	std::vector<AccessRule> rules;

	limitExceptRules() : isActive(0), methodMask(0) {}
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