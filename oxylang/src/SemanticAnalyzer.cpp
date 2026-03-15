#include "SemanticAnalyzer.h"

#include <algorithm>

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
            AddSymbol({func->GetName(), func->GetFunctionType(), Symbol::Kind::Function, func->GetLine(), func->GetColumn()});
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
        assignmentExpression->GetRight()->Accept(this);

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
            auto idx = subscriptExpression->GetIndex();

            if (auto *idxLiteral = dynamic_cast<Ast::LiteralExpression *>(idx)) {
                auto type = idxLiteral->GetLiteralType();
                if (type != LiteralType::Int &&
                    type != LiteralType::U8 &&
                    type != LiteralType::U16 &&
                    type != LiteralType::U32 &&
                    type != LiteralType::U64 &&
                    type != LiteralType::I8 &&
                    type != LiteralType::I16 &&
                    type != LiteralType::I32 &&
                    type != LiteralType::I64) {
                    errors.push_back({"Array index must be of type int", "", idx->GetLine(), idx->GetColumn()});
                }

                auto value = std::get<uint64_t>(idxLiteral->GetValue());

                auto arrayType = ResolveExpressionType(subscriptExpression->GetArray());
                if (value >= (uint64_t)arrayType->GetCount()) {
                    errors.push_back({"Array index is out of bounds (" + std::to_string(value) + " >= " + std::to_string(arrayType->GetCount()) + ")", "", idx->GetLine(), idx->GetColumn()});
                }
            }
            else if (auto *idxUnary = dynamic_cast<Ast::UnaryExpression *>(idx)) {
                if (idxUnary->GetOperator() == Operator::Subtract) {
                    if (auto *idxUnaryLiteral = dynamic_cast<Ast::LiteralExpression *>(idxUnary->GetOperand())) {
                        errors.push_back({"Array index cannot be negative", "", idx->GetLine(), idx->GetColumn()});
                    }
                }
            }
            else {
                auto idxType = ResolveExpressionType(idx);
                if (idxType) {
                    if (idxType->GetLiteralType() != LiteralType::Int &&
                        idxType->GetLiteralType() != LiteralType::U8 &&
                        idxType->GetLiteralType() != LiteralType::U16 &&
                        idxType->GetLiteralType() != LiteralType::U32 &&
                        idxType->GetLiteralType() != LiteralType::U64 &&
                        idxType->GetLiteralType() != LiteralType::I8 &&
                        idxType->GetLiteralType() != LiteralType::I16 &&
                        idxType->GetLiteralType() != LiteralType::I32 &&
                        idxType->GetLiteralType() != LiteralType::I64) {
                        errors.push_back({"Array index must be of type int, but got " + idxType->ToString(), "", idx->GetLine(), idx->GetColumn()});
                    }
                }
            }
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
        auto expectedReturnType = currentFunction ? currentFunction->GetFunctionType()->GetReturnType() : nullptr;

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
        if (binaryExpression->GetOperator() == Operator::Assignment) {
            errors.push_back({"Assignment operator should have been handled as an AssignmentExpression, not a BinaryExpression", "", binaryExpression->GetLine(), binaryExpression->GetColumn()});
            return;
        }

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

    void SemanticAnalyzer::Visit(Ast::StructInitializerExpression *structInitializerExpression) {
        auto identifier = structInitializerExpression->GetStructIdentifier();
        auto type = identifier->ToString();

        auto sym = ResolveSymbol(type);
        if (sym) {
            if (sym->kind != Symbol::Kind::Struct) {
                errors.push_back({"Type '" + type + "' is not a struct", "", structInitializerExpression->GetLine(), structInitializerExpression->GetColumn()});
            }
        }
        else {
            errors.push_back({"Undefined struct: " + type, "", structInitializerExpression->GetLine(), structInitializerExpression->GetColumn()});
        }

        auto structType = dynamic_cast<Ast::StructType*>(sym->type);
        if (!structType) {
            errors.push_back({"Symbol '" + type + "' is not a struct type", "", structInitializerExpression->GetLine(), structInitializerExpression->GetColumn()});
            return;
        }

        for (const auto& field : structInitializerExpression->GetInitializers()) {
            // Verify the field exists in the struct definition.
            auto it = std::find_if(structType->GetFields().begin(), structType->GetFields().end(), [&](const Ast::StructType::Field& f) {
                return f.name == field.first;
            });

            if (it == structType->GetFields().end()) {
                errors.push_back({"Struct '" + type + "' has no field named '" + field.first + "'", "", field.second->GetLine(), field.second->GetColumn()});
                continue;
            }

            field.second->Accept(this);
        }
    }

    void SemanticAnalyzer::Visit(Ast::TypeExpression *typeExpression) {
    }

    Type *SemanticAnalyzer::ResolveExpressionType(Ast::Expression *expression) {
        if (!expression) return nullptr;

        if (auto* lit = dynamic_cast<Ast::LiteralExpression*>(expression)) {
            if (lit->GetLiteralType() == LiteralType::Pointer && std::holds_alternative<uint64_t>(lit->GetValue()) && std::get<uint64_t>(lit->GetValue()) == 0) {
                return new Type(LiteralType::Pointer, 0, new Type(LiteralType::Void));
            }
            else if (lit->GetLiteralType() == LiteralType::Pointer && std::holds_alternative<std::string>(lit->GetValue())) {
                return new Type(LiteralType::Pointer, 0, new Type(LiteralType::U8)); // string literals have type "pointer to u8"
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
                auto *funcType = dynamic_cast<FunctionType*>(sym ? sym->type : nullptr);
                return funcType ? funcType->GetReturnType() : nullptr;
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

        if (auto* memberAccessExpression = dynamic_cast<Ast::MemberAccessExpression*>(expression)) {
            auto* objectType = ResolveExpressionType(memberAccessExpression->GetObject());
            if (objectType && objectType->GetLiteralType() == LiteralType::UserDefined) {
                auto* sym = ResolveSymbol(objectType->GetIdentifier());
                if (sym && sym->kind == Symbol::Kind::Struct) {
                    auto* structType = dynamic_cast<Ast::StructType*>(sym->type);
                    if (structType) {
                        auto fieldName = memberAccessExpression->GetMemberName();
                        auto it = std::find_if(structType->GetFields().begin(), structType->GetFields().end(), [&](const Ast::StructType::Field& f) {
                            return f.name == fieldName;
                        });

                        if (it != structType->GetFields().end()) {
                            return it->type;
                        }
                    }
                }
            }
        }

        if (auto* structInit = dynamic_cast<Ast::StructInitializerExpression*>(expression)) {
            auto identifier = structInit->GetStructIdentifier();
            auto sym = ResolveSymbol(identifier->ToString());
            if (sym && sym->kind == Symbol::Kind::Struct) {
                return sym->type;
            }
        }

        return nullptr;
    }

    bool SemanticAnalyzer::TypesMatch(Type *a, Type *b) {
        if (a == nullptr || b == nullptr) {
            return false;
        }

        if (*a != *b) {
            return false;
        }

        return true;
    }
} // Oxy