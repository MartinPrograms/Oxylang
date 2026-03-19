#include <iostream>
#include <string>
#include <fstream>
#include <CLI/CLI.hpp>

#include "Builder.h"
#include "Project.h"
#include "spdlog/spdlog.h"

// usage of the build system:
// `oxybuild` if pwd is the project directory, or `oxybuild -c /path/to/project` if not
// You can also override the used compiler with `oxybuild -c /path/to/project --compiler /path/to/compiler`

int main(int argc, char* argv[]) {
    CLI::App app{"OxyLang Build System"};

    std::string configDir;
    app.add_option("-c,--config", configDir, "Path to project directory");

    std::string compilerPath;
    app.add_option("--compiler", compilerPath, "Path to OxyLang compiler executable");

    CLI11_PARSE(app, argc, argv);

    // find a project.toml in the specified configDir or current directory
    std::string projectConfigPath;
    if (!configDir.empty()) {
        projectConfigPath = configDir + "/project.toml";
    } else {
        projectConfigPath = "project.toml";
    }

    if (!std::ifstream(projectConfigPath)) {
        std::cerr << "Error: Could not find project.toml in " << (configDir.empty() ? "current directory" : configDir) << std::endl;
        return 1;
    }

    std::string compilerExecutable = compilerPath.empty() ? "oxycli" : compilerPath;
    auto project = LoadProjectConfig(projectConfigPath);
    spdlog::info("Building project '{}' version {}.{}.{} in {} mode", project->name, project->version.major, project->version.minor, project->version.patch, project->buildType == BuildType::Debug ? "Debug" : "Release");
    spdlog::info("Output type: {}", project->outputType == OutputType::Executable ? "Executable" : (project->outputType == OutputType::StaticLibrary ? "Static Library" : "Shared Library"));
    spdlog::info("Output path: {}", project->outputPath);

    // Make the oxycli executable path absolute if it's not already, to ensure it can be found when we invoke it from the Builder.
    if (!std::filesystem::path(compilerExecutable).is_absolute()) {
        compilerExecutable = std::filesystem::absolute(compilerExecutable).string();
    }

    // Change the working directory to the project directory to ensure relative paths in project.toml work correctly
    std::string projectDir = configDir.empty() ? "." : configDir;
    if (chdir(projectDir.c_str()) != 0) {
        spdlog::error("Failed to change directory to project directory: {}", projectDir);
        return 1;
    }

    if (Builder(*project, compilerExecutable).Build()) {
        spdlog::info("Build succeeded!");
    } else {
        spdlog::error("Build failed!");
        return 1;
    }

    return 0;
}