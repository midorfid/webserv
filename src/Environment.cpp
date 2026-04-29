#include "Environment.hpp"
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include "server.hpp"
#include "httpRequest.hpp"
#include <iostream>

size_t Environment::_parent_env_size = 0;
char **Environment::_parent_env = NULL;

Environment::Environment(const HttpRequest &req, const Location &loc, const std::string &client_ip) :
    _req(req),
    _loc(loc),
    _client_ip(client_ip),
    _cenv(NULL) {}

Environment::~Environment() {
    delete[] _cenv;
}

Environment::Environment(const Environment &other) :
    _req(other._req),
    _loc(other._loc),
    _client_ip(other._client_ip),
    _cenv(NULL) // shallow copy
{ *this = other; }

Environment &
Environment::operator=(const Environment &other) {
    if (this != &other) {
        _vsenv = other._vsenv;
    }
    return *this;
}

void
Environment::build(const std::string &target_path) {
    _vsenv.reserve(dfl_size + _req.headers().size());

    append("REQUEST_METHOD", _req.getMethod());
    append("PATH_INFO", _req.getPath());
    append("PATH_TRANSLATED", target_path);
    append("SCRIPT_NAME", _req.getPath());
    append("QUERY_STRING", _req.getQuery());
    append("SERVER_PROTOCOL", _req.getVersion());
    append("REMOTE_ADDR", _client_ip);
    try {
        const std::string &ct = _req.getHeader("content-type");
        append("CONTENT_TYPE", ct);
        // Bare MIME type without parameters (e.g. "text/plain" from "text/plain; charset=utf-8")
        size_t semi = ct.find(';');
        std::string mime = (semi != std::string::npos) ? ct.substr(0, semi) : ct;
        size_t tail = mime.find_last_not_of(" \t");
        if (tail != std::string::npos) mime.erase(tail + 1);
        append("MIME_TYPE", mime);
    } catch (std::out_of_range &) {}
    try {
        append("CONTENT_LENGTH", _req.getHeader("content-length"));
    } catch (std::out_of_range &) {}

    append(_req.headers());

    _build_cenv();
}

void
Environment::_build_cenv() {
    size_t const total_size = _parent_env_size + static_size + _vsenv.size();
    // delete before?
    _cenv = new char*[total_size + 1];
    
    char ** const &parent_offset = _cenv + _parent_env_size;
    char ** const &static_offset = parent_offset + static_size;
    
    memmove(_cenv, _parent_env, _parent_env_size * sizeof(char *));
    memmove(parent_offset, static_env, static_size * sizeof(char *));

    std::vector<std::string>::iterator it = _vsenv.begin();
    for (size_t i = 0; it != _vsenv.end(); ++i, ++it) {
        std::string &str = *it;
        static_offset[i] = const_cast<char*>(str.c_str()); //TODO mb try to remove const_cast alter
    }

    _cenv[total_size] = nullptr;
}

void
Environment::init_env(char **envp) {
    _parent_env = envp;

    if (envp) {
        while (envp[_parent_env_size])
            ++_parent_env_size;
    }
}

void
Environment::append(const std::string &key, const std::string &val) {
    _vsenv.push_back(env_str(key, val));
}

void
Environment::append(const std::map<std::string, std::string> &hdrs) {
    std::string         key;

    for (std::map<std::string, std::string>::const_iterator it = hdrs.begin(); it != hdrs.end(); ++it) {
        key = it->first;
        std::transform(key.begin(), key.end(), key.begin(), trans_char);
        if (key == "CONTENT_TYPE" || key == "CONTENT_LENGTH")
            continue;
        key = "HTTP_" + key;
        _vsenv.push_back(env_str(key, it->second));
    }
}

char
Environment::trans_char(char c) {
    return (c == '-') ? '_' : std::toupper(c); 
}

std::string
Environment::env_str(const std::string &key, const std::string &val) {
    return key + '=' + val;
}

char * const*
Environment::getEnvp() const {
    char * const* envp = const_cast<char**>(_cenv);
    return  envp;
}
