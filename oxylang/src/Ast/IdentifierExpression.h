#ifndef OXYLANG_IDENTIFIEREXPRESSION_H
#define OXYLANG_IDENTIFIEREXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class IdentifierExpression : public Expression {
    public:
        explicit IdentifierExpression(std::string name, int line, int column) : Expression(line, column), name(std::move(name)) {}
        ~IdentifierExpression() override = default;

        std::string ToString() const override {
            return name;
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }
    private:
        std::string name;
    };
}

#endif //OXYLANG_IDENTIFIEREXPRESSION_H