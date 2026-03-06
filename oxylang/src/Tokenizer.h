#ifndef OXYLANG_TOKENIZER_H
#define OXYLANG_TOKENIZER_H

#include <functional>
#include <string>
#include <vector>

#include "Error.h"
#include "Token.h"

namespace Oxy {
    class Tokenizer {
    public:
        Tokenizer(const std::string& sourceCode) : sourceCode(sourceCode) {}

        std::vector<Token> Tokenize();
        std::vector<Error> GetErrors() const { return errors; }

    private:
        std::string sourceCode;

        size_t currentIndex = 0;
        int currentLine = 1;
        int currentColumn = 1;

        // Helper functions for tokenization
        [[nodiscard]] bool IsAtEnd() const {
            return currentIndex >= sourceCode.size();
        }

        [[nodiscard]] char Peek(int i = 0) const {
            if (currentIndex + i >= sourceCode.size()) return '\0';
            return sourceCode[currentIndex + i];
        }

        [[nodiscard]] char PeekNext() const {
            if (currentIndex + 1 >= sourceCode.size()) return '\0';
            return sourceCode[currentIndex + 1];
        }

        char Advance() {
            if (!IsAtEnd()) {
                if (Peek() == '\n') {
                    currentLine++;
                    currentColumn = 0;
                }

                currentColumn++;
                return sourceCode[currentIndex++];
            }
            return '\0';
        }

        [[nodiscard]] bool IsWhitespace() const {
            char c = Peek();
            return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        }

        [[nodiscard]] bool IsDigit() const {
            char c = Peek();
            return c >= '0' && c <= '9';
        }

        [[nodiscard]] bool IsAlpha() const {
            char c = Peek();
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        }

        [[nodiscard]] bool IsAlphaNumeric() const {
            return IsAlpha() || IsDigit();
        }

        std::vector<Error> errors;
    };
} // Oxy

#endif //OXYLANG_TOKENIZER_H