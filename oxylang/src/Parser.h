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

        bool consumeOpeningGeneric();
        bool consumeClosingGeneric();
        Type *parseType();

        Ast::VariableDeclaration *parseVariableDeclaration(std::vector<Ast::Attribute*> attributes);
        Ast::Expression *parseExpression();
        std::vector<Ast::Expression *> parseParameters();
        std::vector<Ast::Attribute *> parseAttributes();
        std::vector<Ast::StructDeclaration *> parseStructs();
        std::vector<Ast::VariableDeclaration *> parseVariableDeclarations();
        std::vector<Ast::Function *> parseFunctions();
        std::vector<Ast::ImportStatement *> parseImports();

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
                static Token dummy(Token::Kind::EndOfFile, {}, LiteralType::Undefined, 0, 0);
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

        [[nodiscard]] size_t GetScopeDepth() const {
            // Check the current scope depth by counting the number of unmatched '{' tokens before the current index
            size_t depth = 0;
            for (size_t i = 0; i < currentIndex; i++) {
                if (tokens[i].kind == Token::Kind::Syntax) {
                    auto syn = std::get<Syntax>(tokens[i].value);
                    if (syn == Syntax::LeftBrace) {
                        depth++;
                    } else if (syn == Syntax::RightBrace) {
                        if (depth > 0) depth--;
                    }
                }
            }

            return depth;
        }
    };
} // Oxy

#endif //OXYLANG_PARSER_H