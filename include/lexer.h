#pragma once

#include <vector>
#include <string>

class TokenStream {
    private:
        std::vector<std::string> tokens;
        size_t pos;
    public:
        TokenStream(const std::vector<std::string> &tokens) : tokens(tokens), pos(0) {}
        
        std::vector<std::string> buildTokens(const std::string &input);
        bool hasNext() {
            return pos < tokens.size();
        }

        std::string next() {
            if (hasNext()) {
                return tokens[++pos];
            }
            throw std::out_of_range("No more tokens available");
        }

        std::string peek() {
            if (hasNext()) {
                return tokens[pos];
            }
            return "";
        }

        void expect(const std::string &expected) {
            if (next() != expected) {
                throw std::runtime_error("Expected token: " + expected);
            }
        }
};