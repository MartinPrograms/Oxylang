#include "Project.h"

#include <fstream>
#include <sstream>
#include <toml++/toml.hpp>

std::unique_ptr<Project> LoadProjectConfig(const std::string &configPath) {
    toml::table config;
    try {
        config = toml::parse_file(configPath);
    } catch (const toml::parse_error &e) {
        throw std::runtime_error("Failed to parse project.toml: " + std::string(e.what()));
    }

    auto project = std::make_unique<Project>();

    // There are 4 regions, [project], [build], [linker], and [modules]

    // [project] contains name and version
    if (config.contains("project")) {
        const auto &projectTable = config["project"];

        auto name = projectTable["name"].value<std::string>();
        if (!name) {
            throw std::runtime_error("Project name is required in project.toml");
        }
        project->name = *name;

        auto versionStr = projectTable["version"].value<std::string>();
        if (!versionStr) {
            throw std::runtime_error("Project version is required in project.toml");
        }
        std::istringstream versionStream(*versionStr);
        char dot1, dot2;
        if (!(versionStream >> project->version.major >> dot1 >> project->version.minor >> dot2 >> project->version.patch) || dot1 != '.' || dot2 != '.') {
            throw std::runtime_error("Invalid version format in project.toml, expected major.minor.patch");
        }
    } else {
        throw std::runtime_error("Missing [project] section in project.toml");
    }

    // [build] contains buildType, outputType, outputPath, sourceFiles, includeDirs
    if (config.contains("build")) {
        const auto &buildTable = config["build"];

        auto buildTypeStr = buildTable["buildType"].value<std::string>();
        if (!buildTypeStr) {
            throw std::runtime_error("buildType is required in [build] section of project.toml");
        }
        if (*buildTypeStr == "Debug") {
            project->buildType = BuildType::Debug;
        } else if (*buildTypeStr == "Release") {
            project->buildType = BuildType::Release;
        } else {
            throw std::runtime_error("Invalid buildType in [build] section of project.toml, expected 'Debug' or 'Release'");
        }

        auto outputTypeStr = buildTable["outputType"].value<std::string>();
        if (!outputTypeStr) {
            throw std::runtime_error("outputType is required in [build] section of project.toml");
        }
        if (*outputTypeStr == "Executable") {
            project->outputType = OutputType::Executable;
        } else if (*outputTypeStr == "StaticLibrary") {
            project->outputType = OutputType::StaticLibrary;
        } else if (*outputTypeStr == "SharedLibrary") {
            project->outputType = OutputType::SharedLibrary;
        }

        auto outputPath = buildTable["outputPath"].value<std::string>();
        if (!outputPath) {
            throw std::runtime_error("outputPath is required in [build] section of project.toml");
        }
        project->outputPath = *outputPath;

        auto sourceFiles = buildTable["sourceFiles"].as_array();
        if (!sourceFiles) {
            throw std::runtime_error("sourceFiles is required in [build] section of project.toml");
        }
        for (const auto &file : *sourceFiles) {
            if (auto fileStr = file.value<std::string>()) {
                project->sourceFiles.push_back(*fileStr);
            }
        }
    } else {
        throw std::runtime_error("Missing [build] section in project.toml");
    }

    // [linker] contains libraries, libraryPaths, and flags
    if (config.contains("linker")) {
        const auto &linkerTable = config["linker"];

        auto libraries = linkerTable["libraries"].as_array();
        if (libraries) {
            for (const auto &lib : *libraries) {
                if (auto libStr = lib.value<std::string>()) {
                    project->linker.libraries.push_back(*libStr);
                }
            }
        }

        auto libraryPaths = linkerTable["libraryPaths"].as_array();
        if (libraryPaths) {
            for (const auto &path : *libraryPaths) {
                if (auto pathStr = path.value<std::string>()) {
                    project->linker.libraryPaths.push_back(*pathStr);
                }
            }
        }

        auto flags = linkerTable["flags"].as_array();
        if (flags) {
            for (const auto &flag : *flags) {
                if (auto flagStr = flag.value<std::string>()) {
                    project->linker.flags.push_back(*flagStr);
                }
            }
        }
    }

    // [modules] contains module name to file path mapping
    if (config.contains("modules")) {
        const auto &modulesTable = config["modules"].as_table();
        if (modulesTable) {
            for (const auto &[moduleName, modulePath] : *modulesTable) {
                if (auto pathStr = modulePath.value<std::string>()) {
                    project->modules[std::string(moduleName.str())] = *pathStr;
                }
            }
        }
    }

    return project;
}