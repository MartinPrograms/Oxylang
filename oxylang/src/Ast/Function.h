#ifndef OXYLANG_FUNCTION_H
#define OXYLANG_FUNCTION_H

#include "Node.h"
#include <string>
#include <vector>
#include "../Definitions.h"

namespace Oxy::Ast {
    class Function : public Node {
    public:
        Function(std::string name, std::vector<VariableDeclaration *> parameters, Type *returnType, std::vector<Node *> body, int line, int column)
            : Node(line, column), name(std::move(name)), parameters(std::move(parameters)), returnType(returnType), body(std::move(body)) {}

        std::string ToString() const override {
            std::string result = "fn " + name + "(";
            for (size_t i = 0; i < parameters.size(); i++) {
                result += parameters[i]->ToString();
                if (i < parameters.size() - 1) {
                    result += ", ";
                }
            }
            result += ") -> " + returnType->ToString() + " {\n";
            for (const auto& stmt : body) {
                result += "  " + stmt->ToString() + "\n";
            }

            result += "}";
            return result;
        }
    private:
        std::string name;
        std::vector<VariableDeclaration *> parameters;
        Type *returnType;
        std::vector<Node *> body;
    };
}

#endif //OXYLANG_FUNCTION_H