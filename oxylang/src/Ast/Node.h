#ifndef OXYLANG_NODE_H
#define OXYLANG_NODE_H

#include <cstdint>
#include <string>
#include "../Visitor.h"

namespace Oxy::Ast {

    // An ast node is a base class for all nodes in the abstract syntax tree.
    class Node {
    public:
        explicit Node(int line, int column) : line(line), column(column) {}
        virtual ~Node() = default;
        [[nodiscard]] virtual std::string ToString() const = 0;

        [[nodiscard]] int GetLine() const { return line; }
        [[nodiscard]] int GetColumn() const { return column; }

        virtual void Accept(Visitor* visitor) = 0;
    private:
        int line;
        int column;
    };

    // An expression is a node that produces a value. It can be a literal, an identifier, a binary operation, etc.
    class Expression : public Node {
    public:
        Expression (int line, int column) : Node(line, column) {}
    };
}

#endif //OXYLANG_NODE_H