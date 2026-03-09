#ifndef OXYLANG_VISITOR_H
#define OXYLANG_VISITOR_H
#include "Definitions.h"

// Forward declarations
namespace Oxy::Ast {
    class Root;
    class Function;
    class VariableDeclaration;
    class StructDeclaration;
    class Attribute;
    class LiteralExpression;
    class AssignmentExpression;
    class ReturnStatement;
    class BinaryExpression;
    class SizeOfExpression;
    class TypeOfExpression;
    class AlignOfExpression;
    class AddressOfExpression;
    class DereferenceExpression;
    class CastExpression;
    class AllocateExpression;
    class FreeStatement;
    class UnaryExpression;
    class FunctionCallExpression;
    class PostfixExpression;
    class SubscriptExpression;
    class MemberAccessExpression;
    class IdentifierExpression;
    class NestedExpression;
    class BreakStatement;
    class IfStatement;
    class ExpressionStatement;
    class WhileStatement;
    class ForStatement;
    class ContinueStatement;
    class ImportStatement;
}

namespace Oxy {
    class Visitor {
    public:
        virtual ~Visitor() = default;

        virtual void Visit(Ast::Root* root) = 0;

        // Visit functions for every node type in the AST.
        virtual void Visit(Ast::Function* function) = 0;
        virtual void Visit(Ast::VariableDeclaration* variableDeclaration) = 0;
        virtual void Visit(Ast::StructDeclaration* structDeclaration) = 0;
        virtual void Visit(Ast::Attribute* attribute) = 0;
        virtual void Visit(Ast::LiteralExpression* literalExpression) = 0;
        virtual void Visit(Ast::AssignmentExpression* assignmentExpression) = 0;
        virtual void Visit(Ast::ReturnStatement* returnStatement) = 0;
        virtual void Visit(Ast::BinaryExpression* binaryExpression) = 0;
        virtual void Visit(Ast::SizeOfExpression* sizeOfExpression) = 0;
        virtual void Visit(Ast::TypeOfExpression* typeOfExpression) = 0;
        virtual void Visit(Ast::AlignOfExpression* alignOfExpression) = 0;
        virtual void Visit(Ast::AddressOfExpression* addressOfExpression) = 0;
        virtual void Visit(Ast::DereferenceExpression* dereferenceExpression) = 0;
        virtual void Visit(Ast::CastExpression* castExpression) = 0;
        virtual void Visit(Ast::AllocateExpression* allocateExpression) = 0;
        virtual void Visit(Ast::FreeStatement* freeStatement) = 0;
        virtual void Visit(Ast::UnaryExpression* unaryExpression) = 0;
        virtual void Visit(Ast::FunctionCallExpression* functionCallExpression) = 0;
        virtual void Visit(Ast::PostfixExpression* postfixExpression) = 0;
        virtual void Visit(Ast::SubscriptExpression* subscriptExpression) = 0;
        virtual void Visit(Ast::MemberAccessExpression* memberAccessExpression) = 0;
        virtual void Visit(Ast::IdentifierExpression* identifierExpression) = 0;
        virtual void Visit(Ast::NestedExpression* nestedExpression) = 0;
        virtual void Visit(Ast::BreakStatement* breakStatement) = 0;
        virtual void Visit(Ast::IfStatement* ifStatement) = 0;
        virtual void Visit(Ast::ExpressionStatement* expressionStatement) = 0;
        virtual void Visit(Ast::WhileStatement* whileStatement) = 0;
        virtual void Visit(Ast::ForStatement* forStatement) = 0;
        virtual void Visit(Ast::ImportStatement* importStatement) = 0;
        virtual void Visit(Ast::ContinueStatement* continueStatement) = 0;
        virtual void Visit(Type* type) = 0;
    };
}

#endif //OXYLANG_VISITOR_H