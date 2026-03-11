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
        for (const auto& structDecl : root->GetStructs()) {
            Visit(structDecl);
        }

        for (const auto& varDecl : root->GetVariables()) {
            Visit(varDecl);
        }

        // Predefine all function signatures before visiting function bodies to allow for recursion and non-linear function definitions.
        for (const auto& func : root->GetFunctions()) {
            auto identifier = func->GetName();

            auto returnType = func->GetReturnType();
            if (!returnType) {
                returnType = new Type(LiteralType::Void);
            }

            auto qbeReturnType = GetQbeType(returnType);
            if (!qbeReturnType) {
                errors.push_back({fmt::format("Unsupported return type '{}' for function '{}'", returnType->ToString(), identifier), "", func->GetLine(), func->GetColumn()});
                return;
            }

            std::vector<Qbe::Local> args;
            for (const auto& param : func->GetParameters()) {
                auto paramType = param->GetType();
                if (!paramType) {
                    errors.push_back({fmt::format("Parameter '{}' in function '{}' has no type", param->GetName(), identifier), "", param->GetLine(), param->GetColumn()});
                    return;
                }

                auto qbeParamType = GetQbeType(paramType);
                if (!qbeParamType) {
                    errors.push_back({fmt::format("Unsupported parameter type '{}' for parameter '{}' in function '{}'", paramType->ToString(), param->GetName(), identifier), "", param->GetLine(), param->GetColumn()});
                    return;
                }

                Qbe::Local arg{param->GetName(), qbeParamType, true};
                args.push_back(arg);
            }

            Qbe::FunctionFlags flags = Qbe::FunctionFlags::None;
            bool isExternal = false;
            std::string symbol = identifier;

            for (const auto& attr : func->GetAttributes()) {
                if (attr->GetName() == "thread") {
                    flags = (Qbe::FunctionFlags)((int)flags | (int)Qbe::FunctionFlags::Thread);
                }
                else if (attr->GetName() == "entry") {
                    flags = (Qbe::FunctionFlags)((int)flags | (int)Qbe::FunctionFlags::EntryPoint);
                }
                else if (attr->GetName() == "extern") {
                    if (func->GetBody().size() > 0) {
                        errors.push_back({fmt::format("Function '{}' is marked as 'extern' but has a body", identifier), "", attr->GetLine(), attr->GetColumn()});
                        return;
                    }

                    isExternal = true;
                }
                else if (attr->GetName() == "symbol") {
                    if (attr->GetArgs().size() != 1) {
                        errors.push_back({fmt::format("Attribute 'symbol' on function '{}' requires exactly one argument", identifier), "", attr->GetLine(), attr->GetColumn()});
                        return;
                    }

                    auto arg = attr->GetArgs()[0];
                    auto literalArg = dynamic_cast<Ast::LiteralExpression*>(arg);
                    if (!literalArg || literalArg->GetLiteralType() != LiteralType::Pointer) {
                        errors.push_back({fmt::format("Argument to 'symbol' attribute on function '{}' must be a string literal", identifier), "", arg->GetLine(), arg->GetColumn()});
                        return;
                    }

                    symbol = std::get<std::string>(literalArg->GetValue());
                }
                else {
                    errors.push_back({fmt::format("Unsupported attribute '{}' on function '{}'", attr->GetName(), identifier), "", attr->GetLine(), attr->GetColumn()});
                    return;
                }
            }

            if (!isExternal) {
                if (func->GetBody().empty()) {
                    errors.push_back({fmt::format("Function '{}' must have a body unless it is marked as 'extern'", identifier), "", func->GetLine(), func->GetColumn()});
                    return;
                }

                functions[identifier] = module.addFunction(symbol, qbeReturnType, args, func->IsVariadic(), flags);
            } else {
                functions[identifier] = module.defineFunction(symbol, qbeReturnType, args, func->IsVariadic());
            }
        }

        isGlobalScope = false;
        for (const auto& func : root->GetFunctions()) {
            Visit(func);
        }
    }

    void CodeGenerator::Visit(Ast::Function *function) {
        currentFunction = functions[function->GetName()];
        blockStack.push(currentFunction->entryPoint());

        EnterScope();

        // Add the parameters to the current scope as variables.
        for (const auto& param : function->GetParameters()) {
            auto paramName = param->GetName();
            auto qbeParamType = GetQbeType(param->GetType());
            Qbe::Local* local = new Qbe::Local{paramName, qbeParamType, true};
            auto allocatedLocal = getCurrentBlock()->addAllocate(GetSize(qbeParamType));
            getCurrentBlock()->addStore(Qbe::ValueReference(local), allocatedLocal);
            if (!AddVariable(paramName, Qbe::ValueReference(allocatedLocal))) {
                errors.push_back({fmt::format("Parameter '{}' in function '{}' is already defined as a variable in the current scope", paramName, function->GetName()), "", param->GetLine(), param->GetColumn()});
            }
        }

        for (const auto& stmt : function->GetBody()) {
            stmt->Accept(this);
        }

        if (function->GetReturnType()->GetLiteralType() == LiteralType::Void) {
            getCurrentBlock()->addReturn(); // add implicit return
        }

        ExitScope();
        blockStack.pop();
    }

    void CodeGenerator::Visit(Ast::VariableDeclaration *variableDeclaration) {
        auto identifier = variableDeclaration->GetName();
        auto type = variableDeclaration->GetType();
        auto initializer = variableDeclaration->GetInitializer();

        if (!type) {
            type = ResolveExpressionType(initializer);
        }

        if (!type) {
            errors.push_back({"Could not resolve type of variable '" + identifier + "'", "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
            return;
        }

        auto qbeType = GetQbeType(type);

        if (isGlobalScope) {
            // We only support global variables with literal initializers for now, so we can emit it directly
            std::string initialValue = "0"; // Default initial value for uninitialized variables
            if (variableDeclaration->GetInitializer()) {
                // For now, we only support literal initializers, so we can just use the literal value as the initial value in Qbe IR.
                auto literalExpr = dynamic_cast<Ast::LiteralExpression*>(variableDeclaration->GetInitializer());
                if (literalExpr) {
                    initialValue = literalExpr->ToString();
                } else {
                    errors.push_back({fmt::format("Only literal initializers are supported in code generation for now (variable '{}')", identifier), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                    return;
                }
            }

            auto literalType = type->GetLiteralType();
            if (literalType == LiteralType::F32) {
                if (!AddVariable(identifier, module.addGlobal(new Qbe::Literal((float)std::stod(initialValue))))) {
                    errors.push_back({fmt::format("Variable '{}' is already defined in the current scope", identifier), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                }
            }
            else if (literalType == LiteralType::F64) {
                if (!AddVariable(identifier, module.addGlobal(new Qbe::Literal(std::stod(initialValue))))) {
                    errors.push_back({fmt::format("Variable '{}' is already defined in the current scope", identifier), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                }
            }
            else if (literalType == LiteralType::I8 || literalType == LiteralType::U8 || literalType == LiteralType::I16 || literalType == LiteralType::U16 || literalType == LiteralType::I32 || literalType == LiteralType::U32) {
                if (!AddVariable(identifier, module.addGlobal(new Qbe::Literal((int32_t)std::stoll(initialValue))))) {
                    errors.push_back({fmt::format("Variable '{}' is already defined in the current scope", identifier), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                }
            }
            else if (literalType == LiteralType::I64 || literalType == LiteralType::U64) {
                if (!AddVariable(identifier, module.addGlobal(new Qbe::Literal((int64_t)std::stoll(initialValue))))) {
                    errors.push_back({fmt::format("Variable '{}' is already defined in the current scope", identifier), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                }
            }
            else {
                errors.push_back({fmt::format("Unsupported literal type for global variable '{}'", identifier), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
                return;
            }
        }
        else {
            auto local = getCurrentBlock()->addAllocate(GetSize(qbeType));

            if (!AddVariable(identifier, Qbe::ValueReference(local))) {
                errors.push_back({fmt::format("Variable '{}' is already defined in the current scope", identifier), "", variableDeclaration->GetLine(), variableDeclaration->GetColumn()});
            }

            if (initializer) {
                auto valueRef = EmitExpression(initializer);
                getCurrentBlock()->addStore(valueRef, local);
            }
        }
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
        this->EmitExpression(assignmentExpression);
    }

    void CodeGenerator::Visit(Ast::ReturnStatement *returnStatement) {
        auto value = returnStatement->GetValue();
        if (value) {
            auto valueRef = EmitExpression(value);
            getCurrentBlock()->addReturn(valueRef);
        } else {
            getCurrentBlock()->addReturn();
        }
    }

    void CodeGenerator::Visit(Ast::BinaryExpression *binaryExpression) {
        this->EmitExpression(binaryExpression);
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
        this->EmitExpression(functionCallExpression); // If we see it here, the result is discarded anyway.
    }

    void CodeGenerator::Visit(Ast::PostfixExpression *postfixExpression) {
        // We actually emulate a postfix expression as "operand = operand <op> 1;" in code generation, so we can just emit it as an assignment expression.
        static auto literal = new Ast::LiteralExpression(LiteralType::I32, (uint64_t)1, postfixExpression->GetLine(), postfixExpression->GetColumn());

        auto assignmentExpr = new Ast::AssignmentExpression(
                postfixExpression->GetOperand(),
                new Ast::BinaryExpression(
                        postfixExpression->GetOperand(),
                        postfixExpression->GetOperator() == Operator::Increment ? Operator::Add
                                                        : Operator::Subtract,
                        literal,
                        postfixExpression->GetLine(),
                        postfixExpression->GetColumn()
                ),
                postfixExpression->GetLine(),
                postfixExpression->GetColumn()
        );

        this->Visit(assignmentExpr);
    }

    void CodeGenerator::Visit(Ast::SubscriptExpression *subscriptExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::MemberAccessExpression *memberAccessExpression) {
        // TODO: Implement
    }

    void CodeGenerator::Visit(Ast::IdentifierExpression *identifierExpression) {
        throw std::runtime_error("IdentifierExpression should be handled in EmitExpression, not visited directly.");
    }

    void CodeGenerator::Visit(Ast::NestedExpression *nestedExpression) {
        // Just emit the inner expression directly, since nested expressions don't have any semantics on their own.
        this->EmitExpression(nestedExpression->GetInner());
    }

    void CodeGenerator::Visit(Ast::BreakStatement *breakStatement) {
        getCurrentBlock()->addUnconditionalJump(breakBlockStack.top());
    }

    void CodeGenerator::Visit(Ast::IfStatement *ifStatement) {
        auto main = ifStatement->GetMainBranch();
        auto elseIfBranches = ifStatement->GetElseIfBranches();
        auto elseBranch = ifStatement->GetElseBranch();

        auto mergeBlock = currentFunction->addBlock(GetLabel("merge"));

        auto conditionRef = EmitExpression(main.condition);
        auto thenBlock = currentFunction->addBlock(GetLabel("then"));
        auto elseBlock = currentFunction->addBlock(GetLabel("else"));

        getCurrentBlock()->addConditionalJump(thenBlock, conditionRef, elseBlock);
        blockStack.push(thenBlock);
        for (const auto& stmt : main.body) {
            stmt->Accept(this);
        }

        if (getCurrentBlock()->isTerminated())
            getCurrentBlock()->addUnconditionalJump(mergeBlock);
        blockStack.pop();

        blockStack.push(elseBlock);
        // Handle else if branches
        for (const auto& elseIf : elseIfBranches) {
            auto elseIfConditionRef = EmitExpression(elseIf.condition);
            auto elseIfThenBlock = currentFunction->addBlock(GetLabel("elseif_then"));
            auto elseIfElseBlock = currentFunction->addBlock(GetLabel("elseif_else"));

            getCurrentBlock()->addConditionalJump(elseIfThenBlock, elseIfConditionRef, elseIfElseBlock);
            blockStack.push(elseIfThenBlock);
            for (const auto& stmt : elseIf.body) {
                stmt->Accept(this);
            }

            getCurrentBlock()->addUnconditionalJump(mergeBlock);
            blockStack.pop();

            blockStack.push(elseIfElseBlock);
        }

        if (!elseBranch.empty()) {
            for (const auto& stmt : elseBranch) {
                stmt->Accept(this);
            }
        }else {
            // If there is no else branch, we need to add an unconditional jump to the merge block to ensure correct control flow.
            getCurrentBlock()->addUnconditionalJump(mergeBlock);
        }
        blockStack.pop();

        blockStack.push(mergeBlock);
    }

    void CodeGenerator::Visit(Ast::ExpressionStatement *expressionStatement) {
        this->EmitExpression(expressionStatement->GetExpression());
    }

    void CodeGenerator::Visit(Ast::WhileStatement *whileStatement) {
        auto conditionBlock = currentFunction->addBlock(GetLabel("while_condition"));
        auto bodyBlock = currentFunction->addBlock(GetLabel("while_body"));
        auto mergeBlock = currentFunction->addBlock(GetLabel("while_merge"));

        getCurrentBlock()->addUnconditionalJump(conditionBlock);

        blockStack.push(conditionBlock);
        auto conditionRef = EmitExpression(whileStatement->GetCondition());
        getCurrentBlock()->addConditionalJump(bodyBlock, conditionRef, mergeBlock);
        blockStack.pop();

        blockStack.push(bodyBlock);
        breakBlockStack.push(mergeBlock);
        continueBlockStack.push(conditionBlock);

        for (const auto& stmt : whileStatement->GetBody()) {
            stmt->Accept(this);
        }
        getCurrentBlock()->addUnconditionalJump(conditionBlock);

        blockStack.pop();
        breakBlockStack.pop();
        continueBlockStack.pop();

        blockStack.push(mergeBlock);
    }

    void CodeGenerator::Visit(Ast::ForStatement *forStatement) {
        auto initializer = forStatement->GetInitializer();
        auto condition = forStatement->GetCondition();
        auto increment = forStatement->GetIncrement();

        auto conditionBlock = currentFunction->addBlock(GetLabel("for_condition"));
        auto bodyBlock = currentFunction->addBlock(GetLabel("for_body"));
        auto incrementBlock = currentFunction->addBlock(GetLabel("for_increment"));
        auto mergeBlock = currentFunction->addBlock(GetLabel("for_merge"));

        // An initializer is guaranteed, so are all the other values.
        initializer->Accept(this);
        getCurrentBlock()->addUnconditionalJump(conditionBlock);

        blockStack.push(conditionBlock);
        auto conditionRef = EmitExpression(condition);
        getCurrentBlock()->addConditionalJump(bodyBlock, conditionRef, mergeBlock);
        blockStack.pop();

        blockStack.push(bodyBlock);
        breakBlockStack.push(mergeBlock);
        continueBlockStack.push(incrementBlock);
        for (const auto& stmt : forStatement->GetBody()) {
            stmt->Accept(this);
        }
        getCurrentBlock()->addUnconditionalJump(incrementBlock);
        blockStack.pop();
        breakBlockStack.pop();
        continueBlockStack.pop();

        blockStack.push(incrementBlock);
        increment->Accept(this);
        getCurrentBlock()->addUnconditionalJump(conditionBlock);
        blockStack.pop();

        blockStack.push(mergeBlock);
    }

    void CodeGenerator::Visit(Ast::ImportStatement *importStatement) {
    }

    void CodeGenerator::Visit(Ast::ContinueStatement *continueStatement) {
        getCurrentBlock()->addUnconditionalJump(continueBlockStack.top());
    }

    void CodeGenerator::Visit(Type *type) {
    }

    Qbe::ITypeDefinition * CodeGenerator::GetQbeType(Type *type) {
        static Qbe::Primitive int32Type(Qbe::TypeDefinitionKind::Int32);
        static Qbe::Primitive int64Type(Qbe::TypeDefinitionKind::Int64);
        static Qbe::Primitive float32Type(Qbe::TypeDefinitionKind::Float32);
        static Qbe::Primitive float64Type(Qbe::TypeDefinitionKind::Float64);
        static Qbe::Primitive pointerType(Qbe::TypeDefinitionKind::Pointer);

        auto literalType = type->GetLiteralType();
        if (literalType != LiteralType::UserDefined) {
            switch (literalType) {
                case LiteralType::I8:
                case LiteralType::U8:
                case LiteralType::I16:
                case LiteralType::U16:
                case LiteralType::I32:
                case LiteralType::U32:
                    return &int32Type;
                case LiteralType::I64:
                case LiteralType::U64:
                    return &int64Type;
                case LiteralType::F32:
                    return &float32Type;
                case LiteralType::F64:
                    return &float64Type;
                case LiteralType::Void:
                    return new Qbe::VoidType();
                case LiteralType::Pointer:
                    return &pointerType;
                default:
                    break;
            }
        }

        errors.push_back({fmt::format("Unsupported type '{}' in code generation", type->ToString()), "", -1, -1});
        return nullptr; // For now, we only support primitive types. User defined types will be implemented later.
    }

    Type * CodeGenerator::ResolveExpressionType(Ast::Expression *expression) {
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


    Qbe::ValueReference CodeGenerator::EmitExpression(Ast::Expression *expression) {
        Qbe::ValueReference *failed = new Qbe::ValueReference((Qbe::Literal*)nullptr);
        if (auto* lit = dynamic_cast<Ast::LiteralExpression*>(expression)) {
            switch (lit->GetLiteralType()) {
                case LiteralType::I32: case LiteralType::U32:
                case LiteralType::I8:  case LiteralType::U8:
                case LiteralType::I16: case LiteralType::U16:
                    return Qbe::CreateLiteral((int32_t)std::stoll(lit->ToString()));
                case LiteralType::I64: case LiteralType::U64:
                    return Qbe::CreateLiteral((int64_t)std::stoll(lit->ToString()));
                case LiteralType::F32:
                    return Qbe::CreateLiteral((float)std::stod(lit->ToString()));
                case LiteralType::F64:
                    return Qbe::CreateLiteral(std::stod(lit->ToString()));
                case LiteralType::Pointer:
                    return module.addGlobal(escapeStringLiteral(std::get<std::string>(lit->GetValue())));
                default:
                    errors.push_back({"Unsupported literal type", "", lit->GetLine(), lit->GetColumn()});
                    return *failed;
            }
        }

        if (auto* ident = dynamic_cast<Ast::IdentifierExpression*>(expression)) {
            auto slot = ResolveVariable(ident->ToString());
            if (!slot) {
                errors.push_back({"Undefined variable: " + ident->ToString(), "", ident->GetLine(), ident->GetColumn()});
                return *failed;
            }
            auto* sym = ResolveSymbol(ident->ToString());
            if (!sym) {
                errors.push_back({"Undefined symbol: " + ident->ToString(), "", ident->GetLine(), ident->GetColumn()});
                return *failed;
            }

            // If it is a global, we need to load it first before we can use it

            return getCurrentBlock()->addLoad(*slot, GetQbeType(sym->type));
        }

        auto assignment = [&](Ast::Expression* left, Ast::Expression* r) -> Qbe::ValueReference {
            auto* leftIdent = dynamic_cast<Ast::IdentifierExpression*>(left);
            if (!leftIdent) {
                errors.push_back({"Left-hand side of assignment must be an identifier for now (only simple variable assignment is supported)", "", left->GetLine(), left->GetColumn()});
                return *failed;
            }

            auto slot = ResolveVariable(leftIdent->ToString());
            if (!slot) {
                errors.push_back({"Undefined variable: " + leftIdent->ToString(), "", leftIdent->GetLine(), leftIdent->GetColumn()});
                return *failed;
            }

            auto right = EmitExpression(r);
            getCurrentBlock()->addStore(right, *slot);
            return right;
        };

        if (auto* assignmentExpr = dynamic_cast<Ast::AssignmentExpression*>(expression)) {
            return assignment(assignmentExpr->GetLeft(), assignmentExpr->GetRight());
        }

        if (auto* bin = dynamic_cast<Ast::BinaryExpression*>(expression)) {
            if (bin->GetOperator() == Operator::Assignment) {
                return assignment(bin->GetLeft(), bin->GetRight());
            }

            auto left = EmitExpression(bin->GetLeft());
            auto right = EmitExpression(bin->GetRight());

            switch (bin->GetOperator()) {
                case Operator::Add: return getCurrentBlock()->addAdd(left, right);
                case Operator::Subtract: return getCurrentBlock()->addSub(left, right);
                case Operator::Multiply: return getCurrentBlock()->addMul(left, right);
                case Operator::Divide: return getCurrentBlock()->addDiv(left, right);
                case Operator::LessThan:  return getCurrentBlock()->addEquality(left, right, Qbe::Instructions::EqualityOperation::LessThan, Qbe::Instructions::EqualityPrimitive::Signed);
                case Operator::LessThanOrEqual: return getCurrentBlock()->addEquality(left, right, Qbe::Instructions::EqualityOperation::LessThanOrEqual, Qbe::Instructions::EqualityPrimitive::Signed);
                case Operator::GreaterThan: return getCurrentBlock()->addEquality(left, right, Qbe::Instructions::EqualityOperation::GreaterThan, Qbe::Instructions::EqualityPrimitive::Signed);
                case Operator::GreaterThanOrEqual: return getCurrentBlock()->addEquality(left, right, Qbe::Instructions::EqualityOperation::GreaterThanOrEqual, Qbe::Instructions::EqualityPrimitive::Signed);
                case Operator::Equal: return getCurrentBlock()->addEquality(left, right, Qbe::Instructions::EqualityOperation::Equal, Qbe::Instructions::EqualityPrimitive::Signed);
                case Operator::NotEqual: return getCurrentBlock()->addEquality(left, right, Qbe::Instructions::EqualityOperation::NotEqual, Qbe::Instructions::EqualityPrimitive::Signed);
                case Operator::LogicalAnd: return getCurrentBlock()->addAnd(left, right);
                case Operator::LogicalOr: return getCurrentBlock()->addOr(left, right);
                case Operator::BitwiseAnd: return getCurrentBlock()->addBitwise(left, right, Qbe::Instructions::BitwiseOperation::And);
                case Operator::BitwiseOr: return getCurrentBlock()->addBitwise(left, right, Qbe::Instructions::BitwiseOperation::Or);
                case Operator::BitwiseXor: return getCurrentBlock()->addBitwise(left, right, Qbe::Instructions::BitwiseOperation::Xor);
                case Operator::ShiftLeft: return getCurrentBlock()->addShiftLeft(left, right);
                case Operator::ShiftRight: return getCurrentBlock()->addShiftRight(left, right);
                default:
                    errors.push_back({"Unsupported binary operator", "", bin->GetLine(), bin->GetColumn()});
                    return *failed;
            }
        }

        if (auto* nested = dynamic_cast<Ast::NestedExpression*>(expression))
            return EmitExpression(nested->GetInner());

        if (auto* cast = dynamic_cast<Ast::CastExpression*>(expression)) {
            auto operand = EmitExpression(cast->GetOperand());
            auto targetType = GetQbeType(cast->GetTargetType());
            if (!targetType) {
                errors.push_back({"Unknown target type in cast expression", "", cast->GetLine(), cast->GetColumn()});
                return *failed;
            }

            auto operandType = ResolveExpressionType(cast->GetOperand());
            if (!operandType) {
                errors.push_back({"Could not resolve type of operand in cast expression", "", cast->GetLine(), cast->GetColumn()});
                return *failed;
            }

            Qbe::Instructions::ConversionType conversionType;
            Qbe::Instructions::ConversionSign conversionSign;

            // If moving from 32 bit to 64 bit we must enable conversionType to Extend
            if ((operandType->GetLiteralType() == LiteralType::I32 || operandType->GetLiteralType() == LiteralType::U32) &&
                (cast->GetTargetType()->GetLiteralType() == LiteralType::I64 || cast->GetTargetType()->GetLiteralType() == LiteralType::U64)) {
                conversionType = Qbe::Instructions::ConversionType::Extend;
                }
            else {
                conversionType = Qbe::Instructions::ConversionType::Truncate;
            }

            // If moving from unsigned to signed, or from smaller to larger unsigned type, we must enable sign to Unsigned
            if ((operandType->GetLiteralType() == LiteralType::U8 || operandType->GetLiteralType() == LiteralType::U16 || operandType->GetLiteralType() == LiteralType::U32) &&
                (cast->GetTargetType()->GetLiteralType() == LiteralType::I8 || cast->GetTargetType()->GetLiteralType() == LiteralType::I16 || cast->GetTargetType()->GetLiteralType() == LiteralType::I32 ||
                 cast->GetTargetType()->GetLiteralType() == LiteralType::U64)) {
                conversionSign = Qbe::Instructions::ConversionSign::Unsigned;
                 }
            else {
                conversionSign = Qbe::Instructions::ConversionSign::Signed;
            }

            // If moving from float to int, we must enable conversionType to FloatToInt
            if ((operandType->GetLiteralType() == LiteralType::F32 || operandType->GetLiteralType() == LiteralType::F64) &&
                (cast->GetTargetType()->GetLiteralType() == LiteralType::I8 || cast->GetTargetType()->GetLiteralType() == LiteralType::I16 || cast->GetTargetType()->GetLiteralType() == LiteralType::I32 || cast->GetTargetType()->GetLiteralType() == LiteralType::I64)) {
                conversionType = Qbe::Instructions::ConversionType::FloatToInt;
            }
            else if ((operandType->GetLiteralType() == LiteralType::F32 || operandType->GetLiteralType() == LiteralType::F64) &&
                     (cast->GetTargetType()->GetLiteralType() == LiteralType::U8 || cast->GetTargetType()->GetLiteralType() == LiteralType::U16 || cast->GetTargetType()->GetLiteralType() == LiteralType::U32 || cast->GetTargetType()->GetLiteralType() == LiteralType::U64)) {
                conversionType = Qbe::Instructions::ConversionType::FloatToInt;
                conversionSign = Qbe::Instructions::ConversionSign::Unsigned;
            }
            else if ((operandType->GetLiteralType() == LiteralType::I8 || operandType->GetLiteralType() == LiteralType::I16 || operandType->GetLiteralType() == LiteralType::I32 || operandType->GetLiteralType() == LiteralType::I64) &&
                     (cast->GetTargetType()->GetLiteralType() == LiteralType::F32 || cast->GetTargetType()->GetLiteralType() == LiteralType::F64)) {
                conversionType = Qbe::Instructions::ConversionType::IntToFloat;
            }
            else if ((operandType->GetLiteralType() == LiteralType::U8 || operandType->GetLiteralType() == LiteralType::U16 || operandType->GetLiteralType() == LiteralType::U32 || operandType->GetLiteralType() == LiteralType::U64) &&
                     (cast->GetTargetType()->GetLiteralType() == LiteralType::F32 || cast->GetTargetType()->GetLiteralType() == LiteralType::F64)) {
                conversionType = Qbe::Instructions::ConversionType::IntToFloat;
                conversionSign = Qbe::Instructions::ConversionSign::Unsigned;
            }

            return getCurrentBlock()->addConversion(operand, (Qbe::Primitive*)targetType, conversionType, conversionSign);
        }

        if (auto* call = dynamic_cast<Ast::FunctionCallExpression*>(expression)) {
            auto* callee = dynamic_cast<Ast::IdentifierExpression*>(call->GetCallee());
            std::vector<Qbe::ValueReference> args;
            for (const auto& arg : call->GetArguments())
                args.push_back(EmitExpression(arg));
            return getCurrentBlock()->addCall(functions[callee->ToString()], args);
        }

        if (auto* addrOf = dynamic_cast<Ast::AddressOfExpression*>(expression)) {
            auto *ident = dynamic_cast<Ast::IdentifierExpression*>(addrOf->GetOperand());
            if (!ident) {
                errors.push_back({"Address-of operator can only be applied to identifiers for now", "", addrOf->GetLine(), addrOf->GetColumn()});
                return *failed;
            }

            auto slot = ResolveVariable(ident->ToString());
            if (!slot) {
                errors.push_back({"Undefined variable: " + ident->ToString(), "", ident->GetLine(), ident->GetColumn()});
                return *failed;
            }

            return *slot;
        }

        if (auto* deref = dynamic_cast<Ast::DereferenceExpression*>(expression)) {
            auto operand = EmitExpression(deref->GetOperand());
            return getCurrentBlock()->addLoad(operand, GetQbeType(new Type(LiteralType::Pointer)));
        }

        if (auto* unary = dynamic_cast<Ast::UnaryExpression*>(expression)) {
            auto op = unary->GetOperator();
            auto operand = EmitExpression(unary->GetOperand());

            // Create a temporary variable to hold the result of the unary operation, since Qbe IR requires all operations to have a destination.
            auto temp = getCurrentBlock()->addCopy(operand);
            if (op == Operator::Subtract) {
                return getCurrentBlock()->addSub(Qbe::CreateLiteral(0), operand);
            }
        }

        if (auto* sizeofExpr = dynamic_cast<Ast::SizeOfExpression*>(expression)) {
            auto type = sizeofExpr->GetType();
            auto qbeType = GetQbeType(type);
            if (!qbeType) {
                errors.push_back({"Unsupported type in sizeof expression", "", sizeofExpr->GetLine(), sizeofExpr->GetColumn()});
                return *failed;
            }

            return getCurrentBlock()->addCopy(GetSize(qbeType));
        }

        errors.push_back({"Unsupported expression in codegen", "", expression->GetLine(), expression->GetColumn()});
        return *failed;
    }
} // Oxy