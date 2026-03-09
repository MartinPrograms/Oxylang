#ifndef OXYLANG_IMPORTTYPE_H
#define OXYLANG_IMPORTTYPE_H

#include "Definitions.h"

namespace Oxy {
    class ImportType : public Type {
    public:
        explicit ImportType(std::string moduleName) : Type(LiteralType::UserDefined), moduleName(std::move(moduleName)) {}

        std::string ToString() const override {
            return "import " + moduleName;
        }

        [[nodiscard]] const std::string& GetModuleName() const { return moduleName; }
    private:
        std::string moduleName;
    };
}


#endif //OXYLANG_IMPORTTYPE_H