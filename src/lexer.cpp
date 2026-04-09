#include "lexer.hpp"

std::vector<std::string>
TokenStream::buildTokens(const std::string &input) {
    std::vector<std::string> tokens;
    std::string currentWord = "";
    for (char c : input) {
        if (c == '{' || c == '}' || c == ';') {
            if (!currentWord.empty()) {
                tokens.push_back(currentWord);
                currentWord.clear();
            }
            tokens.push_back(std::string(1, c));
        }
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (!currentWord.empty()) {
                tokens.push_back(currentWord);
                currentWord.clear();
            }
        }
        else {
            currentWord += c;
        }
    }
    if (!currentWord.empty()) {
        tokens.push_back(currentWord);
    }
    return tokens;
}
