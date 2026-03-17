#ifndef OXYLANG_MEMBERACCESSEXPRESSION_H
#define OXYLANG_MEMBERACCESSEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class MemberAccessExpression : public Expression {
    public:
        MemberAccessExpression(Expression* object, std::string memberName, int line, int column)
            : Expression(line, column), object(object), memberName(std::move(memberName)) {}
        ~MemberAccessExpression() override = default;

        std::string ToString() const override {
            return object->ToString() + "." + memberName;
        }
        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetObject() const { return object; }
        [[nodiscard]] const std::string& GetMemberName() const { return memberName; }

    private:
        Expression* object;
        std::string memberName;
    };

    class PointerMemberAccessExpression : public Expression {
    public:
        PointerMemberAccessExpression(Expression* object, std::string memberName, int line, int column)
            : Expression(line, column), object(object), memberName(std::move(memberName)) {}
        ~PointerMemberAccessExpression() override = default;

        std::string ToString() const override {
            return object->ToString() + "->" + memberName;
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetObject() const { return object; }
        [[nodiscard]] const std::string& GetMemberName() const { return memberName; }

    private:
        Expression* object;
        std::string memberName;
    };
}

#endif //OXYLANG_MEMBERACCESSEXPRESSION_H