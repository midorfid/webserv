#include "ParseConfig.hpp"

#include <sys/stat.h>

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <stdexcept>

#include "log.hpp"
#include "StringUtils.hpp"
#include "lexer.h"

std::vector<Config>
ParseConfig::parse(const std::string &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    _file_content = buffer.str();

    std::vector<std::string_view> tokens_vec = TokenStream::buildTokens(_file_content);
    TokenStream tokens(tokens_vec);

    std::vector<Config> vhosts;
    while (tokens.hasNext()) {
        auto token = tokens.next();
        if (token == "server") {
            vhosts.push_back(parseServerBlock(tokens));
        } else {
            throw std::runtime_error("Unexpected token in global scope: " + std::string(token));
        }
    }
    return vhosts;
}

Config
ParseConfig::parseServerBlock(TokenStream &tokens) {
    tokens.expect("{");

    Config config;
    SharedContext &server_ctx = config.getSharedCtx();

    while (tokens.hasNext() && tokens.peek() != "}") {
        auto token = tokens.peek();
        if (token == "location") {
            tokens.next(); // consume "location"
            parseLocationBlock(tokens, config);
        } else if (token == "listen") {
            tokens.next();
            auto args = collectArguments(tokens);
            if (args.size() != 1) throw std::runtime_error("listen expects 1 argument");
            config.setPort(std::stoi(std::string(args[0])));
        } else {
            tokens.next(); // consume directive
            parseDirective(token, tokens, server_ctx);
        }
    }
    tokens.expect("}");
    return config;
}

void
ParseConfig::parseLocationBlock(TokenStream &tokens, Config &config) {
    auto path = tokens.next();
    tokens.expect("{");

    Location loc;
    loc.setPath(std::string(path));
    
    // Composition: Initialize the location with a copy of the server context
    SharedContext loc_ctx = config.getSharedCtx();

    while (tokens.hasNext() && tokens.peek() != "}") {
        auto token = tokens.peek();
        if (token == "limit_except") {
            tokens.next();
            parseLimitExceptBlock(tokens, loc);
        } else {
            tokens.next();
            parseDirective(token, tokens, loc_ctx);
        }
    }
    tokens.expect("}");

    loc.setSharedCtx(loc_ctx);
    config.addLocation(loc);
}

void
ParseConfig::parseLimitExceptBlock(TokenStream &tokens, Location &location) {
    int bitmask = 0;

    // Consume method tokens before the block open: limit_except GET POST {
    while (tokens.hasNext() && tokens.peek() != "{") {
        bitmask |= StringUtils::stringToMethod(std::string(tokens.next()));
    }

    tokens.expect("{");
    while (tokens.hasNext() && tokens.peek() != "}") {
        tokens.next(); // inner deny/allow rules not yet implemented
    }
    tokens.expect("}");

    location.setLimitExcept(bitmask);
}

void
ParseConfig::parseDirective(std::string_view name, TokenStream &tokens, SharedContext &ctx) {
    auto args = collectArguments(tokens);
    if (name == "root") {
        if (args.size() != 1) throw std::runtime_error("root expects 1 argument");
        ctx.root = std::string(args[0]);
    } else if (name == "index") {
        for (auto arg : args) {
            ctx.index_files.push_back(std::string(arg));
        }
    } else if (name == "autoindex") {
        if (args.size() != 1) throw std::runtime_error("autoindex expects 1 argument");
        ctx.autoindex = (args[0] == "on");
    } else if (name == "client_max_body_size") {
        if (args.size() != 1) throw std::runtime_error("client_max_body_size expects 1 argument");
        ctx.client_max_body_size = std::stoull(std::string(args[0]));
    } else if (name == "keepalive_timeout") {
        if (args.size() != 1) throw std::runtime_error("keepalive_timeout expects 1 argument");
        ctx._keepalive_timer = std::stoi(std::string(args[0]));
    } else if (name == "error_page") {
        if (args.size() < 2) throw std::runtime_error("error_page expects at least 2 arguments");
        std::string page = std::string(args.back());
        for (size_t i = 0; i < args.size() - 1; ++i) {
            ctx.error_pages[std::stoi(std::string(args[i]))] = page;
        }
    } else if (name == "return") {
        if (args.size() != 2) throw std::runtime_error("return expects 2 arguments");
        ctx.redirect = std::make_pair(std::stoi(std::string(args[0])), std::string(args[1]));
    } else if (name == "server_names") {
        for (auto arg : args) {
            ctx.server_names.push_back(std::string(arg));
        }
    } else if (name == "allow_cgi") {
        ctx.allow_cgi = true; // extensions are checked in RouteRequest via ends_with
    } else {
        throw std::runtime_error("Unknown directive: " + std::string(name));
    }
}

std::vector<std::string_view>
ParseConfig::collectArguments(TokenStream &tokens) {
    std::vector<std::string_view> args;
    while (tokens.hasNext() && tokens.peek() != ";") {
        args.push_back(tokens.next());
    }
    tokens.expect(";");
    return args;
}