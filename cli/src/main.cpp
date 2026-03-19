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

    bool is32Bit = false;
    app.add_flag("--32", is32Bit, "Compile for 32-bit target");

    std::string fileIdentifiers;
    app.add_option("-f,--file-ids", fileIdentifiers, "Semicolon-separated list of other modules (e.g., 'oxy/math=/path/to/math.json')");

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

    std::map<std::string, std::string> fileIdMap;
    if (!fileIdentifiers.empty()) {
        std::istringstream ss(fileIdentifiers);
        std::string token;
        while (std::getline(ss, token, ';')) {
            auto eqPos = token.find('=');
            if (eqPos == std::string::npos) {
                spdlog::error("Invalid file identifier format: '{}'. Expected 'id=path'", token);
                return 1;
            }
            std::string id = token.substr(0, eqPos);
            std::string path = token.substr(eqPos + 1);
            fileIdMap[id] = path;
        }
    }

    Compiler compiler({inputSource, inputFile, outputFile, is32Bit, fileIdMap});
    return compiler.Compile();
}
