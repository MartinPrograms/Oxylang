project(OxyCLI LANGUAGES CXX)

file(GLOB_RECURSE CLI_SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_executable(OxyCLI ${CLI_SOURCES})

include(FetchContent)
FetchContent_Declare(
        CLI11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG v2.6.2
)

FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.17.0
)

FetchContent_MakeAvailable(CLI11 spdlog)

target_link_libraries(OxyCLI PRIVATE CLI11::CLI11 spdlog::spdlog)
target_link_libraries(OxyCLI PRIVATE Oxylang)