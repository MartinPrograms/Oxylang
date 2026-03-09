#ifndef OXYLANG_SEMANTICANALYZER_H
#define OXYLANG_SEMANTICANALYZER_H

#include <stack>

#include "./Ast/Nodes.h"
#include "Error.h"
#include "Definitions.h"
#include "Token.h"
#include "Visitor.h"
#include <vector>
#include <unordered_map>
#include <string>

namespace Oxy {
    class SemanticAnalyzer : public Visitor {
    public:
        struct Import {
            std::string moduleName;
            std::string alias;
        };

        struct Symbol {
            std::string name;
            Type* type{};
            enum class Kind { Variable, Function, Struct, Parameter, Import } kind;
            int line{};
            int column{};
        };

        class Scope {
        public:
            Scope* parent;
            std::vector<Scope *> children;
            std::unordered_map<std::string, Symbol> symbols;

            Scope(Scope* parent) : parent(parent) {}

            bool Define(const Symbol& symbol) {
                if (symbols.find(symbol.name) != symbols.end()) {
                    return false; // Symbol already defined in this scope
                }
                symbols[symbol.name] = symbol;
                return true;
            }

            Symbol *Resolve(const std::string& name) {
                if (symbols.find(name) != symbols.end()) {
                    return &symbols[name];
                }
                if (parent) {
                    return parent->Resolve(name);
                }
                return nullptr; // Not found
            }
        };

        struct AnalysisResult {
            std::vector<Import> imports;
            Scope *globalScope;
        };

        AnalysisResult Analyze(Ast::Root* root);
        std::vector<Error> GetErrors() const { return errors; }

        ~SemanticAnalyzer() override;
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
        void Visit(Ast::AddressOfExpression *addressOfExpression) override;
        void Visit(Ast::CastExpression *castExpression) override;
        void Visit(Ast::AllocateExpression *allocateExpression) override;
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
        void Visit(Type *type) override;
        void Visit(Ast::TypeOfExpression *typeOfExpression) override;
        void Visit(Ast::AlignOfExpression *alignOfExpression) override;
        void Visit(Ast::DereferenceExpression *dereferenceExpression) override;
        void Visit(Ast::FreeStatement *freeStatement) override;
        void Visit(Ast::ContinueStatement *continueStatement) override;

    private:
        std::vector<Error> errors = {};
        std::vector<Import> imports = {};
        Scope *globalScope = new Scope(nullptr);
        Scope *currentScope = globalScope;

        void EnterScope() {
            Scope *newScope = new Scope(currentScope);
            currentScope->children.push_back(newScope);
            currentScope = newScope;
        }

        void ExitScope() {
            if (currentScope->parent) {
                currentScope = currentScope->parent;
            }
        }

        void AddSymbol(const Symbol& symbol) {
            if (!currentScope->Define(symbol)) {
                errors.push_back({"Symbol '" + symbol.name + "' is already defined in this scope", "", symbol.line, symbol.column});
            }
        }

        Symbol *ResolveSymbol(const std::string& name) {
            return currentScope->Resolve(name);
        }
    };
} // Oxy

#endif //OXYLANG_SEMANTICANALYZER_H