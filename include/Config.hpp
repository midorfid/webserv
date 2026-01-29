#pragma once

#include <string>
#include <map>
#include <vector>
#include "Location.hpp"

class Config : public AConfigBlock {
	public:

		Config();
		~Config();

		Config(const Config &other);
		Config &operator=(const Config &other);

		const std::vector<Location>		&getLocations() const;
		bool   							checkIfDuplicate(const std::string &path) const;
		bool							getPort(const std::string &key, std::string &out_val) const;
		int								getKeepAliveTimer(void) const;
		int								getMaxBodySize() const;

		void						setMaxBodySize(int);
		void						setLocCgi();
		bool						getErrorPage(int code, std::string &errorPage) const;
		Location					&getNewLocation();
		void						setKeepAliveTimer(int);

	private:

		void				    					setError_page(const std::string &value);
		
    	std::map<int, std::string>			_error_pages;
		std::vector<Location>				_locations;
		int									_port;
		int									_keepalive_timer;
};
