#ifndef OXYLANG_UNARYEXPRESSION_H
#define OXYLANG_UNARYEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class UnaryExpression : public Expression {
    public:
        UnaryExpression(Operator op, Expression* operand, int line, int column) : Expression(line, column), op(op), operand(operand) {}
        ~UnaryExpression() override = default;

        std::string ToString() const override {
            return OperatorToString.at(op) + "(" + operand->ToString() + ")";
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Operator GetOperator() const { return op; }
        [[nodiscard]] Expression* GetOperand() const { return operand; }
    private:
        Operator op;
        Expression* operand;
    };
}

#endif //OXYLANG_UNARYEXPRESSION_H