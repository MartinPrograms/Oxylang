#include "Builder.h"

#include "../../oxylang/src/ModuleData.h"
#include "glob/glob.h"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <queue>
#include <unordered_map>

bool Builder::Build() {
    std::vector<std::string> allSourceFiles;
    for (const auto& pattern : project.sourceFiles) {
        auto matchedFiles = glob::rglob(pattern); // only time we pattern match is for source files, include dirs and module paths must be exact
        allSourceFiles.insert(allSourceFiles.end(), matchedFiles.begin(), matchedFiles.end());
    }

    for (auto &file : allSourceFiles) {
        spdlog::info("Found source file: {}", file);
    }

    if (allSourceFiles.empty()) {
        spdlog::error("No source files found for the project. Please check your project.toml configuration.");
        return false;
    }

    if (!std::filesystem::exists("./build")) {
        std::filesystem::create_directory("./build");
    }

    // First we create module data for each source file, which includes the module name, file path, imports and exports.
    std::vector<std::shared_ptr<Oxy::ModuleData>> allModuleData;
    std::map<std::string, Oxy::ModuleData*> moduleDataByPath;
    for (const auto& file : allSourceFiles) {
        try {
            auto moduleData = Oxy::CreateModuleData(file);
            spdlog::info("Module '{}' from file '{}' has {} imports and {} exports", moduleData->moduleName, file, moduleData->imports.size(), moduleData->exports.size());
            std::string json = Oxy::ToJson(*moduleData);
            std::string outputFilePath = "./build/" + moduleData->moduleName + ".json";
            std::ofstream outFile(outputFilePath);
            if (!outFile) {
                spdlog::error("Could not open output file for writing: {}", outputFilePath);
                return false;
            }
            outFile << json;
            outFile.close();
            allModuleData.push_back(std::move(moduleData));
            moduleDataByPath[allModuleData.back()->filePath] = allModuleData.back().get();
        } catch (const std::exception& e) {
            spdlog::error("Error processing file '{}': {}", file, e.what());
            return false;
        }
    }

    // Now we need to sort them based on their dependencies, so that we can build them in the correct order. We can use a simple topological sort for this.
    std::unordered_map<std::string, size_t> moduleIndex;
    for (size_t i = 0; i < allModuleData.size(); i++)
        moduleIndex[allModuleData[i]->moduleName] = i;

    std::unordered_map<std::string, std::vector<std::string>> dependents; // module -> modules that depend on it
    std::unordered_map<std::string, int> inDegree;

    for (const auto& mod : allModuleData) {
        if (!inDegree.count(mod->moduleName))
            inDegree[mod->moduleName] = 0;

        for (const auto& import : mod->imports) {
            // Only track dependencies that are within the project
            if (!moduleIndex.count(import.moduleName))
                continue;
            dependents[import.moduleName].push_back(mod->moduleName);
            inDegree[mod->moduleName]++;
        }
    }

    std::queue<std::string> ready;
    for (const auto& [name, degree] : inDegree)
        if (degree == 0)
            ready.push(name);

    std::vector<std::shared_ptr<Oxy::ModuleData>> sorted;
    sorted.reserve(allModuleData.size());

    while (!ready.empty()) {
        std::string current = ready.front();
        ready.pop();

        sorted.push_back(allModuleData[moduleIndex[current]]);

        for (const auto& dependent : dependents[current]) {
            if (--inDegree[dependent] == 0)
                ready.push(dependent);
        }
    }

    if (sorted.size() != allModuleData.size()) {
        spdlog::error("Circular dependency detected among modules.");
        return false;
    }

    allModuleData = std::move(sorted);

    // Now we know in what order to compile the modules, we can invoke the code generator for each module in order.
    for (const auto& moduleData : allModuleData) {
        spdlog::info("Building module '{}'", moduleData->moduleName);
        // arguments are --input (file.oxy) --output (file.o) -f "moduleNames=file.json;of;other;modules"
        std::string inputFile = moduleData->filePath;
        std::string outputFile = "./build/" + moduleData->moduleName + ".obj";
        std::string fileIdentifiers;
        for (const auto& mod : allModuleData) {
            if (!fileIdentifiers.empty()) {
                fileIdentifiers += ";";
            }
            fileIdentifiers += mod->moduleName + "=" + "./build/" + mod->moduleName + ".json";
        }
        std::string command = compilerExecutable + " --input " + inputFile + " --output " + outputFile + " -f \"" + fileIdentifiers + "\"";
        spdlog::info("Running command: {}", command);
        int result = system(command.c_str());
        if (result != 0) {
            spdlog::error("Failed to build module '{}'. Command '{}' exited with code {}", moduleData->moduleName, command, result);
            return false;
        }
    }

    return true;
}
