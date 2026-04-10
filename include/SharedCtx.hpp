#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

struct SharedContext {
    std::string root;
    std::vector<std::string> index_files;
    bool autoindex;

    size_t client_max_body_size;
    int _keepalive_timer;
    bool allow_cgi;

    std::unordered_map<int, std::string> error_pages;
    std::optional<std::pair<int, std::string>> redirect;

    SharedContext()
        : root("/var/www/html"),
          autoindex(false),
          client_max_body_size(1048576),
          allow_cgi(false)
    {}
};
