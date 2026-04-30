#pragma once

#include <string>
#include <vector>
#include <string_view>
#include "Config.hpp"
#include "lexer.h"

struct GlobalConfig {
    std::string access_log;
    std::string error_log;
};

class ParseConfig {
public:
    ParseConfig() = default;
    ~ParseConfig() = default;
    ParseConfig(const ParseConfig &other) = default;
    ParseConfig &operator=(const ParseConfig &other) = default;

    std::vector<Config> parse(const std::string &path);
    const GlobalConfig& getGlobal() const { return _global; }

private:
    std::string  _file_content; // Backing buffer to keep string_views valid
    GlobalConfig _global;

    Config parseServerBlock(TokenStream &tokens);
    void parseLocationBlock(TokenStream &tokens, Config &config);
    void parseLimitExceptBlock(TokenStream &tokens, Location &location);
    std::vector<std::string_view> collectArguments(TokenStream &tokens);
    void parseDirective(std::string_view name, TokenStream &tokens, SharedContext &ctx);
};
