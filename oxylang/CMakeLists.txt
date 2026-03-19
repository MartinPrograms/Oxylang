project(Oxylang LANGUAGES CXX)

file(GLOB_RECURSE OXYLANG_SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_library(Oxylang STATIC ${OXYLANG_SOURCES})

include(FetchContent)

FetchContent_Declare(
        QbeIR
        GIT_REPOSITORY https://github.com/MartinPrograms/QbeGenerator-cpp.git
        GIT_TAG master
)

FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
)

FetchContent_MakeAvailable(QbeIR json)

target_link_libraries(Oxylang
        PUBLIC
        QbeIR
        nlohmann_json::nlohmann_json
)