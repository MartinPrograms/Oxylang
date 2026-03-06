#ifndef OXYLANG_TOKEN_H
#define OXYLANG_TOKEN_H

#include <variant>

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
            Syntax,       // { } ; ,
            Punctuation,  // ( ) [ ]
            EndOfFile
        };

        using Value = std::variant<std::monostate, Keyword, LiteralType, Operator, Syntax, std::string, uint64_t, double, char>;

        Token(Kind kind, Value value, int line, int column) : kind(kind), value(std::move(value)), line(line), column(column) {}

        static Token MakeKeyword(Keyword kw, int line, int col) {
            return {Kind::Keyword, kw, line, col};
        }

        static Token MakeTypeName(LiteralType lt, int line, int col) {
            return {Kind::TypeName, lt, line, col};
        }

        static Token MakeIdentifier(std::string name, int line, int col) {
            return {Kind::Identifier, std::move(name), line, col};
        }

        static Token MakeInt(uint64_t val, int line, int col) {
            return {Kind::IntLiteral, val, line, col};
        }

        static Token MakeFloat(double val, int line, int col) {
            return {Kind::FloatLiteral, val, line, col};
        }

        static Token MakeString(std::string val, int line, int col) {
            return {Kind::StringLiteral, std::move(val), line, col};
        }

        static Token MakeOperator(Operator op, int line, int col) {
            return {Kind::Operator, op, line, col};
        }

        static Token MakeSyntax(Syntax syn, int line, int col) {
            return {Kind::Syntax, syn, line, col};
        }

        static Token MakePunctuation(char c, int line, int col) {
            return {Kind::Punctuation, c, line, col};
        }

        static Token MakeEOF(int line, int col) {
            return {Kind::EndOfFile, std::monostate{}, line, col};
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
                    return "IntLiteral('" + std::to_string(std::get<uint64_t>(value)) + "')";
                case Kind::FloatLiteral:
                    return "FloatLiteral('" + std::to_string(std::get<double>(value)) + "')";
                case Kind::StringLiteral:
                    return "StringLiteral(\"" + std::get<std::string>(value) + "\")";
                case Kind::Operator:
                    return "Operator('" + OperatorToString[std::get<Operator>(value)] + "')";
                case Kind::Syntax:
                    return "Syntax('" + SyntaxToString[std::get<Syntax>(value)] + "')";
                case Kind::Punctuation:
                    return std::string("Punctuation('") + std::get<char>(value) + "')";
                case Kind::EndOfFile:
                    return "EndOfFile";
            }
            return "UnknownToken";
        }

        Kind kind;
        Value value;
        int line;
        int column;
    };
}

#endif //OXYLANG_TOKEN_H