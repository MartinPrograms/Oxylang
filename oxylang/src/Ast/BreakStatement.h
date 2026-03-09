#ifndef OXYLANG_BREAKSTATEMENT_H
#define OXYLANG_BREAKSTATEMENT_H

#include "Node.h"

namespace Oxy::Ast {
    class BreakStatement : public Node {
    public:
        BreakStatement(int line, int column) : Node(line, column) {}
        ~BreakStatement() override = default;

        std::string ToString() const override {
            return "break;";
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }
    };

    class ContinueStatement : public Node {
    public:
        ContinueStatement(int line, int column) : Node(line, column) {}
        ~ContinueStatement() override = default;

        std::string ToString() const override {
            return "continue;";
        }
        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }
    };
}

#endif //OXYLANG_BREAKSTATEMENT_H