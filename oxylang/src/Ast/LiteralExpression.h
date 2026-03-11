#ifndef OXYLANG_LITERALEXPRESSION_H
#define OXYLANG_LITERALEXPRESSION_H

#include <variant>

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class LiteralExpression : public Expression {
    public:
        LiteralExpression(LiteralType type, std::variant<uint64_t, double, std::string, char> value, int line, int column)
            : Expression(line, column), type(type), value(std::move(value)) {}

        std::string ToString() const override {
            switch (type) {
                case LiteralType::U8:
                case LiteralType::I8:
                case LiteralType::U16:
                case LiteralType::I16:
                case LiteralType::U32:
                case LiteralType::I32:
                case LiteralType::U64:
                case LiteralType::I64:
                    return std::to_string(std::get<uint64_t>(value)) + (LiteralToString[type]);

                case LiteralType::F32:
                case LiteralType::F64:
                    return std::to_string(std::get<double>(value)) + (LiteralToString[type]);

                case LiteralType::Pointer:
                    return "\"" + std::get<std::string>(value) + "\"";
                default:
                    return "Unknown literal type";
            }
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] LiteralType GetLiteralType() const { return type; }
        [[nodiscard]] const std::variant<uint64_t, double, std::string, char>& GetValue() const { return value; }
    private:
        LiteralType type;
        std::variant<uint64_t, double, std::string, char> value;
    };
}

#endif //OXYLANG_LITERALEXPRESSION_H