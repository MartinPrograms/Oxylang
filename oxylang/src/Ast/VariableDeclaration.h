#ifndef OXYLANG_VARIABLEDECLARATION_H
#define OXYLANG_VARIABLEDECLARATION_H

#include "Node.h"
#include <string>
#include "../Definitions.h"

namespace Oxy::Ast {
    class VariableDeclaration : public Node {
    public:
        VariableDeclaration(std::string name, Type *type, Expression *initializer, int line, int column)
            : Node(line, column), name(std::move(name)), type(type), initializer(initializer) {}

        std::string ToString() const override {
            std::string result = name + ": " + type->ToString();
            if (initializer) {
                result += " = " + initializer->ToString();
            }
            return result;
        }
    private:
        std::string name;
        Type *type;
        Expression *initializer;
    };
}

#endif //OXYLANG_VARIABLEDECLARATION_H