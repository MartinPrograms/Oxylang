#ifndef OXYLANG_IMPORTEXPRESSION_H
#define OXYLANG_IMPORTEXPRESSION_H

#include "Node.h"
#include "../Definitions.h"

// import "module" as alias;

namespace Oxy::Ast {
    class ImportStatement : public Node {
    public:
        ImportStatement(std::string moduleName, std::string alias, int line, int column)
            : Node(line, column), moduleName(std::move(moduleName)), alias(std::move(alias)) {}

        std::string ToString() const override {
            return "import \"" + moduleName + "\" as " + alias + ";";
        }

        [[nodiscard]] const std::string& GetModuleName() const { return moduleName; }
        [[nodiscard]] const std::string& GetAlias() const { return alias; }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }
    private:
        std::string moduleName;
        std::string alias;
    };
}

#endif //OXYLANG_IMPORTEXPRESSION_H