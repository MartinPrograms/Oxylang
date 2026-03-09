#ifndef OXYLANG_CASTEXPRESSION_H
#define OXYLANG_CASTEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class CastExpression : public Expression {
    public:
        CastExpression(Expression* operand, Type* targetType, int line, int column) : Expression(line, column), operand(operand), targetType(targetType) {}
        ~CastExpression() override = default;

        std::string ToString() const override {
            return "cast<" + targetType->ToString() + ">(" + operand->ToString() + ")";
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetOperand() const { return operand; }
        [[nodiscard]] Type* GetTargetType() const { return targetType; }
    private:
        Expression* operand;
        Type* targetType;
    };
}

#endif //OXYLANG_CASTEXPRESSION_H