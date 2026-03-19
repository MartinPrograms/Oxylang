project(oxybuild LANGUAGES CXX)

file(GLOB_RECURSE BUILD_SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_executable(oxybuild ${BUILD_SOURCES})

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

FetchContent_Declare(
        tomlpluslus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG v3.4.0
)

set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)
FetchContent_Declare(
        Glob
        GIT_REPOSITORY https://github.com/p-ranav/glob.git
        GIT_TAG master
)

FetchContent_MakeAvailable(CLI11 spdlog tomlpluslus Glob)

target_link_libraries(oxybuild PRIVATE CLI11::CLI11 spdlog::spdlog tomlplusplus::tomlplusplus Glob)
target_link_libraries(oxybuild PRIVATE Oxylang)