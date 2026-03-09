#ifndef OXYLANG_SIZEOFEXPRESSION_H
#define OXYLANG_SIZEOFEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class SizeOfExpression : public Expression {
    public:
        explicit SizeOfExpression(Type* type, int line, int column) : Expression(line, column), type(type) {}
        ~SizeOfExpression() override = default;

        std::string ToString() const override {
            return "sizeof<" + type->ToString() + ">()";
        }
        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Type* GetType() const { return type; }

    private:
        Type* type;
    };

    class AlignOfExpression : public Expression {
    public:
        explicit AlignOfExpression(Type* type, int line, int column) : Expression(line, column), type(type) {}
        ~AlignOfExpression() override = default;

        std::string ToString() const override {
            return "alignof<" + type->ToString() + ">()";
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Type* GetType() const { return type; }
    private:
        Type* type;
    };

    class TypeOfExpression : public Expression {
    public:
        explicit TypeOfExpression(Expression* expr, int line, int column) : Expression(line, column), expr(expr) {}
        ~TypeOfExpression() override = default;

        std::string ToString() const override {
            return "typeof(" + expr->ToString() + ")";
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetExpression() const { return expr; }
    private:
        Expression* expr;
    };
}

#endif //OXYLANG_SIZEOFEXPRESSION_H