#ifndef OXYLANG_TYPEEXPRESSION_H
#define OXYLANG_TYPEEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"
#include <vector>

namespace Oxy::Ast {
    class TypeExpression : public Expression {
    public:
        explicit TypeExpression(std::string identifier, std::vector<Type *> genericArgs, int line, int column) : Expression(line, column), identifier(std::move(identifier)), genericArgs(std::move(genericArgs)) {}

        std::string ToString() const override {
            std::string result = identifier;
            if (!genericArgs.empty()) {
                result += "<";
                for (size_t i = 0; i < genericArgs.size(); i++) {
                    result += genericArgs[i]->ToString();
                    if (i < genericArgs.size() - 1) {
                        result += ", ";
                    }
                }
                result += ">";
            }
            return result;
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] const std::string& GetIdentifier() const { return identifier; }
        [[nodiscard]] const std::vector<Type*>& GetGenericArgs() const { return genericArgs; }

    private:
        std::string identifier;
        std::vector<Type *> genericArgs;
    };
}

#endif //OXYLANG_TYPEEXPRESSION_H