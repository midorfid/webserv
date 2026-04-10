#pragma once

#include <vector>
#include <string_view>
#include <stdexcept>
#include <string>

class TokenStream {
    private:
        std::vector<std::string_view> tokens;
        size_t pos;
    public:
        TokenStream(const std::vector<std::string_view> &tokens) : tokens(tokens), pos(0) {}
        
        static std::vector<std::string_view> buildTokens(std::string_view input);
        bool hasNext() const {
            return pos < tokens.size();
        }

        std::string_view next() {
            if (hasNext()) {
                return tokens[pos++];
            }
            throw std::out_of_range("No more tokens available");
        }

        std::string_view peek() const {
            if (hasNext()) {
                return tokens[pos];
            }
            return "";
        }

        void expect(std::string_view expected) {
            if (next() != expected) {
                throw std::runtime_error(std::string("Expected token: ") + std::string(expected));
            }
        }
};