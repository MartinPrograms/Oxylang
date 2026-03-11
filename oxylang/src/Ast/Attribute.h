#ifndef OXYLANG_ATTRIBUTE_H
#define OXYLANG_ATTRIBUTE_H

#include "Node.h"
#include <string>
#include "../Definitions.h"
#include <vector>

namespace Oxy::Ast {
    class Attribute : public Node {
    public:
        Attribute(std::string name, std::vector<Expression*> args, int line, int column)
            : Node(line, column), name(std::move(name)), args(std::move(args)) {}

        std::string ToString() const override {
            std::string result = "@" + name;
            if (!args.empty()) {
                result += "(";
                for (size_t i = 0; i < args.size(); i++) {
                    result += args[i]->ToString();
                    if (i < args.size() - 1) {
                        result += ", ";
                    }
                }
                result += ")";
            }
            return result;
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] const std::string& GetName() const { return name; }
        [[nodiscard]] const std::vector<Expression*>& GetArgs() const { return args; }
    private:
        std::string name;
        std::vector<Expression*> args;
    };
}

#endif //OXYLANG_ATTRIBUTE_H