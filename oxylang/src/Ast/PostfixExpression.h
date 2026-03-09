#ifndef OXYLANG_POSTFIXEXPRESSION_H
#define OXYLANG_POSTFIXEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class PostfixExpression : public Expression {
    public:
        PostfixExpression(Expression* operand, Operator op, int line, int column) : Expression(line, column), operand(operand), op(op) {}
        ~PostfixExpression() override = default;

        std::string ToString() const override {
            return "(" + operand->ToString() + OperatorToString.at(op) + ")";
        }


        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetOperand() const { return operand; }
        [[nodiscard]] Operator GetOperator() const { return op; }
    private:
        Expression* operand;
        Operator op; // This should only be ++ or --
    };
}

#endif //OXYLANG_POSTFIXEXPRESSION_H