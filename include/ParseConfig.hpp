#pragma once

#include <string>
#include <vector>
#include <string_view>
#include "Config.hpp"
#include "lexer.h"

class ParseConfig {
public:
    ParseConfig() = default;
    ~ParseConfig() = default;
    ParseConfig(const ParseConfig &other) = default;
    ParseConfig &operator=(const ParseConfig &other) = default;

    void parse(const std::string &path, Config &config);

private:
    std::string _file_content; // Backing buffer to keep string_views valid

    void parseServerBlock(TokenStream &tokens, Config &config);
    void parseLocationBlock(TokenStream &tokens, Config &config);
    void parseLimitExceptBlock(TokenStream &tokens, Location &location);
    std::vector<std::string_view> collectArguments(TokenStream &tokens);
    void parseDirective(std::string_view name, TokenStream &tokens, SharedContext &ctx);
};
