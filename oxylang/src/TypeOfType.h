#ifndef OXYLANG_TYPEOFTYPE_H
#define OXYLANG_TYPEOFTYPE_H

#include "Definitions.h"
#include <string>
#include "Ast/Node.h"

namespace Oxy {
    class TypeOfType : public Type {
    public:
        explicit TypeOfType(Ast::Expression* type) : Type(LiteralType::TypeOf), type(type) {}

        [[nodiscard]] std::string ToString() const override {
            return "typeof(" + type->ToString() + ")";
        }

        [[nodiscard]] Ast::Expression* GetExpression() const { return type; }
    private:
        Ast::Expression* type;
    };
}

#endif //OXYLANG_TYPEOFTYPE_H