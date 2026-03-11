#include "CodeGenerator.h"

#include "spdlog/spdlog.h"

namespace Oxy {
    CodeGenerator::~CodeGenerator() {
    }

    std::string CodeGenerator::Generate(Ast::Root *ast) {
        Visit(ast);
        if (!errors.empty()) {
            return "";
        }
        return module.emit();
    }

    void CodeGenerator::Visit(Ast::Root *root) {

    }

    void CodeGenerator::Visit(Ast::Function *function) {

    }

    void CodeGenerator::Visit(Ast::VariableDeclaration *variableDeclaration) {

    }

    void CodeGenerator::Visit(Ast::StructDeclaration *structDeclaration) {
        // TODO: Implement struct declarations in code generation.
    }

    void CodeGenerator::Visit(Ast::Attribute *attribute) {
        // Ignored.
    }

    void CodeGenerator::Visit(Ast::LiteralExpression *literalExpression) {
        // Ignored.
    }

    void CodeGenerator::Visit(Ast::AssignmentExpression *assignmentExpression) {

    }

    void CodeGenerator::Visit(Ast::ReturnStatement *returnStatement) {

    }

    void CodeGenerator::Visit(Ast::BinaryExpression *binaryExpression) {

    }

    void CodeGenerator::Visit(Ast::SizeOfExpression *sizeOfExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::TypeOfExpression *typeOfExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::AlignOfExpression *alignOfExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::AddressOfExpression *addressOfExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::DereferenceExpression *dereferenceExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::CastExpression *castExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::AllocateExpression *allocateExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::FreeStatement *freeStatement) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::UnaryExpression *unaryExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::FunctionCallExpression *functionCallExpression) {

    }

    void CodeGenerator::Visit(Ast::PostfixExpression *postfixExpression) {

    }

    void CodeGenerator::Visit(Ast::SubscriptExpression *subscriptExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::MemberAccessExpression *memberAccessExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::IdentifierExpression *identifierExpression) {

    }

    void CodeGenerator::Visit(Ast::NestedExpression *nestedExpression) {

    }

    void CodeGenerator::Visit(Ast::BreakStatement *breakStatement) {

    }

    void CodeGenerator::Visit(Ast::IfStatement *ifStatement) {

    }

    void CodeGenerator::Visit(Ast::ExpressionStatement *expressionStatement) {

    }

    void CodeGenerator::Visit(Ast::WhileStatement *whileStatement) {

    }

    void CodeGenerator::Visit(Ast::ForStatement *forStatement) {
    }

    void CodeGenerator::Visit(Ast::ImportStatement *importStatement) {
    }

    void CodeGenerator::Visit(Ast::ContinueStatement *continueStatement) {
    }

    void CodeGenerator::Visit(Type *type) {
    }

    void CodeGenerator::Visit(Ast::DereferenceAssignmentStatement *dereferenceAssignmentStatement) {

    }

    std::string CodeGenerator::escapeStringLiteral(const std::string &get) {
        std::string newStr;
        for (char c : get) {
            switch (c) {
                case '\n': newStr += "\\n"; break;
                case '\t': newStr += "\\t"; break;
                case '\r': newStr += "\\r"; break;
                case '\\': newStr += "\\\\"; break;
                case '\"': newStr += "\\\""; break;
                default: newStr += c; break;
            }
        }

        return newStr;
    }
} // Oxy