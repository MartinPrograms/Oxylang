#ifndef OXYLANG_RETURN_H
#define OXYLANG_RETURN_H

#include "Node.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class ReturnStatement : public Node {
    public:
        ReturnStatement(Expression* value, int line, int column) : Node(line, column), value(value) {}
        ~ReturnStatement() override = default;

        std::string ToString() const override {
            if (value) {
                return "return " + value->ToString() + ";";
            } else {
                return "return;";
            }
        }
    private:
        Expression* value;
    };
}

#endif //OXYLANG_RETURN_H