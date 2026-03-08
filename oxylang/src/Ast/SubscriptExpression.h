#ifndef OXYLANG_SUBSCRIPTEXPRESSION_H
#define OXYLANG_SUBSCRIPTEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class SubscriptExpression : public Expression {
    public:
        SubscriptExpression(Expression* array, Expression* index, int line, int column) : Expression(line, column), array(array), index(index) {}
        ~SubscriptExpression() override = default;

        std::string ToString() const override {
            return array->ToString() + "[" + index->ToString() + "]";
        }
    private:
        Expression* array;
        Expression* index;
    };
}

#endif //OXYLANG_SUBSCRIPTEXPRESSION_H