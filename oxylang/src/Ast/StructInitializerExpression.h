#ifndef OXYLANG_STRUCTINITIALIZEREXPRESSION_H
#define OXYLANG_STRUCTINITIALIZEREXPRESSION_H

#include "Node.h"
#include <string>

namespace Oxy::Ast {
    class StructInitializerExpression : public Expression {
    public:
        StructInitializerExpression(Ast::Expression* structIdentifier, std::vector<std::pair<std::string, Expression*>> initializers, int line, int column)
            : Expression(line, column), structIdentifier(structIdentifier), initializers(std::move(initializers)) {}

        std::string ToString() const override {
            std::string result = structIdentifier->ToString() + " { ";
            for (const auto& initializer : initializers) {
                result += initializer.first + ": " + initializer.second->ToString() + ", ";
            }
            if (!initializers.empty()) {
                result.pop_back(); // Remove last space
                result.pop_back(); // Remove last comma
            }
            result += " }";
            return result;
        }

        Ast::Expression* GetStructIdentifier() const { return structIdentifier; }
        const std::vector<std::pair<std::string, Expression*>>& GetInitializers() const { return initializers; }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }
    private:
        Ast::Expression *structIdentifier;
        std::vector<std::pair<std::string, Expression*>> initializers;
    };
}

#endif //OXYLANG_STRUCTINITIALIZEREXPRESSION_H