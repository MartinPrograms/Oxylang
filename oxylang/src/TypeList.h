#ifndef OXYLANG_TYPELIST_H
#define OXYLANG_TYPELIST_H

#include "Definitions.h"
#include <vector>

namespace Oxy {
    class TypeList : public Type {
    public:
        explicit TypeList(std::string identifier, std::vector<Type*> types) : Type(LiteralType::UserDefined, 0, nullptr, identifier), types(std::move(types)) {}

        [[nodiscard]] std::string ToString() const override {
            std::string result = GetIdentifier() + "<";
            for (size_t i = 0; i < types.size(); i++) {
                result += types[i]->ToString();
                if (i < types.size() - 1) {
                    result += ", ";
                }
            }
            result += ">";
            return result;
        }

        [[nodiscard]] const std::vector<Type*>& GetTypes() const { return types; }
    private:
        std::vector<Type*> types;
    };
}

#endif //OXYLANG_TYPELIST_H