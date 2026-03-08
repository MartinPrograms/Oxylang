#ifndef OXYLANG_IFSTATEMENT_H
#define OXYLANG_IFSTATEMENT_H

#include "Node.h"
#include "NestedExpression.h"
#include "Nodes.h"

namespace Oxy::Ast {
    class IfStatement : public Node {
    public:
        struct Branch {
            Expression* condition; // If this is nullptr, then it's the else branch.
            std::vector<Node*> body;
        };

        IfStatement(Branch mainBranch, std::vector<Branch> elseIfBranches, std::vector<Node*> elseBranch, int line, int column)
            : Node(line, column), mainBranch(std::move(mainBranch)), elseIfBranches(std::move(elseIfBranches)), elseBranch(std::move(elseBranch)) {}

        std::string ToString() const override {
            std::string result = "if " + mainBranch.condition->ToString() + " {\n";
            for (const auto& stmt : mainBranch.body) {
                result += "  " + stmt->ToString() + "\n";
            }
            result += "}";

            for (const auto& elseIf : elseIfBranches) {
                result += " else if " + elseIf.condition->ToString() + " {\n";
                for (const auto& stmt : elseIf.body) {
                    result += "  " + stmt->ToString() + "\n";
                }
                result += "}";
            }

            if (!elseBranch.empty()) {
                result += " else {\n";
                for (const auto& stmt : elseBranch) {
                    result += "  " + stmt->ToString() + "\n";
                }
                result += "}";
            }

            return result;
        }

    private:
        Branch mainBranch;
        std::vector<Branch> elseIfBranches;
        std::vector<Node*> elseBranch;
    };
}

#endif //OXYLANG_IFSTATEMENT_H