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

    void CodeGenerator::SetupStandardLibrary() {
        auto memcpy = module.defineFunction("$memcpy",
            new Qbe::VoidType(),
            {
                Qbe::Local("dest", new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer, false), true),
                Qbe::Local("src", new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer, false), true),
                Qbe::Local("n", new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer, false), true)
            },
        false);
        functions["memcpy"] = memcpy;

        auto memset = module.defineFunction("$memset",
            new Qbe::VoidType(),
            {
                Qbe::Local("dest", new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer, false), true),
                Qbe::Local("value", new Qbe::Primitive(Qbe::TypeDefinitionKind::Int32, false), true),
                Qbe::Local("n", new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer, false), true)
            },
        false);
        functions["memset"] = memset;
    }

    void CodeGenerator::Visit(Ast::Root *root) {
        for (const auto& import : root->GetImports()) {
            Visit(import);
        }

        for (const auto& structDecl : root->GetStructs()) {
            Visit(structDecl);
        }

        for (const auto& varDecl : root->GetVariables()) {
            Visit(varDecl);
        }

        SetupStandardLibrary();

        for (const auto& func : root->GetFunctions()) {
            std::vector<Qbe::Local> args;
            for (const auto& param : func->GetParameters()) {
                if (param->GetType()->GetLiteralType() == LiteralType::UserDefined) {
                    args.push_back(Qbe::Local(param->GetName(), new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer, false), true));
                }
                else {
                    args.push_back(Qbe::Local(param->GetName(), GetQbeType(param->GetType()), true));
                }
            }

            bool isVariadic = func->IsVariadic();

            Qbe::FunctionFlags flags = Qbe::FunctionFlags::None;
            std::string symbol = func->GetName();
            bool isExtern = false;
            if (func->GetAttributes().size() > 0) {
                for (const auto& attr : func->GetAttributes()) {
                    if (attr->GetName() == "export") {
                        flags = Qbe::FunctionFlags::Export;
                    } else if (attr->GetName() == "extern") {
                        isExtern = true;
                    } else if (attr->GetName() == "entry") {
                        flags = Qbe::FunctionFlags::EntryPoint;
                    } else if (attr->GetName() == "symbol") {
                        if (attr->GetArgs().size() != 1) {
                            errors.push_back({"'symbol' attribute requires exactly one argument", "", attr->GetLine(), attr->GetColumn()});
                        }

                        auto arg = attr->GetArgs()[0];
                        if (auto literalArg = dynamic_cast<Ast::LiteralExpression *>(arg)) {
                            if (literalArg->GetLiteralType() == LiteralType::Pointer) {
                                auto value = literalArg->GetValue();
                                if (std::holds_alternative<std::string>(value)) {
                                    symbol = std::get<std::string>(value);
                                } else {
                                    errors.push_back({"'symbol' attribute argument must be a string literal", "", arg->GetLine(), arg->GetColumn()});
                                }
                            } else {
                                errors.push_back({"'symbol' attribute argument must be a string literal", "", arg->GetLine(), arg->GetColumn()});
                            }
                        } else {
                            errors.push_back({"'symbol' attribute argument must be a string literal", "", arg->GetLine(), arg->GetColumn()});
                        }
                    }
                    else {
                        errors.push_back({"Unknown attribute '" + attr->GetName() + "' on function '" + func->GetName() + "'", "", attr->GetLine(), attr->GetColumn()});
                    }
                }
            }

            if (isExtern) {
                auto function = module.defineFunction(symbol, GetQbeType(func->GetReturnType()), args, isVariadic);
                functions[func->GetName()] = function;
            }
            else {
                auto function = module.addFunction(symbol, GetQbeType(func->GetReturnType()), args, isVariadic, flags);
                functions[func->GetName()] = function;
            }

            currentScope->symbols[func->GetName()] = {func->GetName(), func->GetFunctionType(), SemanticAnalyzer::Symbol::Kind::Function, func->GetLine(), func->GetColumn()};
        }

        for (const auto& func : root->GetFunctions()) {
            Visit(func);
        }
    }

    void CodeGenerator::Visit(Ast::Function *function) {
        auto qbeFunction = functions[function->GetName()];
        auto entryPoint = qbeFunction->entryPoint();
        blockStack.push(entryPoint);
        currentFunction = qbeFunction;
        isGlobalScope = false;

        EnterScope();
        for (size_t i = 0; i < function->GetParameters().size(); i++) {
            auto param = function->GetParameters()[i];
            if (param->GetType()->GetLiteralType() == LiteralType::UserDefined) {
                auto qbeType = GetQbeType(param->GetType());
                auto allocated = entryPoint->addAllocate(GetSize(qbeType));
                addMemcpy(Qbe::ValueReference(&qbeFunction->parameters[i]), allocated, (long)qbeType->ByteSize(module.is64Bit));
                AddVariable(param->GetName(), allocated, {param->GetName(), param->GetType(), SemanticAnalyzer::Symbol::Kind::Variable, param->GetLine(), param->GetColumn()});
            }
            else {
                auto allocated = entryPoint->addAllocate(GetSize(GetQbeType(param->GetType())));
                entryPoint->addStore(Qbe::ValueReference(&qbeFunction->parameters[i]), allocated);
                AddVariable(param->GetName(), allocated, {param->GetName(), param->GetType(), SemanticAnalyzer::Symbol::Kind::Variable, param->GetLine(), param->GetColumn()});
            }
        }

        getCurrentBlock()->addComment("Begin function '" + function->GetName() + "' body");
        for (const auto& stmt : function->GetBody()) {
            stmt->Accept(this);
        }

        if (function->GetReturnType()->GetLiteralType() == LiteralType::Void) {
            if (getCurrentBlock()->isTerminated() == false) {
                getCurrentBlock()->addReturn();
            }
        }
        getCurrentBlock()->addComment("End function '" + function->GetName() + "' body");

        ExitScope();
        isGlobalScope = true;
    }

    void CodeGenerator::RegisterFunctionType(const std::string &get_name, Type *explicit_type) {
        auto funcType = dynamic_cast<FunctionType *>(explicit_type);
        if (!funcType) {
            errors.push_back({"Type of function '" + get_name + "' is not a function type", "", 0, 0});
            return;
        }

        currentScope->symbols[get_name] = {get_name, explicit_type, SemanticAnalyzer::Symbol::Kind::Function, 0, 0};
    }

    void CodeGenerator::EmitGlobal(Ast::VariableDeclaration *variableDeclaration, Type *explicitType, Ast::Expression *initializer) {
        auto qbeType = GetQbeType(explicitType);

        std::string value = "0";
        if (initializer) {
            if (auto literal = dynamic_cast<Ast::LiteralExpression *>(initializer)) {
                if (literal->GetLiteralType() == LiteralType::Pointer && std::holds_alternative<std::string>(
                        literal->GetValue())) {
                    std::string str = std::get<std::string>(literal->GetValue());
                    AddVariable(variableDeclaration->GetName(), module.addGlobal(escapeStringLiteral(str)), {variableDeclaration->GetName(), explicitType, SemanticAnalyzer::Symbol::Kind::Variable, variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                    return;
                } else {
                    // If its an integer, we can just set the value to that integer
                    if (std::holds_alternative<uint64_t>(literal->GetValue())) {
                        value = std::to_string(std::get<uint64_t>(literal->GetValue()));
                    } else if (std::holds_alternative<double>(literal->GetValue())) {
                        value = std::to_string(std::get<double>(literal->GetValue()));
                    } else {
                        errors.push_back({
                            "Unsupported literal type for global variable initializer", "", initializer->GetLine(),
                            initializer->GetColumn()
                        });
                        return;
                    }
                }
            } else if (auto unary = dynamic_cast<Ast::UnaryExpression *>(initializer)) {
                if (unary->GetOperator() == Operator::Subtract) {
                    if (auto unaryLiteral = dynamic_cast<Ast::LiteralExpression *>(unary->GetOperand())) {
                        if (std::holds_alternative<uint64_t>(unaryLiteral->GetValue())) {
                            value = "-" + std::to_string(std::get<uint64_t>(unaryLiteral->GetValue()));
                        } else if (std::holds_alternative<double>(unaryLiteral->GetValue())) {
                            value = "-" + std::to_string(std::get<double>(unaryLiteral->GetValue()));
                        } else {
                            errors.push_back({
                                "Unsupported literal type for global variable initializer", "",
                                initializer->GetLine(), initializer->GetColumn()
                            });
                            return;
                        }
                    } else {
                        errors.push_back({
                            "Global variable initializer must be a constant expression", "", initializer->GetLine(),
                            initializer->GetColumn()
                        });
                        return;
                    }
                } else {
                    errors.push_back({
                        "Unsupported unary operator in global variable initializer", "", initializer->GetLine(),
                        initializer->GetColumn()
                    });
                    return;
                }
            } else {
                errors.push_back({
                    "Global variable initializer must be a constant expression", "", initializer->GetLine(),
                    initializer->GetColumn()
                });
                return;
            }
        }

        auto primitive = dynamic_cast<Qbe::Primitive*>(qbeType);
        if (!primitive) {
            errors.push_back({"Global variables of type '" + explicitType->ToString() + "' are not supported", "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
            return;
        }

        auto global = module.addGlobal(primitive, value);
        AddVariable(variableDeclaration->GetName(), global, {variableDeclaration->GetName(), explicitType, SemanticAnalyzer::Symbol::Kind::Variable, variableDeclaration->GetLine(), variableDeclaration->GetColumn()});

        if (explicitType->GetLiteralType() == LiteralType::Function) {
            // We need to register this as a function too
            RegisterFunctionType(variableDeclaration->GetName(), explicitType);
        }
    }

    void CodeGenerator::addMemcpy(const Qbe::ValueReference &value, const Qbe::ValueReference &allocated,
        long byte_size) {
        auto memcpyFunc = functions["memcpy"];
        getCurrentBlock()->addCall(memcpyFunc, {allocated, value, Qbe::CreateLiteral((int64_t)byte_size)});
    }

    void CodeGenerator::Visit(Ast::VariableDeclaration *variableDeclaration) {
        auto explicitType = variableDeclaration->GetType();
        auto initializer = variableDeclaration->GetInitializer();

        if (explicitType == nullptr) {
            explicitType = ResolveExpressionType(initializer);
            if (!explicitType) {
                errors.push_back({"Could not infer type of variable '" + variableDeclaration->GetName() + "' from initializer", "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                return;
            }
        }

        if (isGlobalScope) {
            EmitGlobal(variableDeclaration, explicitType, initializer);
            return;
        }

        auto qbeType = GetQbeType(explicitType);
        auto allocated = getCurrentBlock()->addAllocate(GetSize(qbeType));
        AddVariable(variableDeclaration->GetName(), allocated, {variableDeclaration->GetName(), explicitType, SemanticAnalyzer::Symbol::Kind::Variable, variableDeclaration->GetLine(), variableDeclaration->GetColumn()});

        if (initializer) {
            auto value = EmitExpression(initializer);
            if (value.GetType()->IsCustomType())
                addMemcpy(value, allocated, qbeType->ByteSize(module.is64Bit));
            else
                getCurrentBlock()->addStore(value, allocated);
        }
    }

    void CodeGenerator::Visit(Ast::StructDeclaration *structDeclaration) {
        std::vector<Ast::StructType::Field> fields;
        for (const auto& field : structDeclaration->GetFields()) {
            fields.push_back({field->GetName(), field->GetType()});
        }

        auto structType = new Ast::StructType(structDeclaration->GetName(), fields);
        currentScope->symbols[structDeclaration->GetName()] = {structDeclaration->GetName(), structType, SemanticAnalyzer::Symbol::Kind::Struct, structDeclaration->GetLine(), structDeclaration->GetColumn()};

        std::vector<Qbe::CustomTypeField> qbeFields;
        for (const auto& field : fields) {
            qbeFields.push_back({field.name, GetQbeType(field.type)});
        }

        auto type = module.addType(structDeclaration->GetName(), qbeFields);
        customTypes[structDeclaration->GetName()] = type;
        currentScope->symbols[structDeclaration->GetName()] = {structDeclaration->GetName(), structType, SemanticAnalyzer::Symbol::Kind::Struct, structDeclaration->GetLine(), structDeclaration->GetColumn()};
    }

    void CodeGenerator::Visit(Ast::Attribute *attribute) {
        // Ignored.
    }

    void CodeGenerator::Visit(Ast::LiteralExpression *literalExpression) {
        // Ignored.
    }

    void CodeGenerator::Visit(Ast::AssignmentExpression *assignmentExpression) {
        auto left = EmitExpression(assignmentExpression->GetLeft(), true);
        auto rightType = ResolveExpressionType(assignmentExpression->GetRight());

        if (rightType && rightType->GetLiteralType() == LiteralType::UserDefined) {
            // RHS by reference gives us the raw data pointer, no slot-load needed
            auto value = EmitExpression(assignmentExpression->GetRight(), true);
            addMemcpy(value, left, GetQbeType(rightType)->ByteSize(module.is64Bit));
        } else {
            auto value = EmitExpression(assignmentExpression->GetRight(), false);
            getCurrentBlock()->addStore(value, left);
        }
    }

    void CodeGenerator::Visit(Ast::ReturnStatement *returnStatement) {
        if (returnStatement->GetValue()) {
            auto value = EmitExpression(returnStatement->GetValue());
            // if the type is not a pointer, or custom type, we can just return the value directly. Otherwise, we need to copy it to the return slot.
            getCurrentBlock()->addReturn(value);
        } else {
            getCurrentBlock()->addReturn();
        }
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
        getCurrentBlock()->addComment("Begin function call '" + functionCallExpression->ToString() + "'");

        auto calleeExpr = functionCallExpression->GetCallee();
        auto identifierExpr = dynamic_cast<Ast::IdentifierExpression*>(calleeExpr);
        if (!identifierExpr) {
            errors.push_back({"Only direct function calls are supported for now", "", calleeExpr->GetLine(), calleeExpr->GetColumn()});
            return;
        }

        std::vector<Qbe::ValueReference> args;
        for (const auto& arg : functionCallExpression->GetArguments()) {
            auto argType = ResolveExpressionType(arg);
            if (argType->GetLiteralType() == LiteralType::UserDefined) {
                args.push_back(EmitExpression(arg, true)); // pass struct by reference
            } else {
                args.push_back(EmitExpression(arg, false)); // pass primitives by value
            }
        }

        auto sym = ResolveSymbol(identifierExpr->ToString());

        // function pointer
        auto functionType = dynamic_cast<FunctionType*>(sym->type);
        if (!functionType) {
            errors.push_back({"Symbol '" + identifierExpr->ToString() + "' is not a function", "", identifierExpr->GetLine(), identifierExpr->GetColumn()});
            return;
        }

        getCurrentBlock()->addCall(EmitExpression(identifierExpr),
            GetQbeType(functionType->GetReturnType()),
            functionType->IsVariadic(),
            functionType->GetParameterTypes().size(),
            module.is64Bit, args);

        getCurrentBlock()->addComment("End function call '" + functionCallExpression->ToString() + "'");
    }

    void CodeGenerator::Visit(Ast::PostfixExpression *postfixExpression) {
        auto operand = postfixExpression->GetOperand();
        auto op = postfixExpression->GetOperator();

        static Ast::LiteralExpression* oneLiteral = new Ast::LiteralExpression(LiteralType::Int, (uint64_t)1, -1, -1);
        // We simply insert a binary expression, with the original operand as the left hand side and a literal "1" as the right hand side.
        auto binary = new Ast::BinaryExpression(
            operand,
            op == Operator::Increment ? Operator::Add : Operator::Subtract,
            oneLiteral,
            postfixExpression->GetLine(),
            postfixExpression->GetColumn()
        );

        auto assignment = new Ast::AssignmentExpression(
            operand,
            binary,
            postfixExpression->GetLine(),
            postfixExpression->GetColumn()
        );

        Visit(assignment);
    }

    void CodeGenerator::Visit(Ast::SubscriptExpression *subscriptExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::MemberAccessExpression *memberAccessExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::IdentifierExpression *identifierExpression) {
        // Not valid but its already checked in the semantic analyzer, so we can just ignore it here.
    }

    void CodeGenerator::Visit(Ast::NestedExpression *nestedExpression) {
        nestedExpression->GetInner()->Accept(this);
    }

    void CodeGenerator::Visit(Ast::BreakStatement *breakStatement) {
        if (breakBlockStack.empty()) {
            errors.push_back({"'break' statement not within a loop", "", breakStatement->GetLine(), breakStatement->GetColumn()});
            return;
        }

        auto breakBlock = breakBlockStack.top();
        getCurrentBlock()->addUnconditionalJump(breakBlock);
    }

    void CodeGenerator::Visit(Ast::IfStatement *ifStatement) {
        auto thenBlock = currentFunction->addBlock(GetLabel("if_then"));
        auto elseBlock = currentFunction->addBlock(GetLabel("if_else"));
        auto afterBlock = currentFunction->addBlock(GetLabel("if_after"));

        auto mainBranch = ifStatement->GetMainBranch();
        auto elseIfBranches = ifStatement->GetElseIfBranches();
        auto elseBranch = ifStatement->GetElseBranch();

        auto conditionValue = EmitExpression(mainBranch.condition);
        getCurrentBlock()->addConditionalJump(thenBlock, conditionValue, elseBlock);

        // Then block
        blockStack.push(thenBlock);
        for (const auto& stmt : mainBranch.body) {
            stmt->Accept(this);
        }

        if (getCurrentBlock()->isTerminated() == false)
            getCurrentBlock()->addUnconditionalJump(afterBlock);

        blockStack.push(elseBlock);

        // Else block, if the else if statements are null, we can just directly process the else block.
        if (elseIfBranches.empty()) {
            for (const auto& stmt : elseBranch) {
                stmt->Accept(this);
            }

            if (getCurrentBlock()->isTerminated() == false)
                getCurrentBlock()->addUnconditionalJump(afterBlock);
        }
        else {
            auto finalElse = currentFunction->addBlock(GetLabel("if_final_else"));

            for (size_t i = 0; i < elseIfBranches.size(); i++) {
                auto& elseIf = elseIfBranches[i];
                auto elseIfThenBlock = currentFunction->addBlock(GetLabel("if_else_if_then"));
                auto elseIfElseBlock = (i == elseIfBranches.size() - 1) ? finalElse : currentFunction->addBlock(GetLabel("if_else_if_else"));

                auto elseIfConditionValue = EmitExpression(elseIf.condition);
                getCurrentBlock()->addConditionalJump(elseIfThenBlock, elseIfConditionValue, elseIfElseBlock);

                // Else if then block
                blockStack.push(elseIfThenBlock);
                for (const auto& stmt : elseIf.body) {
                    stmt->Accept(this);
                }

                if (getCurrentBlock()->isTerminated() == false)
                    getCurrentBlock()->addUnconditionalJump(afterBlock);

                // Else if else block
                blockStack.push(elseIfElseBlock);
            }

            // Final else block
            blockStack.push(finalElse);
            for (const auto& stmt : elseBranch) {
                stmt->Accept(this);
            }

            if (getCurrentBlock()->isTerminated() == false)
                getCurrentBlock()->addUnconditionalJump(afterBlock);
        }

        blockStack.push(afterBlock);
    }

    void CodeGenerator::Visit(Ast::ExpressionStatement *expressionStatement) {
        expressionStatement->GetExpression()->Accept(this);
    }

    void CodeGenerator::Visit(Ast::WhileStatement *whileStatement) {
        auto conditionBlock = currentFunction->addBlock(GetLabel("while_cond"));
        auto bodyBlock = currentFunction->addBlock(GetLabel("while_body"));
        auto afterBlock = currentFunction->addBlock(GetLabel("while_after"));

        breakBlockStack.push(afterBlock);
        continueBlockStack.push(conditionBlock);

        getCurrentBlock()->addUnconditionalJump(conditionBlock);

        // Condition block
        blockStack.push(conditionBlock);
        whileStatement->GetCondition()->Accept(this);
        auto conditionValue = EmitExpression(whileStatement->GetCondition());
        getCurrentBlock()->addConditionalJump(bodyBlock, conditionValue, afterBlock);

        // Body block
        blockStack.push(bodyBlock);
        for (const auto& stmt : whileStatement->GetBody()) {
            stmt->Accept(this);
        }

        if (getCurrentBlock()->isTerminated() == false)
            getCurrentBlock()->addUnconditionalJump(conditionBlock);

        // After block
        blockStack.push(afterBlock);
    }

    void CodeGenerator::Visit(Ast::ForStatement *forStatement) {
        auto conditionBlock = currentFunction->addBlock(GetLabel("for_cond"));
        auto bodyBlock = currentFunction->addBlock(GetLabel("for_body"));
        auto incrementBlock = currentFunction->addBlock(GetLabel("for_inc"));
        auto afterBlock = currentFunction->addBlock(GetLabel("for_after"));

        breakBlockStack.push(afterBlock);
        continueBlockStack.push(incrementBlock);

        EnterScope();
        forStatement->GetInitializer()->Accept(this);
        getCurrentBlock()->addUnconditionalJump(conditionBlock);

        // Condition block
        blockStack.push(conditionBlock);
        forStatement->GetCondition()->Accept(this);
        auto conditionValue = EmitExpression(forStatement->GetCondition());
        getCurrentBlock()->addConditionalJump(bodyBlock, conditionValue, afterBlock);

        // Body block
        blockStack.push(bodyBlock);
        for (const auto& stmt : forStatement->GetBody()) {
            stmt->Accept(this);
        }

        if (getCurrentBlock()->isTerminated() == false)
            getCurrentBlock()->addUnconditionalJump(incrementBlock);

        // Increment block
        blockStack.push(incrementBlock);
        forStatement->GetIncrement()->Accept(this);

        if (getCurrentBlock()->isTerminated() == false)
            getCurrentBlock()->addUnconditionalJump(conditionBlock);

        // After block
        blockStack.push(afterBlock);
    }

    void CodeGenerator::Visit(Ast::ImportStatement *importStatement) {
    }

    void CodeGenerator::Visit(Ast::ContinueStatement *continueStatement) {
        if (continueBlockStack.empty()) {
            errors.push_back({"'continue' statement not within a loop", "", continueStatement->GetLine(), continueStatement->GetColumn()});
            return;
        }

        auto continueBlock = continueBlockStack.top();
        getCurrentBlock()->addUnconditionalJump(continueBlock);
    }

    void CodeGenerator::Visit(Type *type) {
    }

    void CodeGenerator::Visit(Ast::DereferenceAssignmentStatement *dereferenceAssignmentStatement) {
        auto address = EmitExpression(dereferenceAssignmentStatement->GetTarget());
        auto value = EmitExpression(dereferenceAssignmentStatement->GetValue());

        if (address.GetType()->IsCustomType())
            addMemcpy(value, address, value.GetType()->ByteSize(module.is64Bit));
        else
            getCurrentBlock()->addStore(value, address);
    }

    void CodeGenerator::Visit(Ast::StructInitializerExpression *structInitializerExpression) {
        // Does nothing yet.
    }

    void CodeGenerator::Visit(Ast::PointerMemberAccessExpression *pointerMemberAccessExpression) {
        auto object = EmitExpression(pointerMemberAccessExpression->GetObject());
        auto memberName = pointerMemberAccessExpression->GetMemberName();

    }

    void CodeGenerator::Visit(Ast::TypeExpression *typeExpression) {
    }

    Qbe::ITypeDefinition * CodeGenerator::GetQbeType(Type *type) {
        if (type->GetLiteralType() != LiteralType::UserDefined) {
            switch (type->GetLiteralType()) {
                // Integers are rounded *up* to a full word (32 bits) in QBE.
                case LiteralType::U8:
                case LiteralType::U16:
                case LiteralType::U32:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Int32, false); // false for unsigned
                case LiteralType::I8:
                case LiteralType::I16:
                case LiteralType::I32:
                case LiteralType::Int:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Int32, true);

                case LiteralType::U64:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Int64, false);
                case LiteralType::I64:
                case LiteralType::Long:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Int64, true);
                case LiteralType::Pointer:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer); // we lose nested information, but that does not matter.
                case LiteralType::F32:
                case LiteralType::Float:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Float32, true);
                case LiteralType::F64:
                case LiteralType::Double:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Float64, true);
                case LiteralType::Void:
                    return new Qbe::VoidType();
                case LiteralType::Function:
                    return new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer); // Functions are just pointers, to the location of said function.

                default: {
                    errors.push_back({"Unsupported type: " + type->ToString(), "", 0, 0});
                    return nullptr;
                }
            }
        }

        auto sym = ResolveSymbol(type->GetIdentifier());
        if (!sym) {
            errors.push_back({"Undefined type: " + type->GetIdentifier(), "", 0, 0});
            return nullptr;
        }

        auto structType = dynamic_cast<Ast::StructType *>(sym->type);
        if (!structType) {
            errors.push_back({"Symbol '" + structType->GetIdentifier() + "' is not a type", "", 0, 0});
            return nullptr;
        }

        auto it = customTypes.find(structType->GetIdentifier());
        if (it == customTypes.end()) {
            errors.push_back({"Type '" + structType->GetIdentifier() + "' not found in custom types", "", 0, 0});
            return nullptr;
        }

        return it->second;
    }

    Type *CodeGenerator::ResolveExpressionType(Ast::Expression *expression) {
        if (auto* lit = dynamic_cast<Ast::LiteralExpression*>(expression)) {
            if (lit->GetLiteralType() == LiteralType::Pointer && std::holds_alternative<uint64_t>(lit->GetValue()) && std::get<uint64_t>(lit->GetValue()) == 0) {
                return new Type(LiteralType::Pointer, 0, new Type(LiteralType::Void)); // null pointer literal has type "pointer to void"
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

        if (auto* nested = dynamic_cast<Ast::NestedExpression*>(expression)) {
            return ResolveExpressionType(nested->GetInner());
        }

        if (auto* unary = dynamic_cast<Ast::UnaryExpression*>(expression)) {
            return ResolveExpressionType(unary->GetOperand());
        }

        if (auto* subscript = dynamic_cast<Ast::SubscriptExpression*>(expression)) {
            auto arrayType = ResolveExpressionType(subscript->GetArray());
            if (arrayType && arrayType->GetNestedType()) {
                return arrayType->GetNestedType();
            }
        }

        if (auto* call = dynamic_cast<Ast::FunctionCallExpression*>(expression)) {
            auto* callee = dynamic_cast<Ast::IdentifierExpression*>(call->GetCallee());
            if (callee) {
                auto* sym = ResolveSymbol(callee->ToString());
                auto *funcType = dynamic_cast<FunctionType*>(sym ? sym->type : nullptr);
                return funcType ? funcType->GetReturnType() : nullptr;
            }
        }

        if (auto* cast = dynamic_cast<Ast::CastExpression*>(expression)) {
            return cast->GetTargetType();
        }

        if (auto* memberAccess = dynamic_cast<Ast::MemberAccessExpression*>(expression)) {
            auto structType = ResolveExpressionType(memberAccess->GetObject());
            if (!structType) {
                errors.push_back({"Could not resolve type of struct in member access", "", memberAccess->GetLine(), memberAccess->GetColumn()});
                return nullptr;
            }

            if (structType->GetLiteralType() != LiteralType::UserDefined) {
                errors.push_back({"Member access on non-struct type '" + structType->ToString() + "'", "", memberAccess->GetLine(), memberAccess->GetColumn()});
                return nullptr;
            }

            auto sym = ResolveSymbol(structType->GetIdentifier());
            if (!sym) {
                errors.push_back({"Undefined type: " + structType->GetIdentifier(), "", memberAccess->GetLine(), memberAccess->GetColumn()});
                return nullptr;
            }

            auto structDef = dynamic_cast<Ast::StructType *>(sym->type);
            if (!structDef) {
                errors.push_back({"Symbol '" + structType->GetIdentifier() + "' is not a type", "", memberAccess->GetLine(), memberAccess->GetColumn()});
                return nullptr;
            }

            for (const auto& field : structDef->GetFields()) {
                if (field.name == memberAccess->GetMemberName()) {
                    return field.type;
                }
            }

            errors.push_back({"Struct '" + structDef->GetIdentifier() + "' does not have a member named '" + memberAccess->GetMemberName() + "'", "", memberAccess->GetLine(), memberAccess->GetColumn()});
            return nullptr;
        }

        if (auto* pointerMemberAccess = dynamic_cast<Ast::PointerMemberAccessExpression*>(expression)) {
            auto structType = ResolveExpressionType(pointerMemberAccess->GetObject());
            if (!structType) {
                errors.push_back({"Could not resolve type of struct in pointer member access", "", pointerMemberAccess->GetLine(), pointerMemberAccess->GetColumn()});
                return nullptr;
            }

            if (structType->GetNestedType() && structType->GetLiteralType() == LiteralType::Pointer && structType->GetNestedType()->GetLiteralType() == LiteralType::UserDefined) {
                structType = structType->GetNestedType();
            }
            else {
                errors.push_back({"Member access on non-struct type '" + structType->ToString() + "'", "", pointerMemberAccess->GetLine(), pointerMemberAccess->GetColumn()});
                return nullptr;
            }

            auto sym = ResolveSymbol(structType->GetIdentifier());
            if (!sym) {
                errors.push_back({"Undefined type: " + structType->GetIdentifier(), "", pointerMemberAccess->GetLine(), pointerMemberAccess->GetColumn()});
                return nullptr;
            }

            auto structDef = dynamic_cast<Ast::StructType *>(sym->type);
            if (!structDef) {
                errors.push_back({"Symbol '" + structType->GetIdentifier() + "' is not a type", "", pointerMemberAccess->GetLine(), pointerMemberAccess->GetColumn()});
                return nullptr;
            }

            for (const auto& field : structDef->GetFields()) {
                if (field.name == pointerMemberAccess->GetMemberName()) {
                    return field.type;
                }
            }

            errors.push_back({"Struct '" + structDef->GetIdentifier() + "' does not have a member named '" + pointerMemberAccess->GetMemberName() + "'", "", pointerMemberAccess->GetLine(), pointerMemberAccess->GetColumn()});
            return nullptr;
        }

        if (auto* structInit = dynamic_cast<Ast::StructInitializerExpression*>(expression)) {
            auto structType = ResolveExpressionType(structInit->GetStructIdentifier());
            if (!structType) {
                errors.push_back({"Could not resolve type of struct in struct initializer", "", structInit->GetLine(), structInit->GetColumn()});
                return nullptr;
            }

            if (structType->GetLiteralType() != LiteralType::UserDefined) {
                errors.push_back({"Struct initializer for non-struct type '" + structType->ToString() + "'", "", structInit->GetLine(), structInit->GetColumn()});
                return nullptr;
            }

            auto sym = ResolveSymbol(structType->GetIdentifier());
            if (!sym) {
                errors.push_back({"Undefined type: " + structType->GetIdentifier(), "", structInit->GetLine(), structInit->GetColumn()});
                return nullptr;
            }

            auto structDef = dynamic_cast<Ast::StructType *>(sym->type);
            if (!structDef) {
                errors.push_back({"Symbol '" + structType->GetIdentifier() + "' is not a type", "", structInit->GetLine(), structInit->GetColumn()});
                return nullptr;
            }

            return structType;
        }

        if (auto* sizeofType = dynamic_cast<Ast::SizeOfExpression*>(expression)) {
            return new Type(LiteralType::U64); // sizeof always returns a 64-bit unsigned integer
        }

        if (auto* dereference = dynamic_cast<Ast::DereferenceExpression*>(expression)) {
            auto pointerType = ResolveExpressionType(dereference->GetOperand());
            if (pointerType && pointerType->GetLiteralType() == LiteralType::Pointer && pointerType->GetNestedType()) {
                return pointerType->GetNestedType();
            }
        }

        if (auto* addressOfExpression = dynamic_cast<Ast::AddressOfExpression*>(expression)) {
            auto operandType = ResolveExpressionType(addressOfExpression->GetOperand());
            if (operandType) {
                return new Type(LiteralType::Pointer, 0, operandType);
            }
        }

        errors.push_back({"Could not resolve type of expression", "", expression->GetLine(), expression->GetColumn()});
        return nullptr;
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

    size_t CodeGenerator::GetStructMemberOffset(Ast::Expression *expression, const std::string &string) {
        auto structType = ResolveExpressionType(expression);
        if (!structType) {
            errors.push_back({"Could not resolve type of struct in member access", "", expression->GetLine(), expression->GetColumn()});
            return 0;
        }

        if (structType->GetNestedType() && structType->GetLiteralType() == LiteralType::Pointer && structType->GetNestedType()->GetLiteralType() == LiteralType::UserDefined) {
            structType = structType->GetNestedType();
        } else if (structType->GetLiteralType() != LiteralType::UserDefined) {
            errors.push_back({"Member access on non-struct type '" + structType->ToString() + "'", "", expression->GetLine(), expression->GetColumn()});
            return 0;
        }

        auto sym = ResolveSymbol(structType->GetIdentifier());
        if (!sym) {
            errors.push_back({"Undefined type: " + structType->GetIdentifier(), "", expression->GetLine(), expression->GetColumn()});
            return 0;
        }

        auto structDef = dynamic_cast<Ast::StructType *>(sym->type);
        if (!structDef) {
            errors.push_back({"Symbol '" + structType->GetIdentifier() + "' is not a type", "", expression->GetLine(), expression->GetColumn()});
            return 0;
        }

        size_t offset = 0;
        for (const auto& field : structDef->GetFields()) {
            if (field.name == string) {
                return offset;
            }
            offset += GetQbeType(field.type)->ByteSize(module.is64Bit);
        }

        errors.push_back({"Struct '" + structDef->GetIdentifier() + "' does not have a member named '" + string + "'", "", expression->GetLine(), expression->GetColumn()});
        return 0;
    }

    Qbe::ValueReference CodeGenerator::GetStructAddress(Ast::Expression *expression) {
        if (auto *ident = dynamic_cast<Ast::IdentifierExpression *>(expression)) {
            auto var = ResolveVariable(ident->ToString());
            if (!var) {
                errors.push_back({"Undefined variable: " + ident->ToString(), "", ident->GetLine(), ident->GetColumn()});
                return Qbe::ValueReference();
            }

            return *var;
        }

        if (auto *nested = dynamic_cast<Ast::NestedExpression *>(expression)) {
            return GetStructAddress(nested->GetInner());
        }

        if (auto *memberAccess = dynamic_cast<Ast::MemberAccessExpression *>(expression)) {
            auto address = GetStructAddress(memberAccess->GetObject());
            auto offset = GetStructMemberOffset(memberAccess->GetObject(), memberAccess->GetMemberName());
            return getCurrentBlock()->addAdd(address, Qbe::CreateLiteral((int64_t)offset));
        }

        if (auto *dereference = dynamic_cast<Ast::DereferenceExpression *>(expression)) {
            return EmitExpression(dereference->GetOperand());
        }

        if (auto *subscript = dynamic_cast<Ast::SubscriptExpression *>(expression)) {
            auto arrayAddress = GetStructAddress(subscript->GetArray());
            auto indexValue = EmitExpression(subscript->GetIndex());
            auto elementSize = GetQbeType(ResolveExpressionType(subscript->GetArray())->GetNestedType())->ByteSize(module.is64Bit);
            auto offset = getCurrentBlock()->addMul(indexValue, Qbe::CreateLiteral((int64_t)elementSize));
            return getCurrentBlock()->addAdd(arrayAddress, offset);
        }

        if (auto *pointerMemberAccess = dynamic_cast<Ast::PointerMemberAccessExpression *>(expression)) {
            auto address = GetStructAddress(pointerMemberAccess->GetObject());
            auto offset = GetStructMemberOffset(pointerMemberAccess->GetObject(), pointerMemberAccess->GetMemberName());
            return getCurrentBlock()->addAdd(address, Qbe::CreateLiteral((int64_t)offset));
        }

        errors.push_back({"Unsupported expression for getting struct address", "", expression->GetLine(), expression->GetColumn()});
        return Qbe::ValueReference();
    }

    Qbe::ValueReference CodeGenerator::EmitExpression(Ast::Expression *expression, bool getReference) {
        if (auto *literal = dynamic_cast<Ast::LiteralExpression *>(expression)) {
            if (literal->GetLiteralType() == LiteralType::Pointer && std::holds_alternative<std::string>(literal->GetValue())) {
                std::string str = std::get<std::string>(literal->GetValue());
                std::string escapedStr = escapeStringLiteral(str);
                auto global = module.addGlobal(escapedStr);
                return global;
            }

            auto type = literal->GetLiteralType();
            auto value = literal->GetValue();
            switch (type) {
                case LiteralType::U8:
                case LiteralType::U16:
                case LiteralType::U32:
                    return Qbe::ValueReference(new Qbe::Literal((int32_t)std::get<uint64_t>(value), false));
                case LiteralType::I8:
                case LiteralType::I16:
                case LiteralType::I32:
                case LiteralType::Int:
                    return Qbe::ValueReference(new Qbe::Literal((int32_t)std::get<uint64_t>(value), true));
                case LiteralType::U64:
                    return Qbe::ValueReference(new Qbe::Literal((int64_t)std::get<uint64_t>(value), false));
                case LiteralType::I64:
                case LiteralType::Long:
                    return Qbe::ValueReference(new Qbe::Literal((int64_t)std::get<uint64_t>(value), true));
                case LiteralType::F32:
                case LiteralType::Float:
                    return Qbe::ValueReference(new Qbe::Literal((float)std::get<double>(value)));
                case LiteralType::F64:
                case LiteralType::Double:
                    return Qbe::ValueReference(new Qbe::Literal(std::get<double>(value)));
                case LiteralType::Pointer:
                    return Qbe::ValueReference(Qbe::Literal::CreatePointerLiteral(std::get<uint64_t>(value)));
                default:
                    errors.push_back({"Unsupported literal type: " + LiteralToString[type], "", literal->GetLine(), literal->GetColumn()});
                    return Qbe::ValueReference();
            }
        }

        if (auto *ident = dynamic_cast<Ast::IdentifierExpression *>(expression)) {
            if (functions.find(ident->ToString()) != functions.end()) {
                return Qbe::ValueReference(new Qbe::Global(functions[ident->ToString()]->identifier, "")); // function name as pointer
            }

            auto var = ResolveVariable(ident->ToString());
            if (!var) {
                errors.push_back({"Undefined variable: " + ident->ToString(), "", ident->GetLine(), ident->GetColumn()});
                return Qbe::ValueReference();
            }

            if (getReference) {
                return *var;
            }

            auto type = ResolveExpressionType(expression);
            auto loaded = getCurrentBlock()->addLoad(*var, GetQbeType(type));
            return loaded;
        }

        if (auto *nested = dynamic_cast<Ast::NestedExpression *>(expression)) {
            return EmitExpression(nested->GetInner(), getReference);
        }

        if (auto *unary = dynamic_cast<Ast::UnaryExpression *>(expression)) {
            auto operandValue = EmitExpression(unary->GetOperand(), false);
            switch (unary->GetOperator()) {
                case Operator::Subtract:
                    return getCurrentBlock()->addNegate(operandValue);
                default:
                    errors.push_back({"Unsupported unary operator in code generation", "", unary->GetLine(), unary->GetColumn()});
                    return Qbe::ValueReference();
            }
        }

        if (auto *bin = dynamic_cast<Ast::BinaryExpression *>(expression)) {
            auto leftValue = EmitExpression(bin->GetLeft(), false);
            auto rightValue = EmitExpression(bin->GetRight(), false);
            switch (bin->GetOperator()) {
                case Operator::Add:
                    return getCurrentBlock()->addAdd(leftValue, rightValue);
                case Operator::Subtract:
                    return getCurrentBlock()->addSub(leftValue, rightValue);
                case Operator::Multiply:
                    return getCurrentBlock()->addMul(leftValue, rightValue);
                case Operator::Divide:
                    return getCurrentBlock()->addDiv(leftValue, rightValue);
                case Operator::Modulus:
                    return getCurrentBlock()->addRem(leftValue, rightValue);
                case Operator::Equal:
                    return getCurrentBlock()->addEquality(leftValue, rightValue, Qbe::Instructions::EqualityOperation::Equal);
                case Operator::NotEqual:
                    return getCurrentBlock()->addEquality(leftValue, rightValue, Qbe::Instructions::EqualityOperation::NotEqual);
                case Operator::LessThan:
                    return getCurrentBlock()->addEquality(leftValue, rightValue, Qbe::Instructions::EqualityOperation::LessThan);
                case Operator::LessThanOrEqual:
                    return getCurrentBlock()->addEquality(leftValue, rightValue, Qbe::Instructions::EqualityOperation::LessThanOrEqual);
                case Operator::GreaterThan:
                    return getCurrentBlock()->addEquality(leftValue, rightValue, Qbe::Instructions::EqualityOperation::GreaterThan);
                case Operator::GreaterThanOrEqual:
                    return getCurrentBlock()->addEquality(leftValue, rightValue, Qbe::Instructions::EqualityOperation::GreaterThanOrEqual);
                case Operator::LogicalAnd:
                    return getCurrentBlock()->addAnd(leftValue, rightValue);
                case Operator::LogicalOr:
                    return getCurrentBlock()->addOr(leftValue, rightValue);
                case Operator::BitwiseAnd:
                    return getCurrentBlock()->addBitwise(leftValue, rightValue, Qbe::Instructions::BitwiseOperation::And);
                case Operator::BitwiseOr:
                    return getCurrentBlock()->addBitwise(leftValue, rightValue, Qbe::Instructions::BitwiseOperation::Or);
                case Operator::BitwiseXor:
                    return getCurrentBlock()->addBitwise(leftValue, rightValue, Qbe::Instructions::BitwiseOperation::Xor);
                case Operator::ShiftLeft:
                    return getCurrentBlock()->addShift(leftValue, rightValue, Qbe::Instructions::ShiftOperation::ShiftLeft);
                case Operator::ShiftRight:
                    return getCurrentBlock()->addShift(leftValue, rightValue, Qbe::Instructions::ShiftOperation::ShiftRight);
                default:
                    errors.push_back({"Unsupported binary operator in code generation", "", bin->GetLine(), bin->GetColumn()});
                    return Qbe::ValueReference();
            }
        }

        if (auto *subscript = dynamic_cast<Ast::SubscriptExpression *>(expression)) {
            auto arrayValue = EmitExpression(subscript->GetArray(), false);
            auto indexValue = EmitExpression(subscript->GetIndex(), false);
            static Qbe::Primitive* targetType = new Qbe::Primitive(Qbe::TypeDefinitionKind::Pointer, false);

            if (indexValue.GetType()->IsInteger() && !indexValue.GetType()->IsSigned() && indexValue.GetType()->ByteSize(module.is64Bit) == targetType->ByteSize(module.is64Bit)) {
                // No cast needed
            } else {
                indexValue = getCurrentBlock()->addConversion(indexValue, targetType, module.is64Bit);
            }

            // This turns from array[index] into *(array + index)
            auto elementType = ResolveExpressionType(subscript->GetArray());
            if (!elementType || !elementType->GetNestedType()) {
                errors.push_back({"Subscript operator applied to non-array type", "", subscript->GetLine(), subscript->GetColumn()});
                return Qbe::ValueReference();
            }
            auto elementSize = GetSize(GetQbeType(elementType->GetNestedType()));
            auto offset = getCurrentBlock()->addMul(indexValue, Qbe::ValueReference(elementSize));
            auto address = getCurrentBlock()->addAdd(arrayValue, offset);

            if (getReference) {
                return address;
            }

            return getCurrentBlock()->addLoad(address, GetQbeType(elementType->GetNestedType()));
        }

        if (auto *cast = dynamic_cast<Ast::CastExpression *>(expression)) {
            auto value = EmitExpression(cast->GetOperand(), false); // we cant allow getting a reference to a cast operand, that does not make sense.
            auto targetType = GetQbeType(cast->GetTargetType());

            if (value.GetType()->IsEqual(*targetType)) {
                return value; // no cast needed
            }

            if (auto *primitive = dynamic_cast<Qbe::Primitive *>(targetType))
                return getCurrentBlock()->addConversion(value, primitive, module.is64Bit);

            errors.push_back({"Unsupported cast target type in code generation", "", cast->GetLine(), cast->GetColumn()});
            return Qbe::ValueReference();
        }

        if (auto *call = dynamic_cast<Ast::FunctionCallExpression *>(expression)) {
            auto calleeExpr = call->GetCallee();
            auto identifierExpr = dynamic_cast<Ast::IdentifierExpression*>(calleeExpr);
            if (!identifierExpr) {
                errors.push_back({"Only direct function calls are supported for now", "", calleeExpr->GetLine(), calleeExpr->GetColumn()});
                return Qbe::ValueReference();
            }

            std::vector<Qbe::ValueReference> args;
            for (const auto& arg : call->GetArguments()) {
                auto argType = ResolveExpressionType(arg);
                if (argType->GetLiteralType() == LiteralType::UserDefined) {
                    args.push_back(EmitExpression(arg, true)); // pass struct by reference
                } else {
                    args.push_back(EmitExpression(arg, false)); // pass primitives by value
                }
            }

            auto sym = ResolveSymbol(identifierExpr->ToString());

            // function pointer
            auto functionType = dynamic_cast<FunctionType*>(sym->type);
            if (!functionType) {
                errors.push_back({"Symbol '" + identifierExpr->ToString() + "' is not a function", "", identifierExpr->GetLine(), identifierExpr->GetColumn()});
                return Qbe::ValueReference();
            }

            return getCurrentBlock()->addCall(EmitExpression(identifierExpr),
                GetQbeType(functionType->GetReturnType()),
                functionType->IsVariadic(),
                functionType->GetParameterTypes().size(),
                module.is64Bit, args);
        }

        if (auto *addressOf = dynamic_cast<Ast::AddressOfExpression *>(expression)) {
            auto operand = addressOf->GetOperand();
            if (auto *ident = dynamic_cast<Ast::IdentifierExpression *>(operand)) {
                auto var = ResolveVariable(ident->ToString());
                if (!var) {
                    errors.push_back({"Undefined variable: " + ident->ToString(), "", ident->GetLine(), ident->GetColumn()});
                    return Qbe::ValueReference();
                }

                return *var;
            } else if (auto *memberAccess = dynamic_cast<Ast::MemberAccessExpression *>(operand)) {
                return GetStructAddress(memberAccess);
            } else if (auto *subscript = dynamic_cast<Ast::SubscriptExpression *>(operand)) {
                return GetStructAddress(subscript);
            } else if (auto *dereference = dynamic_cast<Ast::DereferenceExpression *>(operand)) {
                return EmitExpression(dereference->GetOperand());
            } else if (auto *pointerMemberAccess = dynamic_cast<Ast::PointerMemberAccessExpression *>(operand)) {
                auto baseAddress = EmitExpression(pointerMemberAccess->GetObject());
                auto offset = GetStructMemberOffset(pointerMemberAccess->GetObject(), pointerMemberAccess->GetMemberName());
                return getCurrentBlock()->addAdd(baseAddress, Qbe::CreateLiteral((int64_t)offset));
            }
            else {
                errors.push_back({"Address-of operator can only be applied to variables for now", "", operand->GetLine(), operand->GetColumn()});
                return Qbe::ValueReference();
            }
        }

        if (auto *dereferenceExpression = dynamic_cast<Ast::DereferenceExpression *>(expression)) {
            auto operandValue = EmitExpression(dereferenceExpression->GetOperand(), false);
            auto operandType = ResolveExpressionType(dereferenceExpression->GetOperand());
            if (!operandType || operandType->GetLiteralType() != LiteralType::Pointer) {
                errors.push_back({"Dereference operator applied to non-pointer type", "", dereferenceExpression->GetLine(), dereferenceExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            if (getReference) {
                return operandValue;
            }

            auto innerType = operandType->GetNestedType();
            if (innerType->GetLiteralType() == LiteralType::UserDefined) {
                return operandValue;
            }

            return getCurrentBlock()->addLoad(operandValue, GetQbeType(innerType));
        }

        if (auto *memberAccessExpression = dynamic_cast<Ast::MemberAccessExpression *>(expression)) {

            // Get the memory offset to the member. We can then add that offset to the base address of the struct to get the address of the member.
            auto offset = GetStructMemberOffset(memberAccessExpression->GetObject(), memberAccessExpression->GetMemberName());
            auto base = GetStructAddress(memberAccessExpression->GetObject());

            if (getReference) {
                return getCurrentBlock()->addAdd(base, Qbe::ValueReference(new Qbe::Literal((int64_t)offset, false)));
            }

            auto memberType = ResolveExpressionType(memberAccessExpression->GetObject());
            if (!memberType) {
                errors.push_back({"Could not resolve type of struct in member access", "", memberAccessExpression->GetLine(), memberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            if (memberType->GetLiteralType() != LiteralType::UserDefined) {
                errors.push_back({"Member access on non-struct type '" + memberType->ToString() + "'", "", memberAccessExpression->GetLine(), memberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto sym = ResolveSymbol(memberType->GetIdentifier());
            if (!sym) {
                errors.push_back({"Undefined type: " + memberType->GetIdentifier(), "", memberAccessExpression->GetLine(), memberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto structDef = dynamic_cast<Ast::StructType *>(sym->type);
            if (!structDef) {
                errors.push_back({"Symbol '" + memberType->GetIdentifier() + "' is not a type", "", memberAccessExpression->GetLine(), memberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto fieldIt = std::find_if(structDef->GetFields().begin(), structDef->GetFields().end(), [&](const Ast::StructType::Field& f) {
                return f.name == memberAccessExpression->GetMemberName();
            });

            if (fieldIt == structDef->GetFields().end()) {
                errors.push_back({"Struct '" + structDef->GetIdentifier() + "' does not have a member named '" + memberAccessExpression->GetMemberName() + "'", "", memberAccessExpression->GetLine(), memberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto memberTypeQbe = GetQbeType(fieldIt->type);
            auto fieldAddress = getCurrentBlock()->addAdd(base, Qbe::ValueReference(new Qbe::Literal((int64_t)offset, false)));

            // Struct-typed fields are already addressable, load would dereference the struct as a pointer
            if (fieldIt->type->GetLiteralType() == LiteralType::UserDefined) {
                auto dest = getCurrentBlock()->addAllocate(Qbe::ValueReference(new Qbe::Literal((int64_t)memberTypeQbe->ByteSize(module.is64Bit), false)));
                dest.value.local->type = memberTypeQbe;
                addMemcpy(fieldAddress, dest, memberTypeQbe->ByteSize(module.is64Bit));
                return dest;
            }

            return getCurrentBlock()->addLoad(fieldAddress, memberTypeQbe);
        }

        if (auto *pointerMemberAccessExpression = dynamic_cast<Ast::PointerMemberAccessExpression *>(expression)) {
            auto base = EmitExpression(pointerMemberAccessExpression->GetObject(), false);
            auto baseType = ResolveExpressionType(pointerMemberAccessExpression->GetObject()); // pointer to struct
            if (!baseType) {
                errors.push_back({"Could not resolve type of struct in pointer member access", "", pointerMemberAccessExpression->GetLine(), pointerMemberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            if (baseType->GetNestedType() && baseType->GetLiteralType() == LiteralType::Pointer && baseType->GetNestedType()->GetLiteralType() == LiteralType::UserDefined) {
                baseType = baseType->GetNestedType();
            }
            else {
                errors.push_back({"Pointer member access on non-struct type '" + baseType->ToString() + "'", "", pointerMemberAccessExpression->GetLine(), pointerMemberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto sym = ResolveSymbol(baseType->GetIdentifier());
            if (!sym) {
                errors.push_back({"Undefined type: " + baseType->GetIdentifier(), "", pointerMemberAccessExpression->GetLine(), pointerMemberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto structDef = dynamic_cast<Ast::StructType *>(sym->type);
            if (!structDef) {
                errors.push_back({"Symbol '" + baseType->GetIdentifier() + "' is not a type", "", pointerMemberAccessExpression->GetLine(), pointerMemberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto fieldIt = std::find_if(structDef->GetFields().begin(), structDef->GetFields().end(), [&](const Ast::StructType::Field& f) {
                return f.name == pointerMemberAccessExpression->GetMemberName();
            });

            if (fieldIt == structDef->GetFields().end()) {
                errors.push_back({"Struct '" + structDef->GetIdentifier() + "' does not have a member named '" + pointerMemberAccessExpression->GetMemberName() + "'", "", pointerMemberAccessExpression->GetLine(), pointerMemberAccessExpression->GetColumn()});
                return Qbe::ValueReference();
            }

            auto offset = GetStructMemberOffset(pointerMemberAccessExpression->GetObject(), pointerMemberAccessExpression->GetMemberName());
            auto memberTypeQbe = GetQbeType(fieldIt->type);

            auto fieldAddress = getCurrentBlock()->addAdd(base, Qbe::ValueReference(new Qbe::Literal((int64_t)offset, false)));

            if (getReference) {
                return fieldAddress;
            }

            if (fieldIt->type->GetLiteralType() == LiteralType::UserDefined) {
                auto dest = getCurrentBlock()->addAllocate(Qbe::ValueReference(new Qbe::Literal((int64_t)memberTypeQbe->ByteSize(module.is64Bit), false)));
                addMemcpy(fieldAddress, dest, memberTypeQbe->ByteSize(module.is64Bit));
                dest.value.local->type = memberTypeQbe;
                return dest;
            }

            return getCurrentBlock()->addLoad(fieldAddress, memberTypeQbe);
        }

        if (auto *structInit = dynamic_cast<Ast::StructInitializerExpression *>(expression)) {
            auto allocated = getCurrentBlock()->addAllocate(Qbe::ValueReference(new Qbe::Literal((int64_t)GetQbeType(ResolveExpressionType(structInit->GetStructIdentifier()))->ByteSize(module.is64Bit), false)));
            auto structType = ResolveExpressionType(structInit->GetStructIdentifier());
            if (!structType) {
                errors.push_back({"Could not resolve type of struct in struct initializer", "", structInit->GetLine(), structInit->GetColumn()});
                return Qbe::ValueReference();
            }

            if (structType->GetLiteralType() != LiteralType::UserDefined) {
                errors.push_back({"Struct initializer for non-struct type '" + structType->ToString() + "'", "", structInit->GetLine(), structInit->GetColumn()});
                return Qbe::ValueReference();
            }

            auto sym = ResolveSymbol(structType->GetIdentifier());
            if (!sym) {
                errors.push_back({"Undefined type: " + structType->GetIdentifier(), "", structInit->GetLine(), structInit->GetColumn()});
                return Qbe::ValueReference();
            }

            auto structDef = dynamic_cast<Ast::StructType *>(sym->type);
            if (!structDef) {
                errors.push_back({"Symbol '" + structType->GetIdentifier() + "' is not a type", "", structInit->GetLine(), structInit->GetColumn()});
                return Qbe::ValueReference();
            }

            for (const auto& fieldInit : structInit->GetInitializers()) {
                auto fieldIt = std::find_if(structDef->GetFields().begin(), structDef->GetFields().end(), [&](const Ast::StructType::Field& f) {
                    return f.name == fieldInit.first;
                });

                if (fieldIt == structDef->GetFields().end()) {
                    errors.push_back({"Struct '" + structDef->GetIdentifier() + "' does not have a member named '" + fieldInit.first + "'", "", structInit->GetLine(), structInit->GetColumn()});
                    continue;
                }

                auto offset = GetStructMemberOffset(structInit->GetStructIdentifier(), fieldInit.first);
                auto value = EmitExpression(fieldInit.second);
                if (value.GetType()->IsCustomType())
                    addMemcpy(value, getCurrentBlock()->addAdd(allocated, Qbe::ValueReference(new Qbe::Literal((int64_t)offset, false))), GetQbeType(ResolveExpressionType(fieldInit.second))->ByteSize(module.is64Bit));
                else
                    getCurrentBlock()->addStore(value, getCurrentBlock()->addAdd(allocated, Qbe::ValueReference(new Qbe::Literal((int64_t)offset, false))));
            }

            allocated.value.local->type = GetQbeType(structType);
            return allocated;
        }

        if (auto* sizeofExpr = dynamic_cast<Ast::SizeOfExpression*>(expression)) {
            auto type = sizeofExpr->GetType();
            auto qbeType = GetQbeType(type);
            if (!qbeType) {
                errors.push_back({"Could not resolve type in sizeof expression", "", sizeofExpr->GetLine(), sizeofExpr->GetColumn()});
                return Qbe::ValueReference();
            }

            return Qbe::ValueReference(new Qbe::Literal((int64_t)qbeType->ByteSize(module.is64Bit), false));
        }

        errors.push_back({"Expression type not supported yet in code generation", "", expression->GetLine(), expression->GetColumn()});
        return Qbe::ValueReference();
    }
} // Oxy