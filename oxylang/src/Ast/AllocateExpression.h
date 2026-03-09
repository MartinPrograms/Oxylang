#ifndef OXYLANG_ALLOCATEEXPRESSION_H
#define OXYLANG_ALLOCATEEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class AllocateExpression : public Expression {
    public:
        AllocateExpression(Type* type, Expression *count, int line, int column) : Expression(line, column), type(type), count(count) {}
        ~AllocateExpression() override = default;

        std::string ToString() const override {
            if (count) {
                return "allocate(" + type->ToString() + ", " + count->ToString() + ")";
            } else {
                return "allocate(" + type->ToString() + ")";
            }
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Type* GetType() const { return type; }
        [[nodiscard]] Expression* GetCount() const { return count; }
    private:
        Type* type;
        Expression *count; // If count is nullptr, then it's a single allocation. Otherwise we allocate T * count.
    };

    class FreeStatement : public Node {
    public:
        explicit FreeStatement(Expression* pointer, int line, int column) : Node(line, column), pointer(pointer) {}
        ~FreeStatement() override = default;

        std::string ToString() const override {
            return "free(" + pointer->ToString() + ")";
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetPointer() const { return pointer; }
    private:
        Expression* pointer;
    };
}

#endif //OXYLANG_ALLOCATEEXPRESSION_H