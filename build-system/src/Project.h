#ifndef OXYLANG_PROJECT_H
#define OXYLANG_PROJECT_H

#include <string>
#include <vector>
#include <map>
#include <memory>

struct Version {
    int major;
    int minor;
    int patch;
};

enum class BuildType {
    Debug,
    Release
};

enum class OutputType {
    Executable,
    StaticLibrary,
    SharedLibrary
};

struct LinkerConfig {
    std::vector<std::string> libraries;     // -l
    std::vector<std::string> libraryPaths;  // -L
    std::vector<std::string> flags;         // raw linker flags for edge cases
};

struct Project {
    std::string name; // Name of this project
    Version version; // Version of this project
    BuildType buildType; // Build type (Debug or Release)
    OutputType outputType; // Type of output (Executable, StaticLibrary, SharedLibrary)
    std::string outputPath; // Path to the output file (e.g., bin/myapp or lib/mylib.so)
    std::vector<std::string> sourceFiles; // List of source files (e.g., src/main.oxy, src/utils.oxy) can use glob patterns like src/**/*.oxy
    std::vector<std::string> includeDirs; // List of include directories (e.g., include/, src/)
    std::map<std::string, std::string> modules; // Map of module name to file path (e.g., "math" -> "src/math.oxy")
    LinkerConfig linker; // Linker configuration (libraries, library paths, and raw flags)
};

std::unique_ptr<Project> LoadProjectConfig(const std::string& configPath);

#endif //OXYLANG_PROJECT_H