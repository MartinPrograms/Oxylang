#include "BodyParser.h"
#include <fmt/format.h>

namespace Oxy {
    BodyParser::BodyParser(const std::vector<Token> &vector, size_t index, std::vector<Error> *errors) : tokens(vector), errors(errors), currentIndex(index) {
    }

    std::vector<Ast::Node *> BodyParser::ParseBody() {
        std::vector<Ast::Node *> body;

        // Parse statements until we hit a '}'
        while (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightBrace) {
            if (IsAtEnd()) {
                errors->push_back({fmt::format("Unexpected end of file while parsing function body"), "", Peek().line, Peek().column});
                break;
            }

            if (isStatementStart()) {
                auto stmt = parseStatement();
                if (stmt) {
                    body.push_back(stmt);
                }
            }
            else {
                // Skip if ';'
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Semicolon) {
                    Advance();
                    continue;
                }

                errors->push_back({fmt::format("Unexpected token '{}' in function body", Peek().ToString()), "", Peek().line, Peek().column});
                Advance();
            }
        }

        return body;
    }

    Ast::VariableDeclaration *BodyParser::parseVariableDeclaration() {
        // A variable decleration is identifier: type = initializer; where "= initializer" is optional.
        if (Match(Token::Kind::Identifier).kind != Token::Kind::Identifier) {
            errors->push_back({fmt::format("Expected variable name in variable declaration"), "", Peek().line, Peek().column});
            return nullptr;
        }

        std::string varName = std::get<std::string>(Match(Token::Kind::Identifier).value);
        Advance(); // consume variable name

        Type* varType = nullptr;
        // A type is optional, but only if there is an initializer. If there is no type and no initializer, then it's an error.
        if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Colon) {
            Advance(); // consume ':'
            varType = parseType();
            if (!varType) {
                errors->push_back({fmt::format("Expected type in variable declaration"), "", Peek().line, Peek().column});
                return nullptr;
            }
        }

        Ast::Expression* initializer = nullptr;
        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Assignment) {
            Advance(); // consume '='
            initializer = parseExpression();
            if (!initializer) {
                errors->push_back({fmt::format("Expected initializer expression in variable declaration"), "", Peek().line, Peek().column});
                return nullptr;
            }
        }

        if (!varType && !initializer) {
            errors->push_back({fmt::format("Variable declaration must have a type or an initializer"), "", Peek().line, Peek().column});
            return nullptr;
        }

        return new Ast::VariableDeclaration(varName, varType, initializer, {}, Peek().line, Peek().column);
    }

    Ast::Expression * BodyParser::parsePrimary() {
        auto tok = Peek();

        if (tok.kind == Token::Kind::IntLiteral) {
            Advance();
            return new Ast::LiteralExpression(tok.literalType,
                std::get<uint64_t>(tok.value), tok.line, tok.column);
        }
        if (tok.kind == Token::Kind::FloatLiteral) {
            Advance();
            return new Ast::LiteralExpression(tok.literalType,
                std::get<double>(tok.value), tok.line, tok.column);
        }
        if (tok.kind == Token::Kind::StringLiteral) {
            Advance();
            return new Ast::LiteralExpression(LiteralType::Pointer,
                std::get<std::string>(tok.value), tok.line, tok.column);
        }

        if (tok.kind == Token::Kind::Keyword) {
            if (std::get<Keyword>(tok.value) == Keyword::True) {
                Advance();
                return new Ast::LiteralExpression(LiteralType::U8, (uint64_t)1, tok.line, tok.column);
            }

            if (std::get<Keyword>(tok.value) == Keyword::False) {
                Advance();
                return new Ast::LiteralExpression(LiteralType::U8, (uint64_t)0, tok.line, tok.column);
            }

            if (std::get<Keyword>(tok.value) == Keyword::Null) {
                Advance();
                return new Ast::LiteralExpression(LiteralType::Pointer, (uint64_t)0, tok.line, tok.column);
            }

            if (std::get<Keyword>(tok.value) == Keyword::Addr) {
                Advance(); // consume 'addr'
                if (!expectSyntax(Syntax::LeftParen)) return nullptr;
                auto expr = parseExpression();
                if (!expr) {
                    errors->push_back({fmt::format("Expected expression in 'addr' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::RightParen)) return nullptr;
                return new Ast::AddressOfExpression(expr, tok.line, tok.column);
            }

            if (std::get<Keyword>(tok.value) == Keyword::Deref) {
                Advance(); // consume 'deref'
                if (!expectSyntax(Syntax::LeftParen)) return nullptr;
                auto expr = parseExpression();
                if (!expr) {
                    errors->push_back({fmt::format("Expected expression in 'deref' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::RightParen)) return nullptr;
                return new Ast::DereferenceExpression(expr, tok.line, tok.column);
            }

            if (std::get<Keyword>(tok.value) == Keyword::SizeOf) {
                Advance(); // consume 'sizeof'
                // Expect <
                if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::LessThan) {
                    Advance(); // consume '<'
                    auto type = parseType();
                    if (!type) {
                        errors->push_back({fmt::format("Expected type in 'sizeof' operator"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    if (Peek().kind != Token::Kind::Operator || std::get<Operator>(Peek().value) != Operator::GreaterThan) {
                        errors->push_back({fmt::format("Expected '>' after type in 'sizeof' operator"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    Advance(); // consume '>'
                    if (!expectSyntax(Syntax::LeftParen)) {
                        errors->push_back({fmt::format("Expected '(' after 'sizeof' type in sizeof expression"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    if (!expectSyntax(Syntax::RightParen)) return nullptr;
                    return new Ast::SizeOfExpression(type, tok.line, tok.column);
                }
            }

            if (std::get<Keyword>(tok.value) == Keyword::AlignOf) {
                Advance(); // consume 'alignof'
                // Expect <
                if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::LessThan) {
                    Advance(); // consume '<'
                    auto type = parseType();
                    if (!type) {
                        errors->push_back({fmt::format("Expected type in 'alignof' operator"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    if (Peek().kind != Token::Kind::Operator || std::get<Operator>(Peek().value) != Operator::GreaterThan) {
                        errors->push_back({fmt::format("Expected '>' after type in 'alignof' operator"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    Advance(); // consume '>'
                    if (!expectSyntax(Syntax::LeftParen)) {
                        errors->push_back({fmt::format("Expected '(' after 'alignof' type in alignof expression"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    if (!expectSyntax(Syntax::RightParen)) return nullptr;
                    return new Ast::AlignOfExpression(type, tok.line, tok.column);
                }
            }

            if (std::get<Keyword>(tok.value) == Keyword::TypeOf) {
                Advance(); // consume 'typeof'
                if (!expectSyntax(Syntax::LeftParen)) return nullptr;
                auto expr = parseExpression();
                if (!expr) {
                    errors->push_back({fmt::format("Expected expression in 'typeof' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::RightParen)) return nullptr;
                return new Ast::TypeOfExpression(expr, tok.line, tok.column);
            }

            if (std::get<Keyword>(tok.value) == Keyword::Cast) {
                Advance(); // consume 'cast'
                if (Peek().kind != Token::Kind::Operator || std::get<Operator>(Peek().value) != Operator::LessThan) {
                    errors->push_back({fmt::format("Expected '<' after 'cast' in cast expression"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!consumeOpeningGeneric()) {
                    errors->push_back({fmt::format("Expected '<' after 'cast' in cast expression"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto type = parseType();
                if (!type) {
                    errors->push_back({fmt::format("Expected type in 'cast' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!consumeClosingGeneric()) {
                    errors->push_back({fmt::format("Expected '>' after type in 'cast' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'cast' type in cast expression"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto expr = parseExpression();
                if (!expr) {
                    errors->push_back({fmt::format("Expected expression in cast expression"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after expression in cast expression"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Ast::CastExpression(expr, type, tok.line, tok.column);
            }

            // allocate<T>(count) // count is optional, if count is not provided, we just allocate a single T.
            if (std::get<Keyword>(tok.value) == Keyword::Allocate) {
                Advance(); // consume 'allocate'
                if (!consumeOpeningGeneric()) {
                    errors->push_back({fmt::format("Expected '<' after 'allocate' in allocate expression"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto type = parseType();
                if (!type) {
                    errors->push_back({fmt::format("Expected type in 'allocate' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!consumeClosingGeneric()) {
                    errors->push_back({fmt::format("Expected '>' after type in 'allocate' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Ast::Expression *countExpr = nullptr;
                if (!expectSyntax(Syntax::LeftParen)) return nullptr;
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                    Advance(); // consume ')'
                }else {
                    countExpr = parseExpression();
                    if (!countExpr) {
                        errors->push_back({fmt::format("Expected expression for count in 'allocate' operator"), "", Peek().line, Peek().column});
                        return nullptr;
                    }

                    if (!expectSyntax(Syntax::RightParen)) return nullptr;
                }

                return new Ast::AllocateExpression(type, countExpr, tok.line, tok.column);
            }
        }

        if (tok.kind == Token::Kind::Syntax && std::get<Syntax>(tok.value) == Syntax::LeftParen) {
            Advance(); // consume '('
            auto* inner = parseExpression();
            if (!inner) return nullptr;
            if (!expectSyntax(Syntax::RightParen)) return nullptr;
            return new Ast::NestedExpression(inner, tok.line, tok.column);
        }

        if (tok.kind == Token::Kind::Operator && std::get<Operator>(tok.value) == Operator::Subtract) {
            Advance();

            auto* operand = parseExpression(HIGHEST_PRECEDENCE);
            if (!operand) return nullptr;
            return new Ast::UnaryExpression(std::get<Operator>(tok.value), operand, tok.line, tok.column);
        }

        if (tok.kind == Token::Kind::Identifier) {
            Advance(); // consume identifier
            return new Ast::IdentifierExpression(std::get<std::string>(tok.value), tok.line, tok.column);
        }

        // If its ) or ] or } or ;, then we just return nullptr to indicate the end of the current expression. (for example 'for (let i: i32 = 0; i < 10; i + 1)' when we parse 'i + 1', we will hit the ')' at the end, which indicates the end of the expression.)
        if (tok.kind == Token::Kind::Syntax) {
            auto syn = std::get<Syntax>(tok.value);
            if (syn == Syntax::RightParen || syn == Syntax::RightBracket || syn == Syntax::RightBrace || syn == Syntax::Semicolon) {
                Advance();
                return nullptr;
            }
        }

        errors->push_back({fmt::format("Unexpected token '{}' in expression", tok.ToString()), "", tok.line, tok.column});
        Advance(); // consume to avoid infinite loop
        return nullptr;
    }

    Ast::Expression * BodyParser::parseExpression(float precedence) {
        auto left = parsePrimary();
        if (!left) return nullptr;

        while (true) {
            auto token = Peek();

            // Postfix (++, --)
            if (token.kind == Token::Kind::Operator) {
                auto op = std::get<Operator>(token.value);
                if (op == Operator::Increment || op == Operator::Decrement) {
                    Advance(); // consume operator
                    left = new Ast::PostfixExpression(left, op, token.line, token.column);
                    continue;
                }
            }

            // If token is (, it's a function call
            if (token.kind == Token::Kind::Syntax && std::get<Syntax>(token.value) == Syntax::LeftParen) {
                Advance(); // consume '('
                std::vector<Ast::Expression *> args;
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                    Advance(); // consume ')'
                } else {
                    while (true) {
                        auto arg = parseExpression();
                        if (!arg) {
                            errors->push_back({fmt::format("Expected expression in function call arguments"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        args.push_back(arg);

                        if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                            Advance(); // consume ','
                        } else if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                            Advance(); // consume ')'
                            break;
                        } else {
                            errors->push_back({fmt::format("Expected ',' or ')' in function call arguments"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                    }
                }

                left = new Ast::FunctionCallExpression(left, args, token.line, token.column);
                continue;
            }

            // Subscript []
            if (token.kind == Token::Kind::Syntax && std::get<Syntax>(token.value) == Syntax::LeftBracket) {
                Advance(); // consume '['
                auto indexExpr = parseExpression();
                if (!indexExpr) return nullptr;
                if (!expectSyntax(Syntax::RightBracket)) return nullptr;

                left = new Ast::SubscriptExpression(left, indexExpr, token.line, token.column);
                continue;
            }

            // Member access .
            if (token.kind == Token::Kind::Operator && std::get<Operator>(token.value) == Operator::MemberAccess) {
                Advance(); // consume '.'
                if (Peek().kind != Token::Kind::Identifier) {
                    errors->push_back({fmt::format("Expected identifier after '.' in member access"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                std::string memberName = std::get<std::string>(Match(Token::Kind::Identifier).value);
                Advance(); // consume member name

                left = new Ast::MemberAccessExpression(left, memberName, token.line, token.column);
                continue;
            }

            // If we get here, it is a binary expression.
            if (token.kind != Token::Kind::Operator)
                break;

            auto op = std::get<Operator>(token.value);
            bool isBitwise = isBitwiseOperator(op);
            float opPrecedence = OperatorPrecedence[op];
            if (opPrecedence > precedence) // lower precedence = higher binding power.
                break;
            bool isRightAssociative = OperatorRightAssociative[op];
            Advance(); // consume operator

            // If this is a bitwise binary operator, and left was also a bitwise binary operator, then we need to cause a fault. All binary operators are ambigious and must be disambiguated with parentheses.
            auto* leftBinary = dynamic_cast<Ast::BinaryExpression*>(left);
            if (leftBinary && isBitwise && isBitwiseOperator(leftBinary->GetOperator())) {
                errors->push_back({fmt::format("Bitwise operator '{}' is ambigious. Use parentheses to disambiguate.", token.ToString()), "", token.line, token.column});
            }

            auto right = parseExpression(isRightAssociative ? opPrecedence : opPrecedence - 0.1f); // If the operator is right associative, then we use the same precedence for the right side, otherwise we use a slightly lower precedence to ensure left associativity.
            if (!right) return nullptr;
            left = new Ast::BinaryExpression(left, op, right, token.line, token.column);
        }

        return left;
    }

    bool BodyParser::isStatementStart() {
        if (Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) {
            auto kw = std::get<Keyword>(Match(Token::Kind::Keyword).value);
            return kw == Keyword::Let || kw == Keyword::If || kw == Keyword::While || kw == Keyword::Return || kw == Keyword::Break || kw == Keyword::Continue || kw == Keyword::For || kw == Keyword::Deref;
        }

        return Match(Token::Kind::Identifier).kind == Token::Kind::Identifier;
    }

    Ast::Node * BodyParser::parseStatement() {
        if (Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) {
            auto kw = std::get<Keyword>(Match(Token::Kind::Keyword).value);
            if (kw == Keyword::Let) {
                // A variable declaration statement starts with "let"
                Advance(); // consume 'let'
                auto declaration = parseVariableDeclaration();
                if (!declaration) {
                    errors->push_back({fmt::format("Invalid variable declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::Semicolon)) {
                    errors->push_back({fmt::format("Expected ';' after variable declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                return declaration;
            }
            else if (kw == Keyword::If) {
                Advance(); // consume 'if'
                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'if'"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                auto condition = parseExpression();
                if (!condition) {
                    errors->push_back({fmt::format("Expected condition expression in 'if' statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after condition in 'if' statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::LeftBrace)) {
                    errors->push_back({fmt::format("Expected '{{' at start of 'if' body"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                BodyParser ifBodyParser(tokens, currentIndex, errors);
                auto ifBody = ifBodyParser.ParseBody();
                currentIndex = ifBodyParser.currentIndex; // update current index after parsing body
                if (!expectSyntax(Syntax::RightBrace)) {
                    errors->push_back({fmt::format("Expected '}}' at end of 'if' body"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Ast::IfStatement::Branch ifBranch{condition, ifBody};
                std::vector<Ast::IfStatement::Branch> elseIfBranches;
                std::vector<Ast::Node*> elseBranch;

                // Parse zero or more "else if" branches
                while (Match(Token::Kind::Keyword).kind == Token::Kind::Keyword && std::get<Keyword>(Match(Token::Kind::Keyword).value) == Keyword::Else) {
                    Advance(); // consume 'else'
                    if (Match(Token::Kind::Keyword).kind == Token::Kind::Keyword && std::get<Keyword>(Match(Token::Kind::Keyword).value) == Keyword::If) {
                        Advance(); // consume 'if'
                        if (!expectSyntax(Syntax::LeftParen)) {
                            errors->push_back({fmt::format("Expected '(' after 'else if'"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        auto elseIfCondition = parseExpression();
                        if (!elseIfCondition) {
                            errors->push_back({fmt::format("Expected condition expression in 'else if' statement"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        if (!expectSyntax(Syntax::RightParen)) {
                            errors->push_back({fmt::format("Expected ')' after condition in 'else if' statement"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        if (!expectSyntax(Syntax::LeftBrace)) {
                            errors->push_back({fmt::format("Expected '{{' at start of 'else if' body"), "", Peek().line, Peek().column});
                            return nullptr;
                        }

                        BodyParser elseIfBodyParser(tokens, currentIndex, errors);
                        auto elseIfBody = elseIfBodyParser.ParseBody();
                        currentIndex = elseIfBodyParser.currentIndex; // update current index after parsing body
                        if (!expectSyntax(Syntax::RightBrace)) {
                            errors->push_back({fmt::format("Expected '}}' at end of 'else if' body"), "", Peek().line, Peek().column});
                            return nullptr;
                        }

                        elseIfBranches.push_back({elseIfCondition, elseIfBody});
                    }
                    else {
                        // This is an "else" branch without an "if"
                        if (!expectSyntax(Syntax::LeftBrace)) {
                            errors->push_back({fmt::format("Expected '{{' at start of 'else' body"), "", Peek().line, Peek().column});
                            return nullptr;
                        }

                        BodyParser elseBodyParser(tokens, currentIndex, errors);
                        elseBranch = elseBodyParser.ParseBody();
                        currentIndex = elseBodyParser.currentIndex; // update current index after parsing body
                        if (!expectSyntax(Syntax::RightBrace)) {
                            errors->push_back({fmt::format("Expected '}}' at end of 'else' body"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        break; // else must be the last branch, so we break out of the loop after parsing it
                    }
                }

                return new Ast::IfStatement(ifBranch, elseIfBranches, elseBranch, Peek().line, Peek().column);
            }
            else if (kw == Keyword::While) {
                Advance(); // consume 'while'
                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'while'"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                auto condition = parseExpression();
                if (!condition) {
                    errors->push_back({fmt::format("Expected condition expression in 'while' statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after condition in 'while' statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::LeftBrace)) {
                    errors->push_back({fmt::format("Expected '{{' at start of 'while' body"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                BodyParser whileBodyParser(tokens, currentIndex, errors);
                auto whileBody = whileBodyParser.ParseBody();
                currentIndex = whileBodyParser.currentIndex; // update current index after parsing body
                if (!expectSyntax(Syntax::RightBrace)) {
                    errors->push_back({fmt::format("Expected '}}' at end of 'while' body"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Ast::WhileStatement(condition, whileBody, Peek().line, Peek().column);
            }
            else if (kw == Keyword::For) {
                Advance(); // consume 'for'

                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'for'"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                // for looks like C: for (init; condition; increment) { body }
                Ast::Node* init = nullptr;
                init = parseStatement();
                if (!init) {
                    errors->push_back({fmt::format("Expected initialization statement in 'for' loop"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                // We already consumed the semicolon.

                Ast::Expression* condition = nullptr;
                condition = parseExpression();
                if (!condition) {
                    errors->push_back({fmt::format("Expected condition expression in 'for' loop"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::Semicolon)) {
                    errors->push_back({fmt::format("Expected ';' after condition expression in 'for' loop"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Ast::Expression* increment = nullptr;
                increment = parseExpression();
                if (!increment) {
                    errors->push_back({fmt::format("Expected increment expression in 'for' loop"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after increment expression in 'for' loop"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::LeftBrace)) {
                    errors->push_back({fmt::format("Expected '{{' at start of 'for' loop body"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                BodyParser forBodyParser(tokens, currentIndex, errors);
                auto forBody = forBodyParser.ParseBody();
                currentIndex = forBodyParser.currentIndex; // update current index after parsing body
                if (!expectSyntax(Syntax::RightBrace)) {
                    errors->push_back({fmt::format("Expected '}}' at end of 'for' loop body"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Ast::ForStatement(init, condition, increment, forBody, Peek().line, Peek().column);
            }
            else if (kw == Keyword::Return) {
                Advance(); // consume 'return'
                auto expr = parseExpression();
                if (!expectSyntax(Syntax::Semicolon)) {
                    errors->push_back({fmt::format("Expected ';' after return statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                return new Ast::ReturnStatement(expr, Peek().line, Peek().column);
            }
            else if (kw == Keyword::Break) {
                Advance(); // consume 'break'
                if (!expectSyntax(Syntax::Semicolon)) {
                    errors->push_back({fmt::format("Expected ';' after break statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                return new Ast::BreakStatement(Peek().line, Peek().column);
            }
            else if (kw == Keyword::Continue) {
                Advance(); // consume 'continue'
                if (!expectSyntax(Syntax::Semicolon)) {
                    errors->push_back({fmt::format("Expected ';' after continue statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                return new Ast::ContinueStatement(Peek().line, Peek().column);
            }
            else if (kw == Keyword::Deref) {
                Advance(); // consume 'deref'
                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'deref' in dereference statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                auto expr = parseExpression();
                if (!expr) {
                    errors->push_back({fmt::format("Expected expression in 'deref' statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after expression in 'deref' statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                // Now we expect an assignment, since "deref (expr) = value" is how we do pointer assignment in Oxylang.
                if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Assignment) {
                    Advance(); // consume '='
                    auto valueExpr = parseExpression();
                    if (!valueExpr) {
                        errors->push_back({fmt::format("Expected expression for value in 'deref' assignment statement"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    if (!expectSyntax(Syntax::Semicolon)) {
                        errors->push_back({fmt::format("Expected ';' after 'deref' assignment statement"), "", Peek().line, Peek().column});
                        return nullptr;
                    }

                    return new Ast::DereferenceAssignmentStatement(expr, valueExpr, Peek().line, Peek().column);
                }
                else {
                    errors->push_back({fmt::format("Expected '=' after 'deref' expression in dereference assignment statement"), "", Peek().line, Peek().column});
                    return nullptr;
                }
            }
        }
        else if (Match(Token::Kind::Identifier).kind == Token::Kind::Identifier) {
            auto expr = parseExpression();
            if (!expr) {
                errors->push_back({fmt::format("Invalid expression in expression statement"), "", Peek().line, Peek().column});
                return nullptr;
            }
            if (!expectSyntax(Syntax::Semicolon)) {
                errors->push_back({fmt::format("Expected ';' after expression statement"), "", Peek().line, Peek().column});
                return nullptr;
            }

            return new Ast::ExpressionStatement(expr, Peek().line, Peek().column);
        }

        errors->push_back({fmt::format("Unexpected token '{}' at start of statement", Peek().ToString()), "", Peek().line, Peek().column});
        Advance(); // consume the unexpected token to avoid infinite loop
        return nullptr;
    }

    bool BodyParser::consumeOpeningGeneric() {
        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::LessThan) {
            Advance();
            return true;
        }

        return false;
    }

    bool BodyParser::consumeClosingGeneric() {
        // Consume > or >>
        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::GreaterThan) {
            Advance();
            return true;
        } else if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::ShiftRight) {
            auto shiftRightToken = Peek();
            tokens[currentIndex].value = Operator::GreaterThan;
            return true;
        }

        return false;
    }

    Type * BodyParser::parseType() {
        // Types can be nested identifiers with <T> generics, like "Option<i32>", "Result<String, Error>", etc.
        // There are 2 exceptions to this: ptr<T> and array<T, N>, which are keywords.

        // First check for the ptr and array keywords
        if (Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) {
            auto kwToken = Match(Token::Kind::Keyword);
            if (std::get<Keyword>(kwToken.value) == Keyword::Ptr) {
                Advance(); // consume 'ptr'

                if (!consumeOpeningGeneric()) {
                    errors->push_back({fmt::format("Expected '<' after 'ptr' in type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Type *nestedType = parseType();
                if (!consumeClosingGeneric()) {
                    errors->push_back({fmt::format("Expected '>' after nested type in 'ptr' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Type(LiteralType::Pointer, 0, nestedType);
            } else if (std::get<Keyword>(kwToken.value) == Keyword::Array) {
                Advance(); // consume 'array'

                if (!consumeOpeningGeneric()) {
                    errors->push_back({fmt::format("Expected '<' after 'array' in type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Type *nestedType = parseType();
                if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::Comma) {
                    errors->push_back({fmt::format("Expected ',' after nested type in 'array' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Advance(); // consume ','
                if (Match(Token::Kind::IntLiteral).kind != Token::Kind::IntLiteral) {
                    errors->push_back({fmt::format("Expected integer literal for array size in 'array' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto sizeToken = Match(Token::Kind::IntLiteral);
                Advance(); // consume size literal
                if (!consumeClosingGeneric()) {
                    errors->push_back({fmt::format("Expected '>' after array size in 'array' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Type(LiteralType::Pointer, std::get<uint64_t>(sizeToken.value), nestedType);
            }
        }

        // Otherwise, it is either a TypeName or an Identifier (for user defined types)
        if (Match(Token::Kind::TypeName).kind == Token::Kind::TypeName) {
            auto token = Match(Token::Kind::TypeName);
            Advance(); // consume the token
            return new Type(std::get<LiteralType>(token.value));
        } else if (Match(Token::Kind::Identifier).kind == Token::Kind::Identifier) {
            auto typeNameToken = Match(Token::Kind::Identifier);
            Advance(); // consume the identifier
            std::string typeName = std::get<std::string>(typeNameToken.value);
            if (consumeOpeningGeneric()) {
                Type *nestedType = parseType();
                if (Peek().kind != Token::Kind::Operator || std::get<Operator>(Peek().value) != Operator::GreaterThan) {
                    errors->push_back({fmt::format("Expected '>' after nested type in generic type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!consumeClosingGeneric()) {
                    errors->push_back({fmt::format("Expected '>' after nested type in generic type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Type(LiteralType::UserDefined, 0, nestedType, typeName);
            } else {
                return new Type(LiteralType::UserDefined, 0, nullptr, typeName);
            }
        }

        errors->push_back({fmt::format("Expected type name, got '{}'", Peek().ToString()), "", Peek().line, Peek().column});
        return nullptr;
    }
} // Oxy