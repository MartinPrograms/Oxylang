#ifndef OXYLANG_CODEGENERATOR_H
#define OXYLANG_CODEGENERATOR_H

#include "Ast/Nodes.h"
#include "Error.h"
#include "Definitions.h"
#include <Qbe.h>

#include <utility>
#include "SemanticAnalyzer.h"
#include "StructType.h"

namespace Oxy {
    class CodeGenerator : public Visitor {
    public:
        // CodeGenerator variant of scope
        class Scope {
        public:
            Scope* parent = nullptr;
            std::vector<Scope *> children {};
            std::unordered_map<std::string, Qbe::ValueReference> variables {};
            std::unordered_map<std::string, SemanticAnalyzer::Symbol> symbols {};

            Scope(Scope* parent) : parent(parent) {}
        };

        CodeGenerator(SemanticAnalyzer::AnalysisResult result, bool is64BitTarget, const std::map<std::string, ModuleData>& fileIdMap);
        ~CodeGenerator() override;

        std::string Generate(Ast::Root *ast);

        [[nodiscard]] const std::vector<Error>& GetErrors() const { return errors; }

        void SetupStandardLibrary();

        void Visit(Ast::Root *root) override;
        void Visit(Ast::Function *function) override;

        void RegisterFunctionType(const std::string & name, Type * explicit_type);

        void EmitGlobal(Ast::VariableDeclaration *variableDeclaration, Type *explicitType,
                        Ast::Expression *initializer);

        void addMemcpy(const Qbe::ValueReference & value, const Qbe::ValueReference & allocated, long byte_size);

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

        Qbe::ValueReference ResolveFunction(const std::string &string, std::size_t size);

        std::string ResolveFunctionName(Ast::Expression * expression);

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
        void Visit(Type *type) override;
        void Visit(Ast::DereferenceAssignmentStatement *dereferenceAssignmentStatement) override;
        void Visit(Ast::StructInitializerExpression *structInitializerExpression) override;
        void Visit(Ast::PointerMemberAccessExpression *pointerMemberAccessExpression) override;
        void Visit(Ast::TypeExpression *typeExpression) override;

    private:
        SemanticAnalyzer::AnalysisResult result;
        std::vector<Error> errors;
        Qbe::Module module;
        std::unordered_map<std::string, Qbe::Function *> functions{};
        Scope *currentScope = new Scope(nullptr);
        Qbe::Function* currentFunction;
        std::unordered_map<std::string, Qbe::CustomType *> customTypes{};
        std::map<std::string, ModuleData> fileIdMap;

        Qbe::ITypeDefinition *GetQbeType(Type* type);

        Type *ResolveExpressionType(Ast::Expression* expression);

        std::string escapeStringLiteral(const std::string & get);

        size_t GetStructMemberOffset(Ast::Expression * expression, const std::string & string);
        Qbe::ValueReference GetStructAddress(Ast::Expression * expression);

        Qbe::ValueReference EmitExpression(Ast::Expression* expression, bool getReference = false);

        std::stack<Qbe::Block *> blockStack {};
        Qbe::Block *getCurrentBlock() {
            if (blockStack.empty()) {
                throw std::runtime_error("No current block available");
            }
            return blockStack.top();
        }

        std::stack<Qbe::Block *> breakBlockStack {};
        std::stack<Qbe::Block *> continueBlockStack {};

        bool isGlobalScope = true;

        void EnterScope() {
            Scope* newScope = new Scope(currentScope);
            if (currentScope) {
                currentScope->children.push_back(newScope);
            }
            currentScope = newScope;
        }

        void ExitScope() {
            if (currentScope && currentScope->parent) {
                currentScope = currentScope->parent;
            }
        }

        Qbe::ValueReference *ResolveVariable(const std::string& name) {
            Scope* scope = currentScope;
            while (scope) {
                auto it = scope->variables.find(name);
                if (it != scope->variables.end()) {
                    return &it->second;
                }
                scope = scope->parent;
            }
            return nullptr;
        }

        SemanticAnalyzer::Symbol *ResolveSymbol(const std::string& name) {
            Scope* scope = currentScope;
            while (scope) {
                auto it = scope->symbols.find(name);
                if (it != scope->symbols.end()) {
                    return &it->second;
                }
                scope = scope->parent;
            }

            // Check exported functions from imports
            for (const auto& import : result.imports) {
                auto moduleIt = fileIdMap.find(import.moduleName);
                if (moduleIt != fileIdMap.end()) {
                    const auto& moduleData = moduleIt->second;
                    for (const auto& export_ : moduleData.exports) {
                        if (export_.symbolName == name) {
                            if (export_.type == ExportType::Function) {
                                return new SemanticAnalyzer::Symbol{name, nullptr, SemanticAnalyzer::Symbol::Kind::Function, 0, 0};
                            }
                            else if (export_.type == ExportType::Struct) {
                                return new SemanticAnalyzer::Symbol{name, nullptr, SemanticAnalyzer::Symbol::Kind::Struct, 0, 0};
                            }
                            else if (export_.type == ExportType::Variable) {
                                return new SemanticAnalyzer::Symbol{name, nullptr, SemanticAnalyzer::Symbol::Kind::Variable, 0, 0};
                            }
                        }
                    }
                }
            }

            return nullptr;
        }

        bool AddVariable(const std::string& name, const Qbe::ValueReference& value, const SemanticAnalyzer::Symbol& symbol) {
            if (currentScope->variables.find(name) != currentScope->variables.end()) {
                return false; // Variable already exists in the current scope
            }
            currentScope->variables[name] = value;
            currentScope->symbols[name] = symbol;
            return true;
        }

        Qbe::ValueReference GetSize(Qbe::ITypeDefinition* type) {
            if (type == nullptr) {
                throw std::runtime_error("Type cannot be null when getting size");
            }
            auto size = type->ByteSize(module.is64Bit);
            if (!size) {
                throw std::runtime_error("Unsupported type for sizeof");
            }
            return Qbe::CreateLiteral((int64_t)size);
        }

        std::string GetLabel(Ast::Node *node) {
            static uint64_t labelCounter = 0;
            return fmt::format("label_{}_{}{}{}", node->GetLine(), node->GetColumn(), labelCounter, labelCounter++);
        }

        std::string GetLabel(std::string prefix) {
            static uint64_t labelCounter = 0;
            return fmt::format("{}_{}", prefix, labelCounter++);
        }

        // Mangles a type name
        std::string MangleName(Ast::StructType *structType) {
            auto mangled = "struct_" + structType->GetName();
            for (const auto& field : structType->GetFields()) {
                mangled += "_" + field.name;
            }
            return mangled;
        }
    };
} // Oxy

#endif //OXYLANG_CODEGENERATOR_H