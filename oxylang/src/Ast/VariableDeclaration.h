#ifndef OXYLANG_VARIABLEDECLARATION_H
#define OXYLANG_VARIABLEDECLARATION_H

#include "Node.h"
#include <string>
#include <utility>
#include "../Definitions.h"
#include "Attribute.h"

namespace Oxy::Ast {
    class VariableDeclaration : public Node {
    public:
        VariableDeclaration(std::string name, Type *type, Expression *initializer, std::vector<Attribute*> attributes, int line, int column)
            : Node(line, column), name(std::move(name)), type(type), initializer(initializer), attributes(std::move(attributes)) {}

        std::string ToString() const override {
            std::string result = name;
            if (type) {
                result += ": " + type->ToString();
            } else {
                result += ": <inferred>";
            }

            if (initializer) {
                result += " = " + initializer->ToString();
            }
            for (auto* attr : attributes) {
                result = attr->ToString() + " " + result;
            }
            return result;
        }
    private:
        std::string name;
        Type *type;
        Expression *initializer;
        std::vector<Attribute *> attributes;
    };
}

#endif //OXYLANG_VARIABLEDECLARATION_H