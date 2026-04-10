#include "ParseConfig.hpp"

#include <sys/stat.h>

#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <errno.h>
#include <stdexcept>

#include "log.hpp"
#include "StringUtils.hpp"
#include "lexer.h"

void
ParseConfig::parse(const std::string &path, Config &config) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open config file: " + path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    _file_content = buffer.str();

    std::vector<std::string_view> tokens_vec = TokenStream::buildTokens(_file_content);
    TokenStream tokens(tokens_vec);

    while (tokens.hasNext()) {
        auto token = tokens.next();
        if (token == "server") {
            parseServerBlock(tokens, config);
        } else {
            throw std::runtime_error("Unexpected token in global scope: " + std::string(token));
        }
    }
}

void
ParseConfig::parseServerBlock(TokenStream &tokens, Config &config) {
    tokens.expect("{");
    
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
    std::vector<std::string> allowed_methods;
    
    // Parse methods before block opens (e.g. limit_except GET POST {)
    while (tokens.hasNext() && tokens.peek() != "{") {
        allowed_methods.push_back(std::string(tokens.next()));
    }
    
    tokens.expect("{");
    while (tokens.hasNext() && tokens.peek() != "}") {
        tokens.next(); // Currently skip inner elements. Can parse inner rules here if needed.
    }
    tokens.expect("}");
    
    location.setLimitExcept(allowed_methods);
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