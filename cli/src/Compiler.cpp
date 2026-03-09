#include "Compiler.h"
#include <spdlog/spdlog.h>

#include "../../oxylang/src/Error.h"
#include "../../oxylang/src/Parser.h"
#include "../../oxylang/src/SemanticAnalyzer.h"
#include "../../oxylang/src/StructType.h"
#include "../../oxylang/src/Tokenizer.h"

Compiler::Compiler(const std::string &sourceCode, const std::string &filePath, const std::string &outputPath) : sourceCode(sourceCode), filePath(filePath), outputPath(outputPath) {
}

void Compiler::Compile() {
    spdlog::info("Compiling {} to {}", filePath, outputPath);

    Oxy::Tokenizer tokenizer(sourceCode);
    auto tokens = tokenizer.Tokenize();
    spdlog::info("Tokenization complete. {} tokens found.", tokens.size());

    auto errors = tokenizer.GetErrors();
    if (!errors.empty()) {
        spdlog::error("Tokenization failed with {} errors:", errors.size());
        for (const auto& error : errors) {
            spdlog::error(error.ToString());
        }

        return;
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
        return;
    }

    spdlog::info("Root AST:\n{}", ast.ToString());

    auto analyzer = new Oxy::SemanticAnalyzer();
    auto analysis = analyzer->Analyze(&ast);

    if (!analyzer->GetErrors().empty()) {
        spdlog::error("Semantic analysis failed with {} errors:", analyzer->GetErrors().size());
        for (const auto& error : analyzer->GetErrors()) {
            spdlog::error(error.ToString());
        }
        return;
    }

    for (const auto &import : analysis.imports) {
        spdlog::info("Import: {} as {}", import.moduleName, import.alias);
    }

    std::function<void(Oxy::SemanticAnalyzer::Scope*, int)> scopePrinter = [&](Oxy::SemanticAnalyzer::Scope* scope, int indent = 0) {
        std::string indentStr(indent * 2, ' ');
        for (const auto& symbolPair : scope->symbols) {
            const auto& symbol = symbolPair.second;
            spdlog::info("{}Symbol: {} (kind: {}, type: {}) at {}:{}", indentStr, symbol.name,
                symbol.kind == Oxy::SemanticAnalyzer::Symbol::Kind::Variable ? "Variable" :
                symbol.kind == Oxy::SemanticAnalyzer::Symbol::Kind::Function ? "Function" :
                symbol.kind == Oxy::SemanticAnalyzer::Symbol::Kind::Struct ? "Struct" :
                symbol.kind == Oxy::SemanticAnalyzer::Symbol::Kind::Parameter ? "Parameter" :
                symbol.kind == Oxy::SemanticAnalyzer::Symbol::Kind::Import ? "Import" : "Unknown",
                symbol.type ? symbol.type->ToString() : "<unknown>",
                symbol.line, symbol.column);
        }
        for (const auto& child : scope->children) {
            scopePrinter(child, indent + 1);
        }
    };

    spdlog::info("Symbol table:");
    scopePrinter(analysis.globalScope, 0);
/*
    auto codegen = Oxy::CodeGenerator();
    auto result = codegen.Generate(ast);

    if (!codegen.GetErrors().empty()) {
        spdlog::error("Code generation failed with {} errors:", codegen.GetErrors().size());
        for (const auto& error : codegen.GetErrors()) {
            spdlog::error(error.ToString());
        }
        return;
    }
*/

}
