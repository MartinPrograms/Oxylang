#include "BodyParser.h"
#include <fmt/format.h>

#include "TypeList.h"
#include "TypeOfType.h"

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
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Semicolon) {
                    Advance();
                    continue;
                }

                // If all else fails, try to parse as an expression statement. This allows for things like function calls and assignments to be used as statements without needing a separate syntax for them.
                auto expr = parseExpression();
                if (expr) {
                    body.push_back(new Ast::ExpressionStatement(expr, expr->GetLine(), expr->GetColumn()));
                } else {
                    errors->push_back({fmt::format("Unexpected token '{}' in function body", Peek().ToString()), "", Peek().line, Peek().column});
                    Advance();
                }
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

    Ast::Expression * BodyParser::parseAddressOf(Token tok) {
        Advance();
        if (!expectSyntax(Syntax::LeftParen)) {
            errors->push_back({fmt::format("Expected '(' after 'addr'"), "", Peek().line, Peek().column});
            return nullptr;
        }

        auto expression = parseExpression();
        if (!expression) {
            errors->push_back({fmt::format("Expected expression inside 'addr'"), "", Peek().line, Peek().column});
            return nullptr;
        }

        if (!expectSyntax(Syntax::RightParen)) {
            errors->push_back({fmt::format("Expected ')' after expression in 'addr'"), "", Peek().line, Peek().column});
            return nullptr;
        }

        return new Ast::AddressOfExpression(expression, tok.line, tok.column);
    }

    Ast::Expression * BodyParser::parseDereferenceExpression(Token tok) {
        Advance();

        if (!expectSyntax(Syntax::LeftParen)) {
            errors->push_back({fmt::format("Expected '(' after 'deref'"), "", Peek().line, Peek().column});
            return nullptr;
        }

        auto expression = parseExpression();
        if (!expression) {
            errors->push_back({fmt::format("Expected expression inside 'deref'"), "", Peek().line, Peek().column});
            return nullptr;
        }

        if (!expectSyntax(Syntax::RightParen)) {
            errors->push_back({fmt::format("Expected ')' after expression in 'deref'"), "", Peek().line, Peek().column});
            return nullptr;
        }

        return new Ast::DereferenceExpression(expression, tok.line, tok.column);
    }

    Ast::Expression * BodyParser::parsePrimary() {
        auto tok = Peek();

        if (tok.kind == Token::Kind::IntLiteral) {
            Advance();
            return new Ast::LiteralExpression(tok.literalType, std::get<uint64_t>(tok.value), tok.line, tok.column);
        }
        if (tok.kind == Token::Kind::FloatLiteral) {
            Advance();
            return new Ast::LiteralExpression(tok.literalType, std::get<double>(tok.value), tok.line, tok.column);
        }
        if (tok.kind == Token::Kind::StringLiteral) {
            Advance();
            return new Ast::LiteralExpression(LiteralType::Pointer, std::get<std::string>(tok.value), tok.line, tok.column);
        }

        if (tok.kind == Token::Kind::Keyword) {
            auto kw = std::get<Keyword>(tok.value);

            if (kw == Keyword::True) { Advance(); return new Ast::LiteralExpression(LiteralType::U8, (uint64_t)1, tok.line, tok.column); }
            if (kw == Keyword::False) { Advance(); return new Ast::LiteralExpression(LiteralType::U8, (uint64_t)0, tok.line, tok.column); }
            if (kw == Keyword::Null) { Advance(); return new Ast::LiteralExpression(LiteralType::Pointer, (uint64_t)0, tok.line, tok.column); }

            if (kw == Keyword::Addr) {
                // addr(x)
                return parseAddressOf(tok);
            }

            if (kw == Keyword::Deref) {
                return parseDereferenceExpression(tok);
            }

            auto parseTypeOperator = [&](const char* name) -> Type * {
                if (!consumeOpeningGeneric()) {
                    errors->push_back({fmt::format("Expected '<' after '{}'", name), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto type = parseType();
                if (!type) {
                    errors->push_back({fmt::format("Expected type inside '{}' operator", name), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!consumeClosingGeneric()) {
                    errors->push_back({fmt::format("Expected '>' after type in '{}' operator", name), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return type;
            };

            if (kw == Keyword::Cast) {
                Advance();

                auto type = parseTypeOperator("cast");
                if (!type) {
                    errors->push_back({fmt::format("Failed to parse type in cast operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after type in cast operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto expression = parseExpression();

                if (!expression) {
                    errors->push_back({fmt::format("Expected expression in cast operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after expression in cast operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Ast::CastExpression(expression, type, tok.line, tok.column);
            }

            // sizeof<T>
            if (kw == Keyword::SizeOf) {
                Advance();

                auto type = parseTypeOperator("sizeof");
                if (!type) {
                    errors->push_back({fmt::format("Failed to parse type in sizeof operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Ast::SizeOfExpression(type, tok.line, tok.column);
            }

            // alignof<T>
            if (kw == Keyword::AlignOf) {
                Advance();

                auto type = parseTypeOperator("alignof");
                if (!type) {
                    errors->push_back({fmt::format("Failed to parse type in alignof operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Ast::AlignOfExpression(type, tok.line, tok.column);
            }

            // typeof(x)
            if (kw == Keyword::TypeOf) {
                Advance();

                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'typeof'"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto expression = parseExpression();
                if (!expression) {
                    errors->push_back({fmt::format("Expected expression in 'typeof' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after expression in 'typeof' operator"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Ast::TypeOfExpression(expression, tok.line, tok.column);
            }
        }

        if (tok.kind == Token::Kind::Syntax && std::get<Syntax>(tok.value) == Syntax::LeftParen) {
            Advance(); // consume '('
            auto expr = parseExpression();
            if (!expr) {
                errors->push_back({fmt::format("Expected expression after '('"), "", Peek().line, Peek().column});
                return nullptr;
            }
            if (!expectSyntax(Syntax::RightParen)) {
                errors->push_back({fmt::format("Expected ')' after expression"), "", Peek().line, Peek().column});
                return nullptr;
            }
            return expr;
        }

        if (tok.kind == Token::Kind::Operator && std::get<Operator>(tok.value) == Operator::Subtract) {
            Advance(); // consume '-'
            auto right = parseExpression(HIGHEST_PRECEDENCE);
            if (!right) {
                errors->push_back({fmt::format("Expected expression after '-'"), "", Peek().line, Peek().column});
                return nullptr;
            }
            return new Ast::UnaryExpression(Operator::Subtract, right, tok.line, tok.column);
        }

        if (tok.kind == Token::Kind::Identifier) {
            Advance();
            auto name = std::get<std::string>(tok.value);

            std::vector<Type*> typeArgs;
            auto currentIndexBackup = currentIndex;

            if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::LessThan) {
                if (!consumeOpeningGeneric()) {
                    errors->push_back({"Expected '<' in generic expression", "", Peek().line, Peek().column});
                    return nullptr;
                }

                do {
                    auto typeArg = parseType();
                    if (!typeArg) { errors->push_back({"Expected type argument", "", Peek().line, Peek().column}); return nullptr; }
                    typeArgs.push_back(typeArg);
                } while (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma && (Advance(), true));

                if (!consumeClosingGeneric()) {
                    // We might've made a mistake, and this actually isnt a generic expression, but a normal identifier followed by a '<' operator. So we should just ignore the generic part and continue parsing as normal.
                    currentIndex = currentIndexBackup; // reset index to before we tried parsing generics
                    typeArgs.clear();
                }
            }

            // Check for (), if so it's a generic function call, otherwise it's a generic type.
            if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::LeftParen) {
                // It's a generic function call.
                std::vector<Ast::Expression*> args;
                Advance(); // consume '('
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                    Advance(); // consume ')'
                } else {
                    while (true) {
                        auto arg = parseExpression();
                        if (!arg) {
                            errors->push_back({"Expected expression in function call arguments", "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        args.push_back(arg);
                        if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                            Advance(); // consume ','
                        } else if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                            Advance(); // consume ')'
                            break;
                        } else {
                            errors->push_back({"Expected ',' or ')' in function call arguments", "", Peek().line, Peek().column});
                            return nullptr;
                        }
                    }
                }

                return new Ast::FunctionCallExpression(new Ast::IdentifierExpression(name, tok.line, tok.column), args, typeArgs, tok.line, tok.column);
            }

            if (!typeArgs.empty()) {
                return new Ast::TypeExpression(name, typeArgs, tok.line, tok.column);
            }

            return new Ast::IdentifierExpression(name, tok.line, tok.column);
        }

        if (tok.kind == Token::Kind::Syntax) {
            auto syn = std::get<Syntax>(tok.value);
            if (syn == Syntax::RightParen || syn == Syntax::RightBracket ||
                syn == Syntax::RightBrace  || syn == Syntax::Semicolon) {
                return nullptr;
            }
        }

        errors->push_back({fmt::format("Unexpected token '{}' in expression", tok.ToString()), "", tok.line, tok.column});
        Advance(); // consume the unexpected token to avoid infinite loop
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

            std::vector<Type*> genericArgs;
            auto currentIndexBackup = currentIndex;
            if (consumeOpeningGeneric()) {
                while (true) {
                    auto typeArg = parseType();
                    if (!typeArg) {
                        errors->push_back({fmt::format("Expected type argument in generic expression"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    genericArgs.push_back(typeArg);

                    if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                        Advance(); // consume ','
                    } else if (consumeClosingGeneric()) {
                        break;
                    } else {
                        genericArgs.clear();
                        currentIndex = currentIndexBackup; // reset index to before we tried parsing generics
                        break;
                    }
                }

                // we cant do anything with these arguments, but they might be used by the function call below.
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

                left = new Ast::FunctionCallExpression(left, args, genericArgs, token.line, token.column);
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
            if (op == Operator::Assignment) {
                left = new Ast::AssignmentExpression(left, right, token.line, token.column);
            }
            else {
                left = new Ast::BinaryExpression(left, op, right, token.line, token.column);
            }
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
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Semicolon) {
                    Advance(); // consume ';'
                    return new Ast::ReturnStatement(nullptr, Peek().line, Peek().column);
                }

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
                else if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::MemberAccess) {
                    Advance(); // consume '.'
                    if (Peek().kind != Token::Kind::Identifier) {
                        errors->push_back({fmt::format("Expected identifier after '.' in member access after 'deref'"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    std::string memberName = std::get<std::string>(Match(Token::Kind::Identifier).value);
                    Advance(); // consume member name

                    if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Assignment) {
                        Advance(); // consume '='
                        auto valueExpr = parseExpression();
                        if (!valueExpr) {
                            errors->push_back({fmt::format("Expected expression for value in 'deref' member assignment statement"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        if (!expectSyntax(Syntax::Semicolon)) {
                            errors->push_back({fmt::format("Expected ';' after 'deref' member assignment statement"), "", Peek().line, Peek().column});
                            return nullptr;
                        }

                        auto memberAccessExpr = new Ast::MemberAccessExpression(new Ast::DereferenceExpression(expr, Peek().line, Peek().column), memberName, Peek().line, Peek().column);
                        return new Ast::AssignmentExpression(memberAccessExpr, valueExpr, Peek().line, Peek().column);
                    }
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
            } else if (std::get<Keyword>(kwToken.value) == Keyword::Fn) {
                Advance(); // consume 'fn'

                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'fn' in function type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                std::vector<Type*> paramTypes;
                bool isVariadic = false;
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                    Advance(); // consume ')'
                } else {
                    while (true) {
                        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Variadic)
                        {
                            Advance(); // consume '...'
                            isVariadic = true;
                            break;
                        }
                        auto paramType = parseType();
                        if (!paramType) {
                            errors->push_back({fmt::format("Expected type in function parameter list"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        paramTypes.push_back(paramType);

                        if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                            Advance(); // consume ','
                        } else if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                            Advance(); // consume ')'
                            break;
                        } else {
                            errors->push_back({fmt::format("Expected ',' or ')' in function parameter list"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                    }
                }

                Type* returnType = nullptr;
                if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Arrow) {
                    Advance(); // consume '->'
                    returnType = parseType();
                    if (!returnType) {
                        errors->push_back({fmt::format("Expected return type after '->' in function type declaration"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                } else {
                    // If there is no return type specified, we assume the return type is void.
                    returnType = new Type(LiteralType::Void);
                }

                return new FunctionType(paramTypes, returnType, isVariadic);
            }
            else if (std::get<Keyword>(kwToken.value) == Keyword::TypeOf) {
                Advance(); // consume 'typeof'

                if (!expectSyntax(Syntax::LeftParen)) {
                    errors->push_back({fmt::format("Expected '(' after 'typeof' in type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto expr = parseExpression();
                if (!expr) {
                    errors->push_back({fmt::format("Expected expression in 'typeof' type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!expectSyntax(Syntax::RightParen)) {
                    errors->push_back({fmt::format("Expected ')' after expression in 'typeof' type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new TypeOfType(expr);
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
                std::vector<Type *> genericArgs;
                while (true) {
                    auto argType = parseType();
                    if (!argType) {
                        errors->push_back({fmt::format("Expected type in generic arguments for type '{}'", typeName), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    genericArgs.push_back(argType);

                    if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                        Advance(); // consume ','
                    } else if (consumeClosingGeneric()) {
                        break;
                    } else {
                        errors->push_back({fmt::format("Expected ',' or '>' in generic arguments for type '{}'", typeName), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                }

                return new TypeList(typeName, std::move(genericArgs));
            } else {
                return new Type(LiteralType::UserDefined, 0, nullptr, typeName);
            }
        }

        errors->push_back({fmt::format("Expected type name, got '{}'", Peek().ToString()), "", Peek().line, Peek().column});
        return nullptr;
    }
} // Oxy