#include "response.hpp"

void
Response::finalizeResponse(ResponseState &resp, const std::string &path) {
	resp.addHeader("Date", Response::getHttpDate());
	resp.addHeader("Server", "Webserv/ver 1.0");

    switch(resp.status_code) {
        case 201:
            resp.addHeader("Location", path);
            break;
        case 301:
        case 302:
        case 307:
        case 308:
            resp.addHeader("Location", path);
            break;
        case 405:
            resp.addHeader("Allow", "GET, POST, DELETE"); // TODO hardcoded, also I intend to allow PUT, not sure if the message based on location permission
            break;
        case 401:
            resp.addHeader("WWW-Authenticate", "Basic realm=\"Access to site\"");
            break;
        case 204: // add nothing
            break;
        default:
            break;
    }
}


std::string
Response::build(const ResponseState &resp) {
	std::stringstream		response;
	
	std::string status_text = Response::getStatusText(resp.status_code);
	response << "HTTP/1.1 " << resp.status_code << " " << status_text << "\r\n";
	for (std::vector<std::pair<std::string, std::string> >::const_iterator it = resp.headers.begin(); it != resp.headers.end(); ++it) {
		response << it->first << ": " << it->second << "\r\n";
	}
	response << "\r\n";

	if (!resp.body.empty())
		response << resp.body;

	return response.str();
}

std::string
Response::getStatusText(int code) {
	switch (code) {
	case 403: return "Forbidden";
	case 404: return "Not Found";
	case 500: return "Internal Server Error";
	case 200: return "OK";
	case 400: return "Bad Request";
	case 405: return "Method Not Allowed";
	case 408: return "Request Timeout";
	case 413: return "Payload Too Large";
	case 414: return "URl Too Long";
	case 301: return "Moved Permanently";
	default: return "Error";
	}
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
