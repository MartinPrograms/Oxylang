#ifndef OXYLANG_BINARYEXPRESSION_H
#define OXYLANG_BINARYEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class BinaryExpression : public Expression {
    public:
        BinaryExpression(Expression *left, Operator op, Expression *right, int line, int column)
            : Expression(line, column), left(left), op(op), right(right) {}

        std::string ToString() const override {
            return "(" + left->ToString() + " " + OperatorToString.at(op) + " " + right->ToString() + ")";
        }

        Expression *GetLeft() const { return left; }
        Operator GetOperator() const { return op; }
        Expression *GetRight() const { return right; }
    private:
        Expression *left;
        Operator op;
        Expression *right;
    };
}

#endif //OXYLANG_BINARYEXPRESSION_H