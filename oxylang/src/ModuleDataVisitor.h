#ifndef OXYLANG_MODULEDATAVISITOR_H
#define OXYLANG_MODULEDATAVISITOR_H
#include <memory>

namespace Oxy {struct ModuleData;};

#include "Visitor.h"

namespace Oxy {
    class ModuleDataVisitor : public Visitor {
    public:
        explicit ModuleDataVisitor(std::string path);

        ~ModuleDataVisitor() override;
        void Visit(Ast::Root *root) override;
        void Visit(Ast::Function *function) override;
        void Visit(Ast::VariableDeclaration *variableDeclaration) override;
        void Visit(Ast::StructDeclaration *structDeclaration) override;
        void Visit(Ast::Attribute *attribute) override;
        void Visit(Ast::LiteralExpression *literalExpression) override;
        void Visit(Ast::AssignmentExpression *assignmentExpression) override;
        void Visit(Ast::ReturnStatement *returnStatement) override;
        void Visit(Ast::BinaryExpression *binaryExpression) override;
        void Visit(Ast::SizeOfExpression *sizeOfExpression) override;
        void Visit(Ast::TypeOfExpression *typeOfExpression) override;
        void Visit(Ast::AlignOfExpression *alignOfExpression) override;
        void Visit(Ast::AddressOfExpression *addressOfExpression) override;
        void Visit(Ast::DereferenceExpression *dereferenceExpression) override;
        void Visit(Ast::CastExpression *castExpression) override;
        void Visit(Ast::AllocateExpression *allocateExpression) override;
        void Visit(Ast::FreeStatement *freeStatement) override;
        void Visit(Ast::UnaryExpression *unaryExpression) override;
        void Visit(Ast::FunctionCallExpression *functionCallExpression) override;
        void Visit(Ast::PostfixExpression *postfixExpression) override;
        void Visit(Ast::SubscriptExpression *subscriptExpression) override;
        void Visit(Ast::MemberAccessExpression *memberAccessExpression) override;
        void Visit(Ast::IdentifierExpression *identifierExpression) override;
        void Visit(Ast::NestedExpression *nestedExpression) override;
        void Visit(Ast::BreakStatement *breakStatement) override;
        void Visit(Ast::IfStatement *ifStatement) override;
        void Visit(Ast::ExpressionStatement *expressionStatement) override;
        void Visit(Ast::WhileStatement *whileStatement) override;
        void Visit(Ast::ForStatement *forStatement) override;
        void Visit(Ast::ImportStatement *importStatement) override;
        void Visit(Ast::ContinueStatement *continueStatement) override;
        void Visit(Ast::DereferenceAssignmentStatement *dereferenceAssignmentStatement) override;
        void Visit(Ast::TypeExpression *typeExpression) override;
        void Visit(Ast::StructInitializerExpression *structInitializerExpression) override;
        void Visit(Ast::PointerMemberAccessExpression *pointerMemberAccessExpression) override;
        void Visit(Type *type) override;

        ModuleData *moduleData;

    private:
        std::string filePath;
    };
} // Oxy

#endif //OXYLANG_MODULEDATAVISITOR_H