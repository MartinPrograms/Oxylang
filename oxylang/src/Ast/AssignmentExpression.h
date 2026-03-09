#ifndef OXYLANG_ASSIGNMENT_H
#define OXYLANG_ASSIGNMENT_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class AssignmentExpression : public Expression {
    public:
        AssignmentExpression(Expression *left, Expression *right, int line, int column)
            : Expression(line, column), left(left), right(right) {}

        std::string ToString() const override {
            return left->ToString() + " = " + right->ToString();
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetLeft() const { return left; }
        [[nodiscard]] Expression* GetRight() const { return right; }
    private:
        Expression *left;
        Expression *right;
    };
}

#endif //OXYLANG_ASSIGNMENT_H