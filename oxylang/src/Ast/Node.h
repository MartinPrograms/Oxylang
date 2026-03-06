#ifndef OXYLANG_NODE_H
#define OXYLANG_NODE_H

namespace Oxy::Ast {

    // An ast node is a base class for all nodes in the abstract syntax tree.
    class Node {
    public:
        explicit Node(int line, int column) : line(line), column(column) {}
        virtual ~Node() = default;
        virtual std::string ToString() const = 0;
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