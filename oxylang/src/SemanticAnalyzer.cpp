#include "SemanticAnalyzer.h"

#include "ImportType.h"
#include "StructType.h"

namespace Oxy {
    SemanticAnalyzer::AnalysisResult SemanticAnalyzer::Analyze(Ast::Root *root) {

        Visit(root); // calls Visit on all nodes.
        return {imports, globalScope};
    }

    SemanticAnalyzer::~SemanticAnalyzer() {
        // Does nothing.
    }

    void SemanticAnalyzer::Visit(Ast::Root *root) {
        for (const auto& import : root->GetImports()) {
            Visit(import);
        }

        for (const auto& structDecl : root->GetStructs()) {
            Visit(structDecl);
        }

        for (const auto& varDecl : root->GetVariables()) {
            Visit(varDecl);
        }

        // We prescan & add the function signatures to the global scope to allow for recursion and non linear function definitions.
        for (const auto& func : root->GetFunctions()) {
            AddSymbol({func->GetName(), func->GetReturnType(), Symbol::Kind::Function, func->GetLine(), func->GetColumn()});
        }

        for (const auto& func : root->GetFunctions()) {
            Visit(func);
        }
    }

    void SemanticAnalyzer::Visit(Ast::Function *function) {
        EnterScope();

        for (const auto& param : function->GetParameters()) {
            Visit(param);
        }

        for (const auto& stmt : function->GetBody()) {
            stmt->Accept(this);
        }

        ExitScope();
    }

    void SemanticAnalyzer::Visit(Ast::VariableDeclaration *variableDeclaration) {
        auto name = variableDeclaration->GetName();
        auto type = variableDeclaration->GetType();

        auto initializer = variableDeclaration->GetInitializer();
        if (initializer) {
            initializer->Accept(this);
        }

        if (type == nullptr && initializer == nullptr) {
            errors.push_back({"Variable '" + name + "' has no type and no initializer, so its type cannot be inferred", "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
            return;
        }

        // Infer the type if it's not explicitly specified.
        if (type == nullptr) {
            type = ResolveExpressionType(initializer);
            if (!type) {
                errors.push_back({"Could not infer type of variable '" + name + "' from initializer", "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                return;
            }
        }

        AddSymbol({name, type, Symbol::Kind::Variable, variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
    }

    void SemanticAnalyzer::Visit(Ast::StructDeclaration *structDeclaration) {
        std::vector<Ast::StructType::Field> fields;
        for (const auto& field : structDeclaration->GetFields()) {
            fields.push_back({field->GetName(), field->GetType()});
        }

        AddSymbol({structDeclaration->GetName(), new Ast::StructType(structDeclaration->GetName(), fields), Symbol::Kind::Struct, structDeclaration->GetLine(), structDeclaration->GetColumn()});
    }

    void SemanticAnalyzer::Visit(Ast::Attribute *attribute) {
    }

    void SemanticAnalyzer::Visit(Ast::LiteralExpression *literalExpression) {
    }

    void SemanticAnalyzer::Visit(Ast::AssignmentExpression *assignmentExpression) {
        // Check if the left hand side is an identifier.
        auto *identifierExpression = dynamic_cast<Ast::IdentifierExpression *>(assignmentExpression->GetLeft());
        if (identifierExpression) {
            if (!ResolveSymbol(identifierExpression->ToString())) {
                errors.push_back({"Undefined variable: " + identifierExpression->ToString(), "", identifierExpression->GetLine(), identifierExpression->GetColumn()});
            }
        }

        auto *subscriptExpression = dynamic_cast<Ast::SubscriptExpression *>(assignmentExpression->GetLeft());
        if (subscriptExpression) {
            subscriptExpression->Accept(this);
        }

        auto *memberAccessExpression = dynamic_cast<Ast::MemberAccessExpression *>(assignmentExpression->GetLeft());
        if (memberAccessExpression) {
            memberAccessExpression->Accept(this);
        }

        auto *dereferenceExpression = dynamic_cast<Ast::UnaryExpression *>(assignmentExpression->GetLeft());
        if (dereferenceExpression && dereferenceExpression->GetOperand()) {
            dereferenceExpression->Accept(this);
        }
    }

    void SemanticAnalyzer::Visit(Ast::ReturnStatement *returnStatement) {
        auto expression = returnStatement->GetValue();
        if (expression) {
            expression->Accept(this);
        }
    }

    void SemanticAnalyzer::Visit(Ast::BinaryExpression *binaryExpression) {
        auto left = binaryExpression->GetLeft();
        auto right = binaryExpression->GetRight();
        left->Accept(this);
        right->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::SizeOfExpression *sizeOfExpression) {
        auto type = sizeOfExpression->GetType();
        if (!type) {
            errors.push_back({"Unknown type in sizeof expression", "", sizeOfExpression->GetLine(), sizeOfExpression->GetColumn()});
        }
    }

    void SemanticAnalyzer::Visit(Ast::AddressOfExpression *addressOfExpression) {
        auto operand = addressOfExpression->GetOperand();
        operand->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::CastExpression *castExpression) {
        auto operand = castExpression->GetOperand();
        auto targetType = castExpression->GetTargetType();

        if (!targetType) {
            errors.push_back({"Unknown target type in cast expression", "", castExpression->GetLine(), castExpression->GetColumn()});
            return;
        }

        operand->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::AllocateExpression *allocateExpression) {
        auto type = allocateExpression->GetType();
        if (!type) {
            errors.push_back({"Unknown type in allocate expression", "", allocateExpression->GetLine(), allocateExpression->GetColumn()});
            return;
        }

        auto count = allocateExpression->GetCount();
        if (count) {
            count->Accept(this);
        }
    }

    void SemanticAnalyzer::Visit(Ast::UnaryExpression *unaryExpression) {
        auto operand = unaryExpression->GetOperand();
        operand->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::FunctionCallExpression *functionCallExpression) {
        auto callee = functionCallExpression->GetCallee();
        callee->Accept(this);

        for (const auto& arg : functionCallExpression->GetArguments()) {
            arg->Accept(this);
        }
    }

    void SemanticAnalyzer::Visit(Ast::PostfixExpression *postfixExpression) {
        auto operand = postfixExpression->GetOperand();
        operand->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::SubscriptExpression *subscriptExpression) {
        auto array = subscriptExpression->GetArray();
        auto index = subscriptExpression->GetIndex();

        array->Accept(this);
        index->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::MemberAccessExpression *memberAccessExpression) {
        auto object = memberAccessExpression->GetObject();
        object->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::IdentifierExpression *identifierExpression) {
        if (!ResolveSymbol(identifierExpression->ToString())) {
            errors.push_back({"Undefined variable: " + identifierExpression->ToString(), "", identifierExpression->GetLine(), identifierExpression->GetColumn()});
        }
    }

    void SemanticAnalyzer::Visit(Ast::NestedExpression *nestedExpression) {
        nestedExpression->GetInner()->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::BreakStatement *breakStatement) {
    }

    void SemanticAnalyzer::Visit(Ast::IfStatement *ifStatement) {

        auto mainBranch = ifStatement->GetMainBranch();
        EnterScope();
        mainBranch.condition->Accept(this);
        for (const auto& stmt : mainBranch.body) {
            stmt->Accept(this);
        }
        ExitScope();


        for (const auto& elseIf : ifStatement->GetElseIfBranches()) {
            EnterScope();
            elseIf.condition->Accept(this);
            for (const auto& stmt : elseIf.body) {
                stmt->Accept(this);
            }
            ExitScope();
        }

        if (!ifStatement->GetElseBranch().empty()) {
            EnterScope();
            for (const auto& stmt : ifStatement->GetElseBranch()) {
                stmt->Accept(this);
            }
            ExitScope();
        }
    }

    void SemanticAnalyzer::Visit(Ast::ExpressionStatement *expressionStatement) {
        expressionStatement->GetExpression()->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::WhileStatement *whileStatement) {
        EnterScope();
        whileStatement->GetCondition()->Accept(this);
        for (const auto& stmt : whileStatement->GetBody()) {
            stmt->Accept(this);
        }
        ExitScope();
    }

    void SemanticAnalyzer::Visit(Ast::ForStatement *forStatement) {
        EnterScope();
        forStatement->GetInitializer()->Accept(this); // let i = 0;
        forStatement->GetCondition()->Accept(this); // i < 10;
        forStatement->GetIncrement()->Accept(this); // i++
        for (const auto& stmt : forStatement->GetBody()) {
            stmt->Accept(this);
        }
        ExitScope();
    }

    void SemanticAnalyzer::Visit(Ast::ImportStatement *importStatement) {
        imports.push_back({importStatement->GetModuleName(), importStatement->GetAlias()});
        AddSymbol({importStatement->GetAlias(), new ImportType(importStatement->GetModuleName()), Symbol::Kind::Import, importStatement->GetLine(), importStatement->GetColumn()});
    }

    void SemanticAnalyzer::Visit(Type *type) {
        if (type->GetLiteralType() == LiteralType::UserDefined) {
            if (!ResolveSymbol(type->GetIdentifier())) {
                errors.push_back({"Undefined type: " + type->GetIdentifier(), "", 0, 0});
            }
        }
    }

    void SemanticAnalyzer::Visit(Ast::TypeOfExpression *typeOfExpression) {
        auto expr = typeOfExpression->GetExpression();
        expr->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::AlignOfExpression *alignOfExpression) {
        auto type = alignOfExpression->GetType();
        if (!type) {
            errors.push_back({"Unknown type in alignof expression", "", alignOfExpression->GetLine(), alignOfExpression->GetColumn()});
        }
    }

    void SemanticAnalyzer::Visit(Ast::DereferenceExpression *dereferenceExpression) {
        auto operand = dereferenceExpression->GetOperand();
        operand->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::FreeStatement *freeStatement) {
        auto pointer = freeStatement->GetPointer();
        pointer->Accept(this);
    }

    void SemanticAnalyzer::Visit(Ast::ContinueStatement *continueStatement) {
    }

    Type * SemanticAnalyzer::ResolveExpressionType(Ast::Expression *expression) {
        if (!expression) return nullptr;

        if (auto* lit = dynamic_cast<Ast::LiteralExpression*>(expression))
            return new Type(lit->GetLiteralType());

        if (auto* ident = dynamic_cast<Ast::IdentifierExpression*>(expression)) {
            auto* sym = ResolveSymbol(ident->ToString());
            return sym ? sym->type : nullptr;
        }

        if (auto* bin = dynamic_cast<Ast::BinaryExpression*>(expression)) {
            Type* t = ResolveExpressionType(bin->GetLeft());
            return t ? t : ResolveExpressionType(bin->GetRight());
        }

        if (auto* unary = dynamic_cast<Ast::UnaryExpression*>(expression))
            return ResolveExpressionType(unary->GetOperand());

        if (auto* nested = dynamic_cast<Ast::NestedExpression*>(expression))
            return ResolveExpressionType(nested->GetInner());

        if (auto* cast = dynamic_cast<Ast::CastExpression*>(expression))
            return cast->GetTargetType();

        if (auto* call = dynamic_cast<Ast::FunctionCallExpression*>(expression)) {
            auto* callee = dynamic_cast<Ast::IdentifierExpression*>(call->GetCallee());
            if (callee) {
                auto* sym = ResolveSymbol(callee->ToString());
                return sym ? sym->type : nullptr;
            }
        }

        if (auto* postfix = dynamic_cast<Ast::PostfixExpression*>(expression))
            return ResolveExpressionType(postfix->GetOperand());

        if (auto* subscript = dynamic_cast<Ast::SubscriptExpression*>(expression)) {
            return ResolveExpressionType(subscript->GetArray());
        }

        return nullptr;
    }
} // Oxy