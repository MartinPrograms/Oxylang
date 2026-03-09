#ifndef OXYLANG_FUNCTION_H
#define OXYLANG_FUNCTION_H

#include "Node.h"
#include <string>
#include <utility>
#include <vector>
#include "../Definitions.h"

namespace Oxy::Ast {
    class Function : public Node {
    public:
        Function(std::string name, std::vector<VariableDeclaration *> parameters, bool isVariadic, std::vector<Attribute*> attributes, Type *returnType, std::vector<Node *> body, int line, int column)
            : Node(line, column), name(std::move(name)), parameters(std::move(parameters)), isVariadic(isVariadic), attributes(std::move(attributes)), returnType(returnType), body(std::move(body)) {}

        std::string ToString() const override {
            std::string result = "fn " + name + "(";

            for (const auto* attr : attributes) {
                result = attr->ToString() + " " + result;
            }

            for (size_t i = 0; i < parameters.size(); i++) {
                result += parameters[i]->ToString();
                if (i < parameters.size() - 1) {
                    result += ", ";
                }
            }

            if (isVariadic) {
                if (!parameters.empty()) {
                    result += ", ";
                }
                result += "...";
            }

            result += ") -> " + returnType->ToString();
            if (body.empty()) {
                result += ";";
                return result;
            }

            result += " {\n";

            for (const auto& stmt : body) {
                result += "  " + stmt->ToString() + "\n";
            }

            result += "}";
            return result;
        }

        [[nodiscard]] const std::string& GetName() const { return name; }
        [[nodiscard]] const std::vector<VariableDeclaration*>& GetParameters() const { return parameters; }
        [[nodiscard]] bool IsVariadic() const { return isVariadic; }
        [[nodiscard]] const std::vector<Attribute*>& GetAttributes() const { return attributes; }
        [[nodiscard]] Type* GetReturnType() const { return returnType; }
        [[nodiscard]] const std::vector<Node*>& GetBody() const { return body; }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }
    private:
        std::string name;
        std::vector<VariableDeclaration *> parameters;
        bool isVariadic;
        std::vector<Attribute *> attributes;
        Type *returnType;
        std::vector<Node *> body;
    };
}

#endif //OXYLANG_FUNCTION_H