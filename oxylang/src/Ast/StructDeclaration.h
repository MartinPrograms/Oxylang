#ifndef OXYLANG_STRUCTDECLARATION_H
#define OXYLANG_STRUCTDECLARATION_H

#include "Node.h"
#include "VariableDeclaration.h"
#include <vector>

#include "Attribute.h"
#include "../Definitions.h"

namespace Oxy::Ast {
    class StructDeclaration : public Node {
    public:
        StructDeclaration(std::string name, std::vector<Attribute*> attributes, std::vector<VariableDeclaration *> fields, int line, int column)
            : Node(line, column), name(std::move(name)), attributes(std::move(attributes)), fields(std::move(fields)) {}

        std::string ToString() const override {
            std::string result = "struct " + name + " {\n";
            for (const auto& field : fields) {
                result += "  " + field->ToString() + "\n";
            }
            result += "}";
            for (auto* attr : attributes) {
                result = attr->ToString() + " " + result;
            }
            return result;
        }

        void Accept(Visitor* visitor) override {
            visitor->Visit(this);
        }

        [[nodiscard]] const std::string& GetName() const { return name; }
        [[nodiscard]] const std::vector<Attribute*>& GetAttributes() const { return attributes; }
        [[nodiscard]] const std::vector<VariableDeclaration*>& GetFields() const { return fields; }
    private:
        std::string name;
        std::vector<Attribute*> attributes;
        std::vector<VariableDeclaration *> fields;
    };
}

#endif //OXYLANG_STRUCTDECLARATION_H