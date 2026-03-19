#ifndef OXYLANG_COMPILER_H
#define OXYLANG_COMPILER_H
#include <string>
#include <map>

class Compiler {
public:
    struct Options {
        std::string sourceCode;
        std::string inputFile;
        std::string outputFile;
        bool is32BitTarget;
        std::map<std::string, std::string> fileIdMap; // id -> file path
    };
    Compiler(Options options);

    int Compile();
private:
    Options options;
};


#endif //OXYLANG_COMPILER_H