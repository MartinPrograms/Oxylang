#ifndef OXYLANG_BODYPARSER_H
#define OXYLANG_BODYPARSER_H
#include <vector>
#include <bits/stl_vector.h>

#include "Error.h"
#include "Parser.h"
#include "Token.h"
#include "./Ast/Nodes.h"
#include <fmt/format.h>

namespace Oxy {
    /// This parses function bodies. It is separate from the main parser since function bodies are massive and I want to keep the code organized.
    class BodyParser {
    public:
        BodyParser(const std::vector<Token>& vector, size_t index, std::vector<Error>* errors);

        std::vector<Ast::Node *> ParseBody();
        [[nodiscard]] size_t GetCurrentIndex() const { return currentIndex; }
    private:
        std::vector<Token> tokens;
        std::vector<Error> *errors;
        size_t currentIndex;

        Ast::VariableDeclaration *parseVariableDeclaration();
        Ast::Expression *parsePrimary();
        Ast::Expression *parseExpression(float precedence = LOWEST_PRECEDENCE);

        bool isStatementStart();
        Ast::Node * parseStatement();

        bool consumeOpeningGeneric();
        bool consumeClosingGeneric();
        Type *parseType();


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

        bool expectSyntax(Syntax s) {
            if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == s) {
                Advance();
                return true;
            }

            const char* name = "?";
            switch (s) {
                case Syntax::LeftParen: name = "("; break;
                case Syntax::RightParen: name = ")"; break;
                case Syntax::LeftBracket: name = "["; break;
                case Syntax::RightBracket: name = "]"; break;
                case Syntax::Comma: name = ","; break;
                case Syntax::Semicolon: name = ";"; break;
                default: break;
            }

            errors->push_back({fmt::format("Expected '{}', got '{}'", name, Peek().ToString()),
                              "", Peek().line, Peek().column});
            return false;
        }

        bool isBitwiseOperator(Operator op) {
            return op == Operator::BitwiseAnd || op == Operator::BitwiseOr || op == Operator::BitwiseXor || op == Operator::ShiftLeft || op == Operator::ShiftRight;
        }
    };
} // Oxy

#endif //OXYLANG_BODYPARSER_H