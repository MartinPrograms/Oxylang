#ifndef OXYLANG_FORSTATEMENT_H
#define OXYLANG_FORSTATEMENT_H

#include "Node.h"
#include "NestedExpression.h"
#include <vector>

namespace Oxy::Ast {
    class ForStatement : public Node {
    public:
        ForStatement(Node* initializer, Expression* condition, Node* increment, std::vector<Node*> body, int line, int column)
            : Node(line, column), initializer(initializer), condition(condition), increment(increment), body(std::move(body)) {}

        std::string ToString() const override {
            std::string result = "for (" + initializer->ToString() + "; " + (condition ? condition->ToString() : "true") + "; " + increment->ToString() + ") {\n";
            for (const auto& stmt : body) {
                result += "  " + stmt->ToString() + "\n";
            }
            result += "}";
            return result;
        }


        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] Node* GetInitializer() const { return initializer; }
        [[nodiscard]] Expression* GetCondition() const { return condition; }
        [[nodiscard]] Node* GetIncrement() const { return increment; }
        [[nodiscard]] const std::vector<Node*>& GetBody() const { return body; }

    private:
        Node* initializer; // This can be a variable declaration or an expression statement.
        Expression* condition; // This is the loop condition. If it's nullptr, it means the condition is always true.
        Node* increment; // This is the increment step. It can be an expression statement or a variable declaration.
        std::vector<Node*> body;
    };
}

#endif //OXYLANG_FORSTATEMENT_H