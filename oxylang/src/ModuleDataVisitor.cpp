#include "ModuleDataVisitor.h"
#include "ModuleData.h"

namespace Oxy {
    ModuleDataVisitor::ModuleDataVisitor(std::string path) {
        filePath = std::move(path);
        moduleData = new ModuleData();
        moduleData->filePath = filePath;
        moduleData->moduleName = filePath.substr(filePath.find_last_of("/\\") + 1);
        auto dotIndex = moduleData->moduleName.find_last_of('.');
        if (dotIndex != std::string::npos) {
            moduleData->moduleName = moduleData->moduleName.substr(0, dotIndex);
        }
    }

    ModuleDataVisitor::~ModuleDataVisitor() {
    }

    void ModuleDataVisitor::Visit(Ast::Root *root) {
        for (auto &import : root->GetImports()) {
            Visit(import);
        }

        for (auto &structDecl : root->GetStructs()) {
            Visit(structDecl);
        }

        for (auto &varDecl : root->GetVariables()) {
            Visit(varDecl);
        }

        for (auto &func : root->GetFunctions()) {
            Visit(func);
        }
    }

    void ModuleDataVisitor::Visit(Ast::Function *function) {
        bool isPublic = false;
        std::string symbolName = function->GetObfuscatedName();
        auto attributes = function->GetAttributes();
        for (auto &attr : attributes) {
            if (attr->GetName() == "public") {
                isPublic = true;
            }
            if (attr->GetName() == "symbol") {
                auto args = attr->GetArgs();
                if (args.size() == 1) {
                    if (auto *literal = dynamic_cast<Ast::LiteralExpression *>(args[0])) {
                        symbolName = std::get<std::string>(literal->GetValue());
                    }
                }
            }
        }

        if (!isPublic)
            return;

        std::string returnType = function->GetReturnType()->ToString();
        std::vector<std::string> parameterTypes;
        for (const auto& param : function->GetParameters()) {
            parameterTypes.push_back(param->GetType()->ToString());
        }

        moduleData->exports.push_back({ExportType::Function, function->GetName(), symbolName, {new std::vector<std::string>(parameterTypes), function->IsVariadic(), new std::string(returnType)}});
    }

    void ModuleDataVisitor::Visit(Ast::VariableDeclaration *variableDeclaration) {
        bool isPublic = false;
        auto attributes = variableDeclaration->GetAttributes();
        for (auto &attr : attributes) {
            if (attr->GetName() == "public") {
                isPublic = true;
                break;
            }
        }

        if (!isPublic)
            return;

        moduleData->exports.push_back({ExportType::Variable, variableDeclaration->GetName(), variableDeclaration->GetName(), {}});
    }

    void ModuleDataVisitor::Visit(Ast::StructDeclaration *structDeclaration) {
        bool isPublic = false;
        auto attributes = structDeclaration->GetAttributes();
        for (auto &attr : attributes) {
            if (attr->GetName() == "public") {
                isPublic = true;
                break;
            }
        }

        if (!isPublic)
            return;

        moduleData->exports.push_back({ExportType::Struct, structDeclaration->GetName(), structDeclaration->GetName(), {}});
    }

    void ModuleDataVisitor::Visit(Ast::Attribute *attribute) {
    }

    void ModuleDataVisitor::Visit(Ast::LiteralExpression *literalExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::AssignmentExpression *assignmentExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::ReturnStatement *returnStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::BinaryExpression *binaryExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::SizeOfExpression *sizeOfExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::TypeOfExpression *typeOfExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::AlignOfExpression *alignOfExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::AddressOfExpression *addressOfExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::DereferenceExpression *dereferenceExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::CastExpression *castExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::AllocateExpression *allocateExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::FreeStatement *freeStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::UnaryExpression *unaryExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::FunctionCallExpression *functionCallExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::PostfixExpression *postfixExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::SubscriptExpression *subscriptExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::MemberAccessExpression *memberAccessExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::IdentifierExpression *identifierExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::NestedExpression *nestedExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::BreakStatement *breakStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::IfStatement *ifStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::ExpressionStatement *expressionStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::WhileStatement *whileStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::ForStatement *forStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::ImportStatement *importStatement) {
        moduleData->imports.push_back({importStatement->GetAlias(), importStatement->GetModuleName()});
    }

    void ModuleDataVisitor::Visit(Ast::ContinueStatement *continueStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::DereferenceAssignmentStatement *dereferenceAssignmentStatement) {
    }

    void ModuleDataVisitor::Visit(Ast::TypeExpression *typeExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::StructInitializerExpression *structInitializerExpression) {
    }

    void ModuleDataVisitor::Visit(Ast::PointerMemberAccessExpression *pointerMemberAccessExpression) {
    }

    void ModuleDataVisitor::Visit(Type *type) {
    }
} // Oxy