#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

#include "Compiler.h"
#include "spdlog/fmt/bundled/os.h"

int main(int argc, char* argv[]) {
    CLI::App app{"OxyLang CLI"};

    std::string inputFile;
    app.add_option("-i,--input", inputFile, "Input file")->required();

    std::string outputFile;
    app.add_option("-o,--output", outputFile, "Output file")->required();

    CLI11_PARSE(app, argc, argv);

    std::string inputSource;
    try {
        std::ifstream inFile(inputFile);
        if (!inFile) {
            spdlog::error("Could not open input file: {}", inputFile);
            return 1;
        }
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        inputSource = buffer.str();
    } catch (const std::exception& e) {
        spdlog::error("Failed to read input file: {}", e.what());
        return 1;
    }

    Compiler compiler(inputSource, inputFile, outputFile);
    compiler.Compile();
}
