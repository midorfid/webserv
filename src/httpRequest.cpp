#include "httpRequest.hpp"

const std::string	&HttpRequest::getPath() const {
	return this->_path;
}
void				HttpRequest::setPath(const std::string &path) {
	this->_path = path;
}

const std::string	&HttpRequest::getMethod() const { return this->_method;}
const std::string	&HttpRequest::getVersion() const { return this->_http_ver;}
const std::string	&HttpRequest::getQuery() const { return this->_query_str;}
std::string			HttpRequest::getHeader(const std::string &key) const {
	std::map<std::string,std::string>::const_iterator it = _headers.find(key);
	
	if (it != _headers.end())
		return it->second;

	return "";
}
const std::string	&HttpRequest::getBody() const { return this->_body;}


void	HttpRequest::setMethod(const std::string &method) { this->_method = method;}
void	HttpRequest::setVersion(const std::string &version) { this->_http_ver = version;}
void	HttpRequest::setQuery(const std::string &query) { this->_query_str = query;}
void	HttpRequest::setBody(const std::string &body) { this->_body = body;}
void	HttpRequest::addHeader(const std::string &key, const std::string &value) {
	std::map<std::string, std::string>::const_iterator	it;

	it = this->_headers.find(key);
	if (it == this->_headers.end()) {
		this->_headers[key] = value;
	}
	return;
}

HttpRequest::HttpRequest() : _method(""), _path(""), _query_str(""), _http_ver(""), _body(""), _headers() {}

HttpRequest::~HttpRequest() {}

HttpRequest::HttpRequest(const HttpRequest &other) {
	*this = other;
}

HttpRequest &HttpRequest::operator=(const HttpRequest &other) {
	if (this != &other) {
		this->_method = other._method;
		this->_path = other._path;
		this->_query_str = other._query_str;
		this->_http_ver = other._http_ver;
		this->_body = other._body;
		this->_headers = other._headers;
	}
	return *this;
}

const std::map<std::string, std::string>	&
HttpRequest::headers() const {
	return _headers;
}

