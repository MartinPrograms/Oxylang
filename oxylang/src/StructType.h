#ifndef OXYLANG_STRUCTTYPE_H
#define OXYLANG_STRUCTTYPE_H

#include <string>

#include "Definitions.h"
#include <vector>


namespace Oxy::Ast {
    class StructType : public Type {
    public:
        struct Field {
            std::string name;
            Type* type;
        };

        StructType(std::string name, std::vector<Field> fields) : Type(LiteralType::UserDefined, 0, nullptr, name), fields(std::move(fields)), name(std::move(name)) {}

        [[nodiscard]] const std::vector<Field>& GetFields() const { return fields; }
        [[nodiscard]] const std::string& GetName() const { return name; }

         std::string ToString() const override {
             std::string result = "struct " + name + " {\n";
             for (const auto& field : fields) {
                 result += "  " + field.type->ToString() + " " + field.name + ";\n";
             }
             result += "}";
             return result;
         }
    private:
        std::vector<Field> fields;
        std::string name;
    };
}

#endif //OXYLANG_STRUCTTYPE_H