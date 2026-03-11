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
        currentFunction = function;

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

        auto leftType = variableDeclaration->GetType();
        auto rightType = initializer ? ResolveExpressionType(initializer) : nullptr;

        if (!leftType && rightType) {
            return;
        }

        if (leftType && rightType) {
            if (!TypesMatch(leftType, rightType)) {
                errors.push_back({"Type mismatch in variable declaration: declared type is " + leftType->ToString() + " but initializer is of type " + rightType->ToString(), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
            }
        }
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

        auto leftType = ResolveExpressionType(assignmentExpression->GetLeft());
        auto rightType = ResolveExpressionType(assignmentExpression->GetRight());

        // we can allow for implicit left type, so if left type is not defined just set the type to the right type, but if the left type is defined and the right type is not defined, then it's an error.
        if (!leftType && rightType) {
            // This is an implicit variable declaration, so we add it to the current scope.
            auto identifier = dynamic_cast<Ast::IdentifierExpression *>(assignmentExpression->GetLeft());
            if (identifier) {
                AddSymbol({identifier->ToString(), rightType, Symbol::Kind::Variable, identifier->GetLine(), identifier->GetColumn()});
            } else {
                errors.push_back({"Left hand side of assignment must be an identifier for implicit variable declaration", "", assignmentExpression->GetLine(), assignmentExpression->GetColumn()});
            }
        } else if (leftType && rightType) {
            if (!TypesMatch(leftType, rightType)) {
                errors.push_back({"Type mismatch in assignment: left is " + leftType->ToString() + " but right is " + rightType->ToString(), "", assignmentExpression->GetLine(), assignmentExpression->GetColumn()});
            }
        } else if (leftType && !rightType) {
            errors.push_back({"Could not resolve type of right hand side of assignment", "", assignmentExpression->GetLine(), assignmentExpression->GetColumn()});
        } else {
            errors.push_back({"Could not resolve type of left hand side and right hand side of assignment", "", assignmentExpression->GetLine(), assignmentExpression->GetColumn()});
        }
    }

    void SemanticAnalyzer::Visit(Ast::ReturnStatement *returnStatement) {
        auto expression = returnStatement->GetValue();
        if (expression) {
            expression->Accept(this);
        }

        auto expressionType = expression ? ResolveExpressionType(expression) : nullptr;
        auto expectedReturnType = ResolveSymbol(currentFunction->GetName())->type;

        if (!expectedReturnType) {
            errors.push_back({"Could not resolve return type of current function", "", returnStatement->GetLine(), returnStatement->GetColumn()});
            return;
        }

        if (!expressionType) {
            if (expectedReturnType->ToString() != "void") {
                errors.push_back({"Return statement has no value but function return type is " + expectedReturnType->ToString(), "", returnStatement->GetLine(), returnStatement->GetColumn()});
            }
            return;
        }

        if (!TypesMatch(expressionType, expectedReturnType)) {
            errors.push_back({"Type mismatch in return statement: expected " + expectedReturnType->ToString() + " but got " + expressionType->ToString(), "", returnStatement->GetLine(), returnStatement->GetColumn()});
        }
    }

    void SemanticAnalyzer::Visit(Ast::BinaryExpression *binaryExpression) {
        auto left = binaryExpression->GetLeft();
        auto right = binaryExpression->GetRight();
        left->Accept(this);
        right->Accept(this);

        auto typeLeft = ResolveExpressionType(left);
        auto typeRight = ResolveExpressionType(right);

        if (!typeLeft || !typeRight) {
            errors.push_back({"Could not resolve type of binary expression", "", binaryExpression->GetLine(), binaryExpression->GetColumn()});
            return;
        }

        if (TypesMatch(typeLeft, typeRight)) {
            return;
        }

        errors.push_back({"Type mismatch in binary expression: left is " + typeLeft->ToString() + " but right is " + typeRight->ToString(), "", binaryExpression->GetLine(), binaryExpression->GetColumn()});
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

    void SemanticAnalyzer::Visit(Ast::DereferenceAssignmentStatement *dereferenceAssignmentStatement) {

    }

    Type *SemanticAnalyzer::ResolveExpressionType(Ast::Expression *expression) {
        if (!expression) return nullptr;

        if (auto* lit = dynamic_cast<Ast::LiteralExpression*>(expression)) {
            if (lit->GetLiteralType() == LiteralType::Pointer && std::holds_alternative<uint64_t>(lit->GetValue()) && std::get<uint64_t>(lit->GetValue()) == 0) {
                return new Type(LiteralType::Pointer, 0, new Type(LiteralType::Void));
            }
            return new Type(lit->GetLiteralType());
        }

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
            auto type = ResolveExpressionType(subscript->GetArray());
            if (type && type->GetLiteralType() == LiteralType::Pointer) {
                return type->GetNestedType();
            }
        }

        if (auto* addressOf = dynamic_cast<Ast::AddressOfExpression*>(expression)) {
            auto* operandType = ResolveExpressionType(addressOf->GetOperand());
            if (operandType) {
                return new Type(LiteralType::Pointer, 0, operandType);
            }
        }

        if (auto* dereference = dynamic_cast<Ast::DereferenceExpression*>(expression)) {
            auto* operandType = ResolveExpressionType(dereference->GetOperand());
            if (operandType && operandType->GetLiteralType() == LiteralType::Pointer) {
                return operandType->GetNestedType();
            }
        }

        return nullptr;
    }

    bool SemanticAnalyzer::TypesMatch(Type *a, Type *b) {
        if (a->GetLiteralType() != b->GetLiteralType()) {
            return false;
        }

        if (a->GetLiteralType() == LiteralType::Pointer && b->GetLiteralType() == LiteralType::Pointer) {
            auto* aPointed = a->GetNestedType();
            auto* bPointed = b->GetNestedType();
            if (!aPointed || !bPointed) {
                return false; // If either pointed type is null, we can't say they match.
            }
            return TypesMatch(aPointed, bPointed);
        }

        if (a->GetLiteralType() == LiteralType::UserDefined) {
            return a->GetIdentifier() == b->GetIdentifier();
        }

        return true;
    }
} // Oxy