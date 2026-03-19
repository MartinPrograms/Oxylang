#include "Compiler.h"

#include <fstream>
#include <spdlog/spdlog.h>

#include "../../oxylang/src/Error.h"
#include "../../oxylang/src/Parser.h"
#include "../../oxylang/src/SemanticAnalyzer.h"
#include "../../oxylang/src/StructType.h"
#include "../../oxylang/src/Tokenizer.h"
#include "../../oxylang/src/CodeGenerator.h"

Compiler::Compiler(Options options) : options(std::move(options)) {
}

int Compiler::Compile() {
    spdlog::info("Compiling {} to {}", options.inputFile, options.outputFile);

    Oxy::Tokenizer tokenizer(options.sourceCode);
    auto tokens = tokenizer.Tokenize();
    spdlog::info("Tokenization complete. {} tokens found.", tokens.size());

    auto errors = tokenizer.GetErrors();
    if (!errors.empty()) {
        spdlog::error("Tokenization failed with {} errors:", errors.size());
        for (const auto& error : errors) {
            spdlog::error(error.ToString());
        }

        return 1;
    }

    // Log all tokens
    for (const auto& token: tokens) {
        spdlog::info("[{}:{}] Token: {}", token.line, token.column, token.ToString());
    }

    auto parser = Oxy::Parser(tokens);
    auto ast = parser.Parse();
     spdlog::info("Parsing complete.");

    if (!parser.GetErrors().empty()) {
        spdlog::error("Parsing failed with {} errors:", parser.GetErrors().size());
        for (const auto& error : parser.GetErrors()) {
            spdlog::error(error.ToString());
        }
        return 1;
    }

    spdlog::info("Root AST:\n{}", ast.ToString());

    auto fileIdMap = options.fileIdMap;
    std::map<std::string, Oxy::ModuleData> moduleDataMap;
    for (const auto& [id, path] : fileIdMap) {
        try {
            auto jsonContent = [&]() -> std::string {
                std::ifstream inFile(path);
                if (!inFile) {
                    throw std::runtime_error("Could not open file: " + path);
                }
                std::stringstream buffer;
                buffer << inFile.rdbuf();
                return buffer.str();
            }();
            auto moduleData = Oxy::FromJson(jsonContent);
            moduleDataMap[id] = moduleData;
            spdlog::info("Loaded module data for '{}': {} imports, {} exports", id, moduleData.imports.size(), moduleData.exports.size());
        } catch (const std::exception& e) {
            spdlog::error("Failed to load module data for '{}' from path '{}': {}", id, path, e.what());
            return 1;
        }
    }

    auto analyzer = new Oxy::SemanticAnalyzer(moduleDataMap);
    auto analysis = analyzer->Analyze(&ast);

    if (!analyzer->GetErrors().empty()) {
        spdlog::error("Semantic analysis failed with {} errors:", analyzer->GetErrors().size());
        for (const auto& error : analyzer->GetErrors()) {
            spdlog::error(error.ToString());
        }
        return 1;
    }

    for (const auto &import : analysis.imports) {
        spdlog::info("Import: {} as {}", import.moduleName, import.alias);
    }

    std::string result = "Global Scope: \n";
    std::function<void(Oxy::SemanticAnalyzer::Scope*, int)> scopePrinter = [&](Oxy::SemanticAnalyzer::Scope* scope, int indent = 0) {
        std::string indentStr(indent * 2, ' ');
        for (const auto& symbolPair : scope->symbols) {
            const auto& symbol = symbolPair.second;
            if (symbol.type != nullptr) {
                result += indentStr + symbol.name + ": " + symbol.type->ToString() + "\n";
            }
            else {
                result += indentStr + symbol.name + ": <unknown type>\n";
            }
        }

        for (const auto& child : scope->children) {
            result += "\n" + indentStr + "{\n";
            scopePrinter(child, indent + 1);
            result += indentStr + "}\n";
        }
    };

    scopePrinter(analysis.globalScope, 0);
    spdlog::info("Symbol table:\n{}", result);

    auto codegen = new Oxy::CodeGenerator(analysis, !options.is32BitTarget, moduleDataMap);
    auto code = codegen->Generate(&ast);

    if (!codegen->GetErrors().empty()) {
        spdlog::error("Code generation failed with {} errors:", codegen->GetErrors().size());
        for (const auto& error : codegen->GetErrors()) {
            spdlog::error(error.ToString());
        }
        return 1;
    }

    // Write the generated code to the output file.
    std::ofstream outFile(options.outputFile);
    if (!outFile) {
        spdlog::error("Failed to open output file: {}", options.outputFile);
        return 1;
    }

    outFile << code;
    outFile.close();

    spdlog::info("Compilation successful. Output written to {}", options.outputFile);
    return 0;
}
