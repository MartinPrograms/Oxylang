#ifndef OXYLANG_WHILESTATEMENT_H
#define OXYLANG_WHILESTATEMENT_H

#include "Node.h"
#include "NestedExpression.h"
#include <vector>

namespace Oxy::Ast {
    class WhileStatement : public Node {
    public:
        WhileStatement(Expression* condition, std::vector<Node*> body, int line, int column)
            : Node(line, column), condition(condition), body(std::move(body)) {}

        std::string ToString() const override {
            std::string result = "while " + condition->ToString() + " {\n";
            for (const auto& stmt : body) {
                result += "  " + stmt->ToString() + "\n";
            }
            result += "}";
            return result;
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Expression* GetCondition() const { return condition; }
        [[nodiscard]] const std::vector<Node*>& GetBody() const { return body; }
    private:
        Expression* condition;
        std::vector<Node*> body;
    };
}

#endif //OXYLANG_WHILESTATEMENT_H