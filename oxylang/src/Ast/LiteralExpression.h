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
                case LiteralType::Int:
                    return std::to_string(std::get<uint64_t>(value));
                case LiteralType::Float:
                    return std::to_string(std::get<double>(value));
                case LiteralType::Pointer:
                    return "\"" + std::get<std::string>(value) + "\"";
                default:
                    return "Unknown literal type";
            }
        }
    private:
        LiteralType type;
        std::variant<uint64_t, double, std::string, char> value;
    };
}

#endif //OXYLANG_LITERALEXPRESSION_H