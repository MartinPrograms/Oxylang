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
                bool isFloat32 = false;
                bool isHex = false;
                bool isBin = false;
                bool isUnsigned = false;
                uint32_t bitWidth = 0;

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
                } else if (Peek() == '0' && (PeekNext() == 'b' || PeekNext() == 'B')) {
                    raw += Advance(); // '0'
                    raw += Advance(); // 'b' / 'B'
                    isBin = true;
                    while (!IsAtEnd() && (Peek() == '0' || Peek() == '1'))
                        raw += Advance();
                    if (raw.size() == 2) {
                        errors.push_back({fmt::format("Invalid binary literal '{}'", raw), "", tokLine, tokCol});
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
                // Check for a suffxi like 'f', or 'uX'/'iX' for explicit types
                if (!IsAtEnd() && std::isalpha(static_cast<unsigned char>(Peek()))) {
                    if (Peek() == 'f') {
                        isFloat32 = true;
                        raw += Advance(); // consume 'f'
                    } else if (Peek() == 'u' || Peek() == 'i') {
                        isUnsigned = (Peek() == 'u');
                        raw += Advance(); // consume 'u' or 'i'
                        std::string bitWidthStr;
                        while (!IsAtEnd() && std::isdigit(static_cast<unsigned char>(Peek())))
                            bitWidthStr += Advance();
                        if (bitWidthStr.empty()) {
                            errors.push_back({fmt::format("Expected bit width after '{}' in numeric literal '{}'", isUnsigned ? 'u' : 'i', raw), "", tokLine, tokCol});
                            continue;
                        }
                        bitWidth = std::stoul(bitWidthStr);
                    }
                }

                try {
                    if (isFloat) {
                        LiteralType type = isFloat32 ? LiteralType::F32 : LiteralType::F64;
                        double value = std::stod(raw);
                        tokens.push_back(Token::MakeFloat(value, type, tokLine, tokCol));
                    } else {
                        uint64_t value = 0;
                        if (isHex) {
                            value = std::stoull(raw, nullptr, 16);
                        } else if (isBin) {
                            value = std::stoull(raw.substr(2), nullptr, 2);
                        } else {
                            value = std::stoull(raw, nullptr, 10);
                        }

                        LiteralType type;
                        if (isUnsigned) {
                            if (bitWidth == 0) bitWidth = 32; // default to unsigned 32-bit if 'u' is specified without a bit width
                            type = (bitWidth == 8) ? LiteralType::U8 :
                                   (bitWidth == 16) ? LiteralType::U16 :
                                   (bitWidth == 32) ? LiteralType::U32 :
                                   (bitWidth == 64) ? LiteralType::U64 :
                                   throw std::runtime_error(fmt::format("Unsupported unsigned integer bit width: {}", bitWidth));
                        } else {
                            if (bitWidth == 0) bitWidth = 32; // default to signed 32-bit if 'i' is specified without a bit width
                            type = (bitWidth == 8) ? LiteralType::I8 :
                                   (bitWidth == 16) ? LiteralType::I16 :
                                   (bitWidth == 32) ? LiteralType::I32 :
                                   (bitWidth == 64) ? LiteralType::I64 :
                                   throw std::runtime_error(fmt::format("Unsupported signed integer bit width: {}", bitWidth));
                        }

                        tokens.push_back(Token::MakeInt(value, type, tokLine, tokCol));
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

            // Check for syntax ( { } ; , ( ) [ ] )
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