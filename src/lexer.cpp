#include "lexer.h"

std::vector<std::string_view>
TokenStream::buildTokens(std::string_view input) {
    std::vector<std::string_view> tokens;
    size_t start = std::string_view::npos;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '#') {
            if (start != std::string_view::npos) {
                tokens.push_back(input.substr(start, i - start));
                start = std::string_view::npos;
            }
            while (i < input.size() && input[i] != '\n') {
                i++;
            }
            continue;
        }
        if (c == '{' || c == '}' || c == ';') {
            if (start != std::string_view::npos) {
                tokens.push_back(input.substr(start, i - start));
                start = std::string_view::npos;
            }
            tokens.push_back(input.substr(i, 1));
        }
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (start != std::string_view::npos) {
                tokens.push_back(input.substr(start, i - start));
                start = std::string_view::npos;
            }
        }
        else {
            if (start == std::string_view::npos) {
                start = i;
            }
        }
    }
    if (start != std::string_view::npos) {
        tokens.push_back(input.substr(start));
    }
    return tokens;
}
