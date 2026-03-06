#include "Tokenizer.h"

#include <algorithm>

#include "spdlog/spdlog.h"

namespace Oxy {
    std::vector<Token> Tokenizer::Tokenize() {
        std::vector<Token> tokens;

        auto operators = OperatorToString;

        std::unordered_map<std::string, Keyword> kwLookup;
        kwLookup.reserve(KeywordToString.size());
        for (auto& [k, v] : KeywordToString)
            kwLookup[v] = k;

        std::unordered_map<std::string, LiteralType> typeLookup;
        typeLookup.reserve(LiteralToString.size());
        for (auto& [t, v] : LiteralToString)
            typeLookup[v] = t;

        // Operators sorted longest-first so that e.g. "==" is tried before "=",
        // "<<" before "<", etc.
        std::vector<std::pair<Operator, std::string>> sortedOps(
            OperatorToString.begin(), OperatorToString.end());
        std::sort(sortedOps.begin(), sortedOps.end(),
            [](const auto& a, const auto& b) {
                return a.second.size() > b.second.size();
            });

        while (!IsAtEnd()) {
            // First check if the current character is whitespace, and skip it
            if (IsWhitespace()) {
                Advance();
                continue;
            }

            const int tokLine = currentLine;
            const int tokCol = currentColumn;

            // Line comments
            if (Peek() == '/' && PeekNext() == '/') {
                while (!IsAtEnd() && Peek() != '\n')
                    Advance();
                continue;
            }

            // Block comments
            if (Peek() == '/' && PeekNext() == '*') {
                Advance(); // consume '/'
                Advance(); // consume '*'
                while (!IsAtEnd() && !(Peek() == '*' && PeekNext() == '/')) {
                    Advance();
                }
                if (!IsAtEnd()) {
                    Advance(); // consume '*'
                    Advance(); // consume '/'
                }
                continue;
            }

            if (IsDigit()) {
                std::string raw;
                bool isFloat = false;
                bool isHex   = false;

                if (Peek() == '0' && (PeekNext() == 'x' || PeekNext() == 'X')) {
                    raw += Advance(); // '0'
                    raw += Advance(); // 'x' / 'X'
                    isHex = true;
                    while (!IsAtEnd() && std::isxdigit(static_cast<unsigned char>(Peek())))
                        raw += Advance();
                    if (raw.size() == 2) {
                        errors.push_back({fmt::format("Invalid hexadecimal literal '{}'", raw), "", tokLine, tokCol});
                        continue;
                    }
                } else {
                    // Integer part
                    while (!IsAtEnd() && IsDigit())
                        raw += Advance();

                    // Fractional part  .digits
                    if (!IsAtEnd() && Peek() == '.' &&
                        std::isdigit(static_cast<unsigned char>(PeekNext()))) {
                        isFloat = true;
                        raw += Advance(); // '.'
                        while (!IsAtEnd() && IsDigit())
                            raw += Advance();
                    }
                }

                try {
                    if (isFloat) {
                        tokens.push_back(Token::MakeFloat(std::stod(raw), tokLine, tokCol));
                    } else {
                        const int base = isHex ? 16 : 10;
                        tokens.push_back(Token::MakeInt(std::stoull(raw, nullptr, base), tokLine, tokCol));
                    }
                } catch (const std::exception& e) {
                    errors.push_back({fmt::format("Invalid numeric literal '{}': {}", raw, e.what()), "", tokLine, tokCol});
                }
                continue;
            }

            if (IsAlpha()) {
                std::string ident;
                while (!IsAtEnd() && IsAlphaNumeric())
                    ident += Advance();

                if (auto it = kwLookup.find(ident); it != kwLookup.end()) {
                    tokens.push_back(Token::MakeKeyword(it->second, tokLine, tokCol));
                } else if (auto it = typeLookup.find(ident); it != typeLookup.end()) {
                    tokens.push_back(Token::MakeTypeName(it->second, tokLine, tokCol));
                } else {
                    tokens.push_back(Token::MakeIdentifier(ident, tokLine, tokCol));
                }
                continue;
            }

            // Check for syntax ( { } ; , )
            {
                bool matched = false;
                for (auto& [syn, str] : SyntaxToString) {
                    if (Peek() == str[0]) {
                        Advance();
                        tokens.push_back(Token::MakeSyntax(syn, tokLine, tokCol));
                        matched = true;
                        break;
                    }
                }
                if (matched) continue;
            }

            if (Peek() == '(' || Peek() == ')' ||
                Peek() == '[' || Peek() == ']') {
                tokens.push_back(Token::MakePunctuation(Advance(), tokLine, tokCol));
                continue;
            }

            // String literal
            if (Peek() == '"') {
                Advance(); // consume opening quote
                std::string str;
                while (!IsAtEnd() && Peek() != '"') {
                    if (Peek() == '\\') { // Handle escape sequences
                        Advance(); // consume '\'
                        if (IsAtEnd()) break;
                        char esc = Advance();
                        switch (esc) {
                            case 'n': str += '\n'; break;
                            case 't': str += '\t'; break;
                            case '\\': str += '\\'; break;
                            case '"': str += '"'; break;
                            default:
                                spdlog::warn("[{}:{}] Unrecognized escape sequence '\\{}'", tokLine, tokCol, esc);
                                str += esc; // Treat unrecognized escapes as literal characters
                                break;
                        }
                    } else {
                        str += Advance();
                    }
                }
                if (IsAtEnd()) {
                    spdlog::error("[{}:{}] Unterminated string literal", tokLine, tokCol);
                } else {
                    Advance(); // consume closing quote
                    tokens.push_back(Token::MakeString(str, tokLine, tokCol));
                }
                continue;
            }

            // Operators
            {
                bool matched = false;
                for (auto& [op, str] : sortedOps) {
                    if (sourceCode.compare(currentIndex, str.size(), str) == 0) {
                        for (size_t i = 0; i < str.size(); i++)
                            Advance();
                        tokens.push_back(Token::MakeOperator(op, tokLine, tokCol));
                        matched = true;
                        break;
                    }
                }
                if (matched) continue;
            }

            errors.push_back({fmt::format("Unexpected character '{}'", Peek()), "", tokLine, tokCol});
            Advance();
        }

        tokens.push_back(Token::MakeEOF(currentLine, currentColumn));
        return tokens;
    }
} // Oxy