#ifndef OXYLANG_BUILDER_H
#define OXYLANG_BUILDER_H

#include "Project.h"

class Builder {
public:
    Builder(const Project& project, const std::string& compilerExecutable)
        : project(project), compilerExecutable(compilerExecutable) {}

    bool Build();

private:
    Project project;
    std::string compilerExecutable;
};


#endif //OXYLANG_BUILDER_H