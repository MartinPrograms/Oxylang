#ifndef OXYLANG_ERROR_H
#define OXYLANG_ERROR_H

#include <string>

namespace Oxy {
    class Error {
    public:
        Error(const std::string& message, const std::string& filePath, int line, int column)
            : message(message), filePath(filePath), line(line), column(column) {}

        std::string ToString() const {
            return filePath + ":" + std::to_string(line) + ":" + std::to_string(column) + ": " + message;
        }
    private:
        std::string message;
        std::string filePath;
        int line;
        int column;
    };
} // Oxy

#endif //OXYLANG_ERROR_H