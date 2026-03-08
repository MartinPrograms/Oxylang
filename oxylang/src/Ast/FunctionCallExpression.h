#ifndef OXYLANG_FUNCTIONCALLEXPRESSION_H
#define OXYLANG_FUNCTIONCALLEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"
#include <vector>

namespace Oxy::Ast {
    class FunctionCallExpression : public Expression {
    public:
        FunctionCallExpression(Expression* callee, std::vector<Expression*> arguments, int line, int column)
            : Expression(line, column), callee(callee), arguments(std::move(arguments)) {}

        std::string ToString() const override {
            std::string result = callee->ToString() + "(";
            for (size_t i = 0; i < arguments.size(); i++) {
                result += arguments[i]->ToString();
                if (i < arguments.size() - 1) {
                    result += ", ";
                }
            }
            result += ")";
            return result;
        }
    private:
        Expression* callee;
        std::vector<Expression*> arguments;
    };
}

#endif //OXYLANG_FUNCTIONCALLEXPRESSION_H