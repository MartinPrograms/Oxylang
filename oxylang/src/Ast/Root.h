#ifndef OXYLANG_ROOT_H
#define OXYLANG_ROOT_H

#include "Node.h"
#include "VariableDeclaration.h"
#include "Function.h"
#include "../Definitions.h"
#include "../Visitor.h"

// Root holds top level declarations like functions, structs, and global variables. It is the root of the abstract syntax tree.
namespace Oxy::Ast {
    class Root : public Node {
    public:
        Root(std::vector<VariableDeclaration *> variables, std::vector<Function *> functions, std::vector<StructDeclaration *> structs, std::vector<ImportStatement *> imports, int line, int column)
            : Node(line, column), functions(std::move(functions)), variables(std::move(variables)), structs(std::move(structs)), imports(std::move(imports)) {}

        std::string ToString() const override {
            std::string result = "Root:\n";

            result += "Functions:\n";
            for (const auto& func : functions) {
                result += func->ToString() + "\n";
            }

            result += "Variables:\n";
            for (const auto& var : variables) {
                result += var->ToString() + "\n";
            }

            result += "Structs:\n";
            for (const auto& strct : structs) {
                result += strct->ToString() + "\n";
            }

            result += "Imports:\n";
            for (const auto& imp : imports) {
                result += imp->ToString() + "\n";
            }

            return result;
        }

        [[nodiscard]] const std::vector<Function*>& GetFunctions() const { return functions; }
        [[nodiscard]] const std::vector<VariableDeclaration*>& GetVariables() const { return variables; }
        [[nodiscard]] const std::vector<StructDeclaration*>& GetStructs() const { return structs; }
        [[nodiscard]] const std::vector<ImportStatement*>& GetImports() const { return imports; }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }
    private:
        std::vector<Function *> functions;
        std::vector<VariableDeclaration *> variables;
        std::vector<StructDeclaration *> structs;
        std::vector<ImportStatement *> imports;
    };
}

#endif //OXYLANG_ROOT_H