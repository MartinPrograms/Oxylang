#ifndef OXYLANG_PARSER_H
#define OXYLANG_PARSER_H

#include "Token.h"
#include <vector>

#include "Error.h"
#include "Ast/Nodes.h"

namespace Oxy {
    class Parser {
    public:
        Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

        Ast::Root Parse();
        std::vector<Error> GetErrors() const { return errors; }
    private:
        std::vector<Token> tokens;
        std::vector<Error> errors;

        size_t currentIndex = 0;

        Type *parseType();
        Ast::Expression *parseExpression();
        std::vector<Ast::Expression *> parseParameters();
        std::vector<Ast::Attribute *> parseAttributes();
        std::vector<Ast::StructDeclaration *> parseStructs();

        // Helper functions for parsing
        [[nodiscard]] bool IsAtEnd() const {
            return currentIndex >= tokens.size() || tokens[currentIndex].kind == Token::Kind::EndOfFile;
        }

        [[nodiscard]] const Token& Peek(int i = 0) const {
            if (currentIndex + i >= tokens.size()) return tokens.back();
            return tokens[currentIndex + i];
        }

        [[nodiscard]] const Token& Match(Token::Kind kind) const {
            if (IsAtEnd() || Peek().kind != kind) {
                static Token dummy(Token::Kind::EndOfFile, {}, 0, 0);
                return dummy;
            }
            return Peek();
        }

        const Token& Advance() {
            if (!IsAtEnd()) {
                return tokens[currentIndex++];
            }
            return tokens.back();
        }
    };
} // Oxy

#endif //OXYLANG_PARSER_H