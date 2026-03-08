#ifndef OXYLANG_ADDRESSOFEXPRESSION_H
#define OXYLANG_ADDRESSOFEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class AddressOfExpression : public Expression {
    public:
        explicit AddressOfExpression(Expression* operand, int line, int column) : Expression(line, column), operand(operand) {}
        ~AddressOfExpression() override = default;

        std::string ToString() const override {
            return "addr(" + operand->ToString() + ")";
        }
    private:
        Expression* operand;
    };

    class DereferenceExpression : public Expression {
    public:
        explicit DereferenceExpression(Expression* operand, int line, int column) : Expression(line, column), operand(operand) {}
        ~DereferenceExpression() override = default;

        std::string ToString() const override {
            return "deref(" + operand->ToString() + ")";
        }

    private:
        Expression* operand;
    };
}

#endif //OXYLANG_ADDRESSOFEXPRESSION_H