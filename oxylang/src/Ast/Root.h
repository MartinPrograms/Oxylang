#ifndef OXYLANG_ROOT_H
#define OXYLANG_ROOT_H

#include "Node.h"
#include "VariableDeclaration.h"
#include "Function.h"
#include "../Definitions.h"

// Root holds top level declarations like functions, structs, and global variables. It is the root of the abstract syntax tree.
namespace Oxy::Ast {
    class Root : public Node {
    public:
        Root(std::vector<VariableDeclaration *> variables, std::vector<Function *> functions, std::vector<StructDeclaration *> structs, int line, int column)
            : Node(line, column), variables(std::move(variables)), functions(std::move(functions)), structs(std::move(structs)) {}

        std::string ToString() const override {
            std::string result = "Root:\n";
            result += "Variables:\n";
            for (const auto& var : variables) {
                result += var->ToString() + "\n";
            }
            result += "Functions:\n";
            for (const auto& func : functions) {
                result += func->ToString() + "\n";
            }
            result += "Structs:\n";
            for (const auto& strct : structs) {
                result += strct->ToString() + "\n";
            }
            return result;
        }
    private:
        std::vector<VariableDeclaration *> variables;
        std::vector<Function *> functions;
        std::vector<StructDeclaration *> structs;
    };
}

#endif //OXYLANG_ROOT_H