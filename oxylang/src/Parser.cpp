#include "Parser.h"

#include <fmt/format.h>
#include <utility>
#include "BodyParser.h"
#include "FunctionType.h"
#include "TypeList.h"

namespace Oxy {
    Ast::Root Parser::Parse() {
        std::vector<Ast::Function *> functions = parseFunctions();
        std::vector<Ast::StructDeclaration *> structs = parseStructs();
        std::vector<Ast::VariableDeclaration *> variables = parseVariableDeclarations();
        std::vector<Ast::ImportStatement *> imports = parseImports();

        return {std::move(variables), std::move(functions), std::move(structs), std::move(imports), Peek().line, Peek().column};
    }

    bool Parser::consumeOpeningGeneric() {
        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::LessThan) {
            Advance();
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
        // There is also a function type, which goes like "fn(arg, arg2) -> returnType"

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

                return new Type(LiteralType::Pointer, std::get<uint64_t>(sizeToken.value), nestedType);
            }
            else if (std::get<Keyword>(kwToken.value) == Keyword::Fn) {
                Advance(); // consume 'fn'

                if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::LeftParen) {
                    errors.push_back({fmt::format("Expected '(' after 'fn' in function type declaration"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Advance(); // consume '('

                std::vector<Type *> paramTypes;
                bool isVariadic = false;
                if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightParen) {
                    while (true) {
                        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Variadic) {
                            isVariadic = true;
                            Advance(); // consume '...'
                            break;
                        }

                        Type* paramType = parseType();
                        if (!paramType) {
                            errors.push_back({fmt::format("Expected type in function parameter list"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                        paramTypes.push_back(paramType);

                        if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                            Advance(); // consume ','
                        } else if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
                            break;
                        } else {
                            errors.push_back({fmt::format("Expected ',' or ')' in function parameter list"), "", Peek().line, Peek().column});
                            return nullptr;
                        }
                    }
                }

                if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightParen) {
                    errors.push_back({fmt::format("Expected ')' after function parameter list"), "", Peek().line, Peek().column});
                    return nullptr;
                }

                Advance(); // consume ')'
                Type* returnType = nullptr;

                if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Arrow) {
                    Advance(); // consume '->'
                    returnType = parseType();
                    if (!returnType) {
                        errors.push_back({fmt::format("Expected return type in function type declaration"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                }
                else {
                    // If there is no return type, then it's implicitly 'void'
                    returnType = new Type(LiteralType::Void);
                }

                return new FunctionType(std::move(paramTypes), returnType, isVariadic);
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
                    Type* genericArg = parseType();
                    if (!genericArg) {
                        errors.push_back({fmt::format("Expected type in generic type declaration"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                    genericArgs.push_back(genericArg);
                    if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                        Advance(); // consume ','
                    } else if (consumeClosingGeneric()) {
                        break;
                    } else {
                        errors.push_back({fmt::format("Expected ',' or '>' in generic type declaration"), "", Peek().line, Peek().column});
                        return nullptr;
                    }
                }

                return new TypeList(typeName, std::move(genericArgs));
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
            return new Ast::LiteralExpression(token.literalType, std::get<uint64_t>(token.value), token.line, token.column);
        }
        else if (Match(Token::Kind::FloatLiteral).kind == Token::Kind::FloatLiteral) {
            auto token = Match(Token::Kind::FloatLiteral);
            Advance(); // consume the token
            return new Ast::LiteralExpression(token.literalType, std::get<double>(token.value), token.line, token.column);
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

    Ast::VariableDeclaration * Parser::parseVariableDeclaration(std::vector <Ast::Attribute *> attributes) {
        // A variable decleration is identifier: type = initializer; where "= initializer" is optional.
        if (Match(Token::Kind::Identifier).kind != Token::Kind::Identifier) {
            errors.push_back({fmt::format("Expected variable name in variable declaration"), "", Peek().line, Peek().column});
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
                errors.push_back({fmt::format("Expected type in variable declaration"), "", Peek().line, Peek().column});
                return nullptr;
            }
        }

        Ast::Expression* initializer = nullptr;
        if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Assignment) {
            Advance(); // consume '='
            initializer = parseExpression();
            if (!initializer) {
                errors.push_back({fmt::format("Expected initializer expression in variable declaration"), "", Peek().line, Peek().column});
                return nullptr;
            }
        }

        if (!varType && !initializer) {
            errors.push_back({fmt::format("Variable declaration must have a type or an initializer"), "", Peek().line, Peek().column});
            return nullptr;
        }

        return new Ast::VariableDeclaration(varName, varType, initializer, std::move(attributes), Peek().line, Peek().column);
    }

    std::vector<Ast::Expression *> Parser::parseParameters() {
        if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::LeftParen) {
            errors.push_back({fmt::format("Expected '(' at start of parameter list"), "", Peek().line, Peek().column});
            return {};
        }

        Advance(); // consume '('
        std::vector<Ast::Expression *> parameters;
        while (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightParen) {
            auto param = parseExpression();
            if (param) {
                parameters.push_back(param);
            }
            if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                Advance(); // consume ','
            } else if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightParen) {
                errors.push_back({fmt::format("Expected ',' or ')' in parameter list"), "", Peek().line, Peek().column});
                break;
            }
        }
        if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::RightParen) {
            Advance(); // consume ')'
        } else {
            errors.push_back({fmt::format("Expected ')' at end of parameter list"), "", Peek().line, Peek().column});
        }

        return parameters;
    }

    std::vector<Ast::Attribute *> Parser::parseAttributes() {
        std::vector<Ast::Attribute *> attributes;
        while (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::At) {
            Advance(); // consume '@'
            if (Match(Token::Kind::Identifier).kind != Token::Kind::Identifier) {
                errors.push_back({fmt::format("Expected attribute name after '@'"), "", Peek().line, Peek().column});
                break;
            }

            std::string attrName = std::get<std::string>(Match(Token::Kind::Identifier).value);
            Advance(); // consume attribute name

            std::vector<Ast::Expression *> args;
            if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::LeftParen) {
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

                    fields.push_back(new Ast::VariableDeclaration(fieldName, fieldType, nullptr, {}, Peek().line, Peek().column));
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

    std::vector<Ast::VariableDeclaration *> Parser::parseVariableDeclarations() {
        currentIndex = 0;
        std::vector<Ast::VariableDeclaration *> variables;
        while (!IsAtEnd()) {
            // A variable is defined as "let name: type = initializer;", where "let" is a keyword, ": type" is required and "= initializer" is optional.
            auto attributes = parseAttributes();
            if ((Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) && std::get<Keyword>(Match(Token::Kind::Keyword).value) == Keyword::Let && GetScopeDepth() == 0) {
                Advance(); // consume 'let'
                auto varDecl = parseVariableDeclaration(attributes);
                if (varDecl) {
                    variables.push_back(varDecl);
                } else {
                    for (auto* attr : attributes) {
                        delete attr;
                    }
                }
            } else {
                Advance();
                for (auto* attr : attributes) {
                    delete attr;
                }
            }
        }

        return variables;
    }

    std::vector<Ast::Function *> Parser::parseFunctions() {
        // Functions are defined as "fn name(params) -> returnType { body }" or "fn name(params) { body }" if the return type is void, note that the body is optional, and can be ; instead of { body } for function declarations without a body.
        currentIndex = 0;
        std::vector<Ast::Function *> functions;
        while (!IsAtEnd()) {
            auto attributes = parseAttributes();
            if ((Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) && std::get<Keyword>(Match(Token::Kind::Keyword).value) == Keyword::Fn) {
                Advance(); // consume 'fn'
            }
            else {
                Advance();
                for (auto* attr : attributes) {
                    delete attr;
                }
                continue;
            }

            if (Match(Token::Kind::Identifier).kind != Token::Kind::Identifier) {
                if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::LeftParen) {
                    Advance(); // This is a function type, not a function declaration, so we should not report an error here.
                    for (auto* attr : attributes) {
                        delete attr;
                    }
                    continue;
                }

                errors.push_back({fmt::format("Expected function name after 'fn'"), "", Peek().line, Peek().column});
                for (auto* attr : attributes) {
                    delete attr;
                }
                continue;
            }

            std::string funcName = std::get<std::string>(Match(Token::Kind::Identifier).value);
            Advance(); // consume function name

            std::vector<Ast::VariableDeclaration *> parameters;
            bool isVariadic = false;
            if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::LeftParen) {
                Advance(); // consume '('

                while (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightParen) {
                    if (IsAtEnd()) {
                        errors.push_back({fmt::format("Unexpected end of file while parsing function parameters"), "", Peek().line, Peek().column});
                        break;
                    }

                    if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Variadic){
                        Advance(); // consume '...'
                        if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightParen) {
                            errors.push_back({fmt::format("Expected ')' after '...' in parameter list"), "", Peek().line, Peek().column});
                        }
                        isVariadic = true;
                        break;
                    }

                    auto param = parseVariableDeclaration({}); // A parameter can not have attributes.
                    if (param) {
                        parameters.push_back(param);
                    }

                    if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Comma) {
                        Advance(); // consume ','
                    } else if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::RightParen) {
                        errors.push_back({fmt::format("Expected ',' or ')' in parameter list"), "", Peek().line, Peek().column});
                        break;
                    }
                }
            } else {
                errors.push_back({fmt::format("Expected '(' after function name in function declaration"), "", Peek().line, Peek().column});
                for (auto* attr : attributes) {
                    delete attr;
                }
                continue;
            }

            Advance(); // consume ')'

            Type* returnType = nullptr;
            if (Peek().kind == Token::Kind::Operator && std::get<Operator>(Peek().value) == Operator::Arrow) {
                Advance(); // consume '->'
                returnType = parseType();
            } else if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::LeftBrace) {
                returnType = new Type(LiteralType::Void);
            } else if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::Semicolon) {
                returnType = new Type(LiteralType::Void);
            } else {
                errors.push_back({fmt::format("Expected '->', '{{' or ';' after function parameters"), "", Peek().line, Peek().column});
                for (auto* attr : attributes) {
                    delete attr;
                }
                for (auto* param : parameters) {
                    delete param;
                }
                continue;
            }

            std::vector<Ast::Node *> body;
            if (Peek().kind == Token::Kind::Syntax && std::get<Syntax>(Peek().value) == Syntax::LeftBrace) {
                Advance(); // consume '{'
                BodyParser bodyParser(tokens, currentIndex, &errors);
                body = bodyParser.ParseBody();
                currentIndex = bodyParser.GetCurrentIndex();
            }

            functions.push_back(new Ast::Function(funcName, parameters, isVariadic, attributes, returnType, body, Peek().line, Peek().column));
        }

        return functions;
    }

    std::vector<Ast::ImportStatement *> Parser::parseImports() {
        // Imports are defined as 'import "module/path" as alias;'. The 'as alias' part is required.
        currentIndex = 0;
        std::vector<Ast::ImportStatement *> imports;
        while (!IsAtEnd()) {
            // We do NOT have attributes for import statements, so we can just check for the 'import' keyword directly.
            if ((Match(Token::Kind::Keyword).kind == Token::Kind::Keyword) && std::get<Keyword>(Match(Token::Kind::Keyword).value) == Keyword::Import) {
                Advance(); // consume 'import'

                auto pathToken = Match(Token::Kind::StringLiteral);
                if (pathToken.kind != Token::Kind::StringLiteral) {
                    errors.push_back({fmt::format("Expected string literal for module path in import statement"), "", Peek().line, Peek().column});
                    continue;
                }

                Advance(); // consume module path

                if (Peek().kind != Token::Kind::Keyword || std::get<Keyword>(Peek().value) != Keyword::As) {
                    errors.push_back({fmt::format("Expected 'as' in import statement"), "", Peek().line, Peek().column});
                    continue;
                }

                Advance(); // consume 'as'

                auto aliasToken = Match(Token::Kind::Identifier);
                if (aliasToken.kind != Token::Kind::Identifier) {
                    errors.push_back({fmt::format("Expected identifier for alias in import statement"), "", Peek().line, Peek().column});
                    continue;
                }

                Advance(); // consume alias

                if (Peek().kind != Token::Kind::Syntax || std::get<Syntax>(Peek().value) != Syntax::Semicolon) {
                    errors.push_back({fmt::format("Expected ';' at end of import statement"), "", Peek().line, Peek().column});
                    continue;
                }

                Advance(); // consume ';'

                std::string modulePath = std::get<std::string>(pathToken.value);
                std::string alias = std::get<std::string>(aliasToken.value);
                imports.push_back(new Ast::ImportStatement(modulePath, alias, Peek().line, Peek().column));
            }
            else {
                Advance();
            }
        }

        return imports;
    }
} // Oxy