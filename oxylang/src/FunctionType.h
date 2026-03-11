#ifndef OXYLANG_FUNCTIONTYPE_H
#define OXYLANG_FUNCTIONTYPE_H

#include <string>
#include "Definitions.h"

namespace Oxy {
    class FunctionType : public Type {
    public:
        explicit FunctionType (std::vector<Type*> parameterTypes, Type* returnType, bool isVariadic)
            : Type(LiteralType::Function), parameterTypes(std::move(parameterTypes)), returnType(returnType), isVariadic(isVariadic) {}

        [[nodiscard]] std::string ToString() const override {
            std::string result = "fn(";
            for (size_t i = 0; i < parameterTypes.size(); i++) {
                result += parameterTypes[i]->ToString();
                if (i < parameterTypes.size() - 1) {
                    result += ", ";
                }
            }
            if (isVariadic) {
                if (!parameterTypes.empty()) {
                    result += ", ";
                }
                result += "...";
            }
            result += ") -> " + returnType->ToString();
            return result;
        }

        [[nodiscard]] const std::vector<Type*>& GetParameterTypes() const { return parameterTypes; }
        [[nodiscard]] Type* GetReturnType() const { return returnType; }
        [[nodiscard]] bool IsVariadic() const { return isVariadic; }

        // Override the C++ is equal operator to compare function types based on their parameter and return types, as well as whether they are variadic.
        bool operator==(const FunctionType& other) const {
            if (isVariadic != other.isVariadic) {
                return false;
            }

            if (parameterTypes.size() != other.parameterTypes.size()) {
                return false;
            }

            for (size_t i = 0; i < parameterTypes.size(); i++) {
                if (*parameterTypes[i] != *other.parameterTypes[i]) {
                    return false;
                }
            }

            return *returnType == *other.returnType;
        }

    private:
        std::vector<Type*> parameterTypes;
        Type* returnType;
        bool isVariadic;
    };
}

#endif //OXYLANG_FUNCTIONTYPE_H