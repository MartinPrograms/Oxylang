#ifndef OXYLANG_EXPRESSIONSTATEMENT_H
#define OXYLANG_EXPRESSIONSTATEMENT_H

#include "Node.h"

namespace Oxy::Ast {
    class ExpressionStatement : public Node {
    public:
        explicit ExpressionStatement(Expression* expression, int line, int column) : Node(line, column), expression(expression) {}
        ~ExpressionStatement() override = default;

        std::string ToString() const override {
            return expression->ToString() + ";";
        }

    private:
        Expression* expression;
    };
}

#endif //OXYLANG_EXPRESSIONSTATEMENT_H