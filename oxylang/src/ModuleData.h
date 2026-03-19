#ifndef OXYLANG_IMPORT_H
#define OXYLANG_IMPORT_H

#include <memory>
#include <string>
#include <vector>
#include <fstream>

#include <nlohmann/json.hpp>

#include "Interfaces.h"
#include "ModuleDataVisitor.h"
#include "Parser.h"
#include "References.h"
#include "Tokenizer.h"

namespace Oxy {
    struct Import {
        std::string moduleName; // Name of the imported module (e.g., "math")
        std::string filePath;   // File path to the module (e.g., "src/math.oxy")
    };

    enum class ExportType {
        Function,
        Struct,
        Variable
    };

    struct Export {
        ExportType type;       // Type of the export (function, struct, variable)
        std::string name;
        std::string symbolName; // The symbol name to be used for linking (can be the same as name or different if a @symbol attribute is used)

        union {
            struct {
                std::vector<std::string> *parameters; // For functions, the parameter types
                bool isVariadic; // For functions, whether it is variadic
                std::string *returnType; // For functions, the return type
            } functionInfo;
        };
    };

    struct ModuleData {
        std::string moduleName; // Name of the module (e.g., "math")
        std::string filePath;   // File path to the module (e.g., "src/math.oxy")
        std::vector<Import> imports; // List of imports in this module
        std::vector<Export> exports; // List of exports from this module
    };

    /// Create module data from a source file by parsing the file.
    inline std::unique_ptr<ModuleData> CreateModuleData(const std::string& filePath) {
        std::ifstream inFile(filePath);
        if (!inFile) {
            throw std::runtime_error("Could not open source file: " + filePath);
        }

        std::string source ((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        Oxy::Tokenizer tokenizer(source);
        auto tokens = tokenizer.Tokenize();
        auto parser = Oxy::Parser(tokens);
        auto ast = parser.Parse();

        auto m = ModuleDataVisitor(filePath);
        m.Visit(&ast);

        return std::make_unique<ModuleData>(*m.moduleData);
    }

    inline std::string ToJson(const ModuleData& moduleData) {
        nlohmann::json j;
        j["moduleName"] = moduleData.moduleName;
        j["filePath"] = moduleData.filePath;

        j["imports"] = nlohmann::json::array();
        for (const auto& import : moduleData.imports) {
            j["imports"].push_back({
                {"moduleName", import.moduleName},
                {"filePath", import.filePath}
            });
        }

        j["exports"] = nlohmann::json::array();
        for (const auto& export_ : moduleData.exports) {
            std::string typeStr;
            switch (export_.type) {
                case ExportType::Function: typeStr = "function"; break;
                case ExportType::Struct: typeStr = "struct"; break;
                case ExportType::Variable: typeStr = "variable"; break;
            }

            if (export_.type == ExportType::Function) {
                // For functions, we also want to include the parameter types and return type in the JSON
                std::vector<std::string> paramTypeStrs;
                for (const auto& paramType : *export_.functionInfo.parameters) {
                    paramTypeStrs.push_back(paramType);
                }
                j["exports"].push_back({
                    {"type", typeStr},
                    {"name", export_.name},
                    {"symbolName", export_.symbolName},
                    {"parameterTypes", paramTypeStrs},
                    {"returnType", *export_.functionInfo.returnType},
                    {"isVariadic", export_.functionInfo.isVariadic}
                });
            }
            else {
                j["exports"].push_back({
                    {"type", typeStr},
                    {"name", export_.name},
                    {"symbolName", export_.symbolName}
                });
            }

        }

        return j.dump(4); // Pretty print with 4 spaces indentation
    }

    inline ModuleData FromJson(const std::string& jsonStr) {
        auto j = nlohmann::json::parse(jsonStr);
        ModuleData moduleData;
        moduleData.moduleName = j["moduleName"];
        moduleData.filePath = j["filePath"];

        for (const auto& importJson : j["imports"]) {
            moduleData.imports.push_back({
                importJson["moduleName"],
                importJson["filePath"]
            });
        }

        for (const auto& exportJson : j["exports"]) {
            ExportType type;
            std::string typeStr = exportJson["type"];
            if (typeStr == "function") {
                type = ExportType::Function;
            } else if (typeStr == "struct") {
                type = ExportType::Struct;
            } else if (typeStr == "variable") {
                type = ExportType::Variable;
            } else {
                throw std::runtime_error("Unknown export type in JSON: " + typeStr);
            }
            moduleData.exports.push_back({
                type,
                exportJson["name"],
                exportJson["symbolName"],
                {
                    exportJson.contains("parameterTypes") ? new std::vector<std::string>(exportJson["parameterTypes"].get<std::vector<std::string>>()) : nullptr,
                    exportJson.contains("isVariadic") ? exportJson["isVariadic"].get<bool>() : false,
                    exportJson.contains("returnType") ? new std::string(exportJson["returnType"].get<std::string>()) : nullptr
                }
            });
        }

        return moduleData;
    }
}

#endif //OXYLANG_IMPORT_H