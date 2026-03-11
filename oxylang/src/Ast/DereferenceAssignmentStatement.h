#ifndef OXYLANG_DEREFERENCEASSIGNMENTSTATEMENT_H
#define OXYLANG_DEREFERENCEASSIGNMENTSTATEMENT_H

#include "Node.h"
#include "NestedExpression.h"

namespace Oxy::Ast {
    class DereferenceAssignmentStatement : public Node {
    public:
        DereferenceAssignmentStatement(Expression* target, Expression* value, int line, int column)
            : Node(line, column), target(target), value(value) {}

        std::string ToString() const override {
            return "*" + target->ToString() + " = " + value->ToString() + ";";
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetTarget() const { return target; }
        [[nodiscard]] Expression* GetValue() const { return value; }
    private:
        Expression* target;
        Expression* value;
    };
}

#endif //OXYLANG_DEREFERENCEASSIGNMENTSTATEMENT_H