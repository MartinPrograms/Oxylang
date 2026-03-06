#ifndef OXYLANG_COMPILER_H
#define OXYLANG_COMPILER_H
#include <string>


class Compiler {
public:
    Compiler(const std::string& sourceCode, const std::string& filePath, const std::string& outputPath);

    void Compile();
private:
    std::string sourceCode;
    std::string filePath;
    std::string outputPath;
};


#endif //OXYLANG_COMPILER_H