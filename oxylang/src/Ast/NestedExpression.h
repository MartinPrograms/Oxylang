#ifndef OXYLANG_NESTEDEXPRESSION_H
#define OXYLANG_NESTEDEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

// Nested expression is used for grouping expressions with parentheses. For example, in the expression (a + b) * c, the part (a + b) is a nested expression.
namespace Oxy::Ast {
    class NestedExpression : public Expression {
    public:
        explicit NestedExpression(Expression* inner, int line, int column) : Expression(line, column), inner(inner) {}
        ~NestedExpression() override = default;

        std::string ToString() const override {
            return "(" + inner->ToString() + ")";
        }

        Expression* GetInner() const { return inner; }
    private:
        Expression* inner;
    };
}

#endif //OXYLANG_NESTEDEXPRESSION_H