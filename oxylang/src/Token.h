#ifndef OXYLANG_TOKEN_H
#define OXYLANG_TOKEN_H

#include <variant>
#include <cstdint>

#include "Definitions.h"

namespace Oxy {
    class Token {
    public:
        enum class Kind {
            Keyword,      // fn, let, return, if, true, addr
            TypeName,     // i8, u8, i32, f32, void, ptr
            Identifier,   // myVariable, myFunction
            IntLiteral,   // 42, 0xFF
            FloatLiteral, // 3.14, 1e10
            StringLiteral,
            Operator,     // +, -, ==, <=, ...
            Syntax,       // { } ; , ( ) [ ]
            EndOfFile
        };

        using Value = std::variant<std::monostate, Keyword, LiteralType, Operator, Syntax, std::string, uint64_t, double, char>;

        Token(Kind kind, Value value, LiteralType literalType, int line, int column) : kind(kind), value(std::move(value)), literalType(literalType), line(line), column(column) {}

        static Token MakeKeyword(Keyword kw, int line, int col) {
            return {Kind::Keyword, kw, LiteralType::Undefined, line, col};
        }

        static Token MakeTypeName(LiteralType lt, int line, int col) {
            return {Kind::TypeName, lt, LiteralType::Undefined, line, col};
        }

        static Token MakeIdentifier(std::string name, int line, int col) {
            return {Kind::Identifier, std::move(name), LiteralType::Undefined, line, col};
        }

        static Token MakeInt(uint64_t val, LiteralType lt, int line, int col) {
            return {Kind::IntLiteral, val, lt, line, col};
        }

        static Token MakeFloat(double val, LiteralType lt, int line, int col) {
            return {Kind::FloatLiteral, val, lt, line, col};
        }

        static Token MakeString(std::string val, int line, int col) {
            return {Kind::StringLiteral, std::move(val), LiteralType::Pointer, line, col};
        }

        static Token MakeOperator(Operator op, int line, int col) {
            return {Kind::Operator, op, LiteralType::Undefined, line, col};
        }

        static Token MakeSyntax(Syntax syn, int line, int col) {
            return {Kind::Syntax, syn, LiteralType::Undefined, line, col};
        }

        static Token MakeEOF(int line, int col) {
            return {Kind::EndOfFile, std::monostate{}, LiteralType::Undefined, line, col};
        }

        std::string ToString() const {
            // Return as Type('value')
            switch (kind) {
                case Kind::Keyword:
                    return "Keyword('" + KeywordToString[std::get<Keyword>(value)] + "')";
                case Kind::TypeName:
                    return "TypeName('" + LiteralToString[std::get<LiteralType>(value)] + "')";
                case Kind::Identifier:
                    return "Identifier('" + std::get<std::string>(value) + "')";
                case Kind::IntLiteral:
                    return "IntLiteral('" + std::to_string(std::get<uint64_t>(value)) + ", " + LiteralToString[literalType] + "')";
                case Kind::FloatLiteral:
                    return "FloatLiteral('" + std::to_string(std::get<double>(value)) + ", " + LiteralToString[literalType] + "')";
                case Kind::StringLiteral:
                    return "StringLiteral(\"" + std::get<std::string>(value) + "\")";
                case Kind::Operator:
                    return "Operator('" + OperatorToString[std::get<Operator>(value)] + "')";
                case Kind::Syntax:
                    return "Syntax('" + SyntaxToString[std::get<Syntax>(value)] + "')";
                case Kind::EndOfFile:
                    return "EndOfFile";
            }
            return "UnknownToken";
        }

        Kind kind;
        Value value;
        LiteralType literalType; // Only used for literals, to distinguish between int and floats.
        int line;
        int column;
    };
}

#endif //OXYLANG_TOKEN_H