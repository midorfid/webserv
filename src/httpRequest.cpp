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
void	HttpRequest::appendBody(const std::string &extra_body) { this->_body += extra_body;}
bool	HttpRequest::addHeader(const std::string &key, const std::string &value) {
	// Duplicate any of these headers is either an HTTP smuggling vector or
	// ambiguous enough that RFC 7230 §3.3.2 allows rejecting with 400.
	static const std::string sensitive[] = {
		"host", "content-length", "transfer-encoding", "content-type"
	};

	auto it = this->_headers.find(key);
	if (it != this->_headers.end()) {
		for (const auto &s : sensitive)
			if (key == s) return false;
		// Non-sensitive duplicate: fold per RFC 7230 §3.2.2 (comma-join)
		it->second += ", " + value;
		return true;
	}
	this->_headers[key] = value;
	return true;
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

std::string HttpRequest::getCookie(const std::string &name) const {
	const std::string raw = getHeader("cookie");
	size_t pos = 0;
	const size_t len = raw.size();

	while (pos < len) {
		while (pos < len && raw[pos] == ' ') ++pos;

		size_t eq   = raw.find('=', pos);
		size_t semi = raw.find(';', pos);
		if (semi == std::string::npos) semi = len;

		if (eq == std::string::npos || eq >= semi) { pos = semi + 1; continue; }

		std::string_view key(raw.data() + pos, eq - pos);
		while (!key.empty() && key.back() == ' ') key.remove_suffix(1);

		if (key == name)
			return raw.substr(eq + 1, semi - eq - 1);

		pos = semi + 1;
	}
	return "";
}

const std::map<std::string, std::string>	&
HttpRequest::headers() const {
	return _headers;
}

bool
HttpRequest::isKeepAlive() const {
	const std::string &keep_alive_header = getHeader("connection");
	
	return  (keep_alive_header == "keep-alive" || keep_alive_header == "") ? true : false; // I do tolower, don't I
}
