#include "response.hpp"
#include <cstdio>

std::string
Response::mimeFromPath(const std::string &path) {
    static const std::pair<const char *, const char *> types[] = {
        {".html", "text/html; charset=utf-8"},
        {".htm",  "text/html; charset=utf-8"},
        {".css",  "text/css"},
        {".js",   "application/javascript"},
        {".json", "application/json"},
        {".txt",  "text/plain; charset=utf-8"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".ico",  "image/x-icon"},
        {".svg",  "image/svg+xml"},
        {".pdf",  "application/pdf"},
        {".py",   "text/x-python"},
    };
    size_t dot = path.rfind('.');
    if (dot != std::string::npos) {
        std::string ext = path.substr(dot);
        for (const auto &[e, mime] : types)
            if (ext == e) return mime;
    }
    return "application/octet-stream";
}

void
Response::finalizeResponse(ResponseState &resp, const std::string &path, size_t bodySize, bool isConKeepAlive, const std::string &session_cookie) {
	resp.addHeader("date", Response::getHttpDate());
	resp.addHeader("server", "Webserv/1.0");
	resp.addHeader("content-length", std::to_string(bodySize));
	if (bodySize > 0 && resp.headers.find("content-type") == resp.headers.end())
		resp.addHeader("content-type", "text/html; charset=utf-8");
	if (isConKeepAlive)
		resp.addHeader("connection", "keep-alive");
	else
		resp.addHeader("connection", "closed");
    switch(resp.status_code) {
        case 201:
            resp.addHeader("location", path);
            break;
        case 301:
        case 302:
        case 307:
        case 308:
            resp.addHeader("location", path);
            break;
        case 405:
            resp.addHeader("allow", "GET, POST, DELETE"); // TODO hardcoded, also I intend to allow PUT, not sure if the message based on location permission
            break;
        case 401:
            resp.addHeader("www-authenticate", "Basic realm=\"Access to site\"");
            break;
        case 204: // add nothing
            break;
        default:
            break;
    }
	if (!session_cookie.empty())
		resp.cookies.push_back(session_cookie);
}


std::string
Response::build(const ResponseState &resp) {
	std::stringstream		response;
	
	std::string status_text = Response::getStatusText(resp.status_code);
	response << "HTTP/1.1 " << resp.status_code << " " << status_text << "\r\n";
	for (const auto &[key, val] : resp.headers)
		response << key << ": " << val << "\r\n";
	for (const auto &cookie : resp.cookies)
		response << "Set-Cookie: " << cookie << "\r\n";
	response << "\r\n";

	if (!resp.body.empty())
		response << resp.body;

	return response.str();
}

std::string
Response::getStatusText(int code) {
	switch (code) {
	case 200: return "OK";
	case 201: return "Created";
	case 204: return "No Content";
	case 301: return "Moved Permanently";
	case 302: return "Found";
	case 307: return "Temporary Redirect";
	case 308: return "Permanent Redirect";
	case 400: return "Bad Request";
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 405: return "Method Not Allowed";
	case 408: return "Request Timeout";
	case 413: return "Payload Too Large";
	case 414: return "URI Too Long";
	case 431: return "Request Header Fields Too Large";
	case 500: return "Internal Server Error";
	case 502: return "Bad Gateway";
	case 503: return "Service Unavailable";
	case 504: return "Gateway Timeout";
	case 505: return "HTTP Version Not Supported";
	default:  return "Internal Server Error";
	}
}

void
Response::finalizeResponseChunked(ResponseState &resp, const std::string &path, bool isConKeepAlive, const std::string &session_cookie) {
    (void)path;
    resp.addHeader("date", Response::getHttpDate());
    resp.addHeader("server", "Webserv/ver 1.0");
    resp.addHeader("transfer-encoding", "chunked");
    if (isConKeepAlive)
        resp.addHeader("connection", "keep-alive");
    else
        resp.addHeader("connection", "closed");
    if (!session_cookie.empty())
        resp.cookies.push_back(session_cookie);
}

std::string
Response::encodeChunk(const std::string &data) {
    if (data.empty())
        return "";
    char hex[20];
    snprintf(hex, sizeof(hex), "%zx\r\n", data.size());
    return std::string(hex) + data + "\r\n";
}

std::string
Response::encodeChunked(const std::string &body) {
    if (body.empty())
        return "0\r\n\r\n";
    return Response::encodeChunk(body) + "0\r\n\r\n";
}

std::string
Response::getHttpDate() {
	time_t now = time(NULL);

	struct tm *gmt_time = gmtime(&now);

	char buffer[128];
	// Thu, 09 Oct 2025 09:01:45 GMT
	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt_time);

	return std::string(buffer);
}
