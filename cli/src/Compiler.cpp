#include "Compiler.h"
#include <spdlog/spdlog.h>

#include "../../oxylang/src/Error.h"
#include "../../oxylang/src/Parser.h"
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
}
