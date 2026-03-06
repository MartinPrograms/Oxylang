#include "Parser.h"

#include <fmt/format.h>

namespace Oxy {
    Ast::Root Parser::Parse() {
        std::vector<Ast::StructDeclaration *> structs = parseStructs();

        return {{}, {}, std::move(structs), Peek().line, Peek().column};
    }

    bool Parser::consumeOpeningGeneric() {
        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::LessThan) {
            Advance();
            return true;
        } else if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::ShiftLeft) {
            auto shiftLeftToken = Peek();
            tokens[currentIndex].value = Operator::LessThan;
            return true;
        }

        return false;
    }

    bool Parser::consumeClosingGeneric() {
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

    Type * Parser::parseType() {
        // Types can be nested identifiers with <T> generics, like "Option<i32>", "Result<String, Error>", etc.
        // There are 2 exceptions to this: ptr<T> and array<T, N>, which are keywords.

        // First check for the ptr and array keywords
        if (Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) {
            auto kwToken = Match(Token::Kind::Keyword);
            if (std::get<Keyword>(kwToken.value) == Keyword::Ptr) {
                Advance(); // consume 'ptr'

                if (!consumeOpeningGeneric()) {
                    errors.push_back({fmt::format("Expected '<' after 'ptr' in type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Type *nestedType = parseType();
                if (!consumeClosingGeneric()) {
                    errors.push_back({fmt::format("Expected '>' after nested type in 'ptr' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Type(LiteralType::Pointer, 0, nestedType);
            } else if (std::get<Keyword>(kwToken.value) == Keyword::Array) {
                Advance(); // consume 'array'

                if (!consumeOpeningGeneric()) {
                    errors.push_back({fmt::format("Expected '<' after 'array' in type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Type *nestedType = parseType();
                if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::Comma) {
                    errors.push_back({fmt::format("Expected ',' after nested type in 'array' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Advance(); // consume ','
                if (Match(Token::Kind::IntLiteral).kind != Token::Kind::IntLiteral) {
                    errors.push_back({fmt::format("Expected integer literal for array size in 'array' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                auto sizeToken = Match(Token::Kind::IntLiteral);
                Advance(); // consume size literal
                if (!consumeClosingGeneric()) {
                    errors.push_back({fmt::format("Expected '>' after array size in 'array' declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Type(LiteralType::Int, std::get<uint64_t>(sizeToken.value), nestedType);
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
                    errors.push_back({fmt::format("Expected '>' after nested type in generic type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                if (!consumeClosingGeneric()) {
                    errors.push_back({fmt::format("Expected '>' after nested type in generic type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                return new Type(LiteralType::UserDefined, 0, nestedType, typeName);
            } else {
                return new Type(LiteralType::UserDefined, 0, nullptr, typeName);
            }
        }

        errors.push_back({fmt::format("Expected type name, got '{}'", Peek().ToString()), "", Peek().line, Peek().column});
        return nullptr;
    }

    Ast::Expression * Parser::parseExpression() {
        if (Match(Token::Kind::IntLiteral).kind == Token::Kind::IntLiteral) {
            auto token = Match(Token::Kind::IntLiteral);
            Advance(); // consume the token
            return new Ast::LiteralExpression(LiteralType::Int, std::get<uint64_t>(token.value), token.line, token.column);
        }
        else if (Match(Token::Kind::FloatLiteral).kind == Token::Kind::FloatLiteral) {
            auto token = Match(Token::Kind::FloatLiteral);
            Advance(); // consume the token
            return new Ast::LiteralExpression(LiteralType::Float, std::get<double>(token.value), token.line, token.column);
        }
        else if (Match(Token::Kind::StringLiteral).kind == Token::Kind::StringLiteral) {
            auto token = Match(Token::Kind::StringLiteral);
            Advance(); // consume the token
            return new Ast::LiteralExpression(LiteralType::Pointer, std::get<std::string>(token.value), token.line, token.column);
        }
        else {
            errors.push_back({fmt::format("Unexpected token '{}'", Peek().ToString()), "", Peek().line, Peek().column});
            Advance(); // consume the unexpected token to avoid infinite loop
            return nullptr;
        }
    }

    std::vector<Ast::Expression *> Parser::parseParameters() {
        if (Peek().kind != Token::Kind::Punctuation || std::get<char>(Peek().value) != '(') {
            errors.push_back({fmt::format("Expected '(' at start of parameter list"), "", Peek().line, Peek().column});
            return {};
        }
        Advance(); // consume '('
        std::vector<Ast::Expression *> parameters;
        while (Peek().kind != Token::Kind::Punctuation || std::get<char>(Peek().value) != ')') {
            auto param = parseExpression();
            if (param) {
                parameters.push_back(param);
            }
            if (Peek().kind == Token::Kind::Punctuation && std::get<char>(Peek().value) == ',') {
                Advance(); // consume ','
            } else if (Peek().kind != Token::Kind::Punctuation || std::get<char>(Peek().value) != ')') {
                errors.push_back({fmt::format("Expected ',' or ')' in parameter list"), "", Peek().line, Peek().column});
                break;
            }
        }
        if (Peek().kind == Token::Kind::Punctuation && std::get<char>(Peek().value) == ')') {
            Advance(); // consume ')'
        } else {
            errors.push_back({fmt::format("Expected ')' at end of parameter list"), "", Peek().line, Peek().column});
        }
        return parameters;
    }

    std::vector<Ast::Attribute *> Parser::parseAttributes() {
        std::vector<Ast::Attribute *> attributes;
        while (Peek().kind == Token::Kind::Punctuation && std::get<char>(Peek().value) == '@') {
            Advance(); // consume '@'
            if (Match(Token::Kind::Identifier).kind != Token::Kind::Identifier) {
                errors.push_back({fmt::format("Expected attribute name after '@'"), "", Peek().line, Peek().column});
                break;
            }

            std::string attrName = std::get<std::string>(Match(Token::Kind::Identifier).value);
            Advance(); // consume attribute name

            std::vector<Ast::Expression *> args;
            if (Peek().kind == Token::Kind::Punctuation && std::get<char>(Peek().value) == '(') {
                args = parseParameters();
            }

            attributes.push_back(new Ast::Attribute(attrName, args, Peek().line, Peek().column));
        }

        return attributes;
    }

    std::vector<Ast::StructDeclaration *> Parser::parseStructs() {
        // Reset the current index to start parsing from the beginning of the token list
        currentIndex = 0;
        std::vector<Ast::StructDeclaration *> structs;
        while (!IsAtEnd()) {
            // First, parse any attributes that may be present before the struct declaration
            auto attributes = parseAttributes();
            if ((Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) && std::get<Keyword>(Match(Token::Kind::Keyword).value) == Keyword::Struct) {
                Advance(); // consume struct

                if (Match(Token::Kind::Identifier).kind != Token::Kind::Identifier) {
                    errors.push_back({fmt::format("Expected struct name after 'struct'"), "", Peek().line, Peek().column});
                    continue;
                }
                std::string structName = std::get<std::string>(Match(Token::Kind::Identifier).value);
                Advance(); // consume struct name

                if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::LeftBrace) {
                    errors.push_back({fmt::format("Expected '{{' at start of struct body"), "", Peek().line, Peek().column});
                    continue;
                }

                Advance(); // consume '{'
                std::vector<Ast::VariableDeclaration *> fields;
                // Fields are defined like: "name: type;", one per line, until we hit a '}'
                while (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightBrace) {
                    if (IsAtEnd()) {
                        errors.push_back({fmt::format("Unexpected end of file while parsing struct body"), "", Peek().line, Peek().column});
                        break;
                    }

                    // Parse field name
                    if (Match(Token::Kind::Identifier).kind != Token::Kind::Identifier) {
                        errors.push_back({fmt::format("Expected field name in struct declaration"), "", Peek().line, Peek().column});
                        break;
                    }
                    std::string fieldName = std::get<std::string>(Match(Token::Kind::Identifier).value);
                    Advance(); // consume field name

                    if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::Colon) {
                        errors.push_back({fmt::format("Expected ':' after field name in struct declaration"), "", Peek().line, Peek().column});
                        break;
                    }
                    Advance(); // consume ':'

                    auto fieldType = parseType();
                    if (!fieldType) {
                        errors.push_back({fmt::format("Expected type for field in struct declaration"), "", Peek().line, Peek().column});
                        break;
                    }

                    if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::Semicolon) {
                        errors.push_back({fmt::format("Expected ';' after field declaration in struct body"), "", Peek().line, Peek().column});
                        break;
                    }
                    Advance(); // consume ';'

                    fields.push_back(new Ast::VariableDeclaration(fieldName, fieldType, nullptr, Peek().line, Peek().column));
                }

                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightBrace) {
                    Advance(); // consume '}'
                    structs.push_back(new Ast::StructDeclaration(structName, attributes, fields, Peek().line, Peek().column));
                } else {
                    errors.push_back({fmt::format("Expected '}}' at end of struct body"), "", Peek().line, Peek().column});
                }
            }
            else {
                Advance();
                for (auto* attr : attributes) {
                    delete attr;
                }
            }
        }

        return structs;
    }
} // Oxy