using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.Reflection;
using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;
using QbeGenerator;
using QbeGenerator.Instructions;

namespace Oxylang.Systems.Transformers;

public record CodeGeneratorResult(string Code, bool Success);

public class CodeGenerator(ILogger _logger, SourceFile _sourceFile, bool _is64Bit, List<ResolvedReliance> _resolvedReliances, CompilationUnit _thisUnit) : ITransformer<CodeGeneratorResult>
{
    private QbeModule _module;
    public QbeModule Module => _module;

    private Dictionary<string, CompilationUnit> _imports = new ();
    private Dictionary<string, (QbeAggregateType qbe, StructType type)> _structs = new ();
    private Dictionary<string, (QbeFunction qbe, FunctionType type)> _functions = new ();
    private Dictionary<string, QbeValue> _strings = new ();

    private Stack<Dictionary<string, (QbeValue qbe, TypeNode type)>> _scopes = new ();
    private Stack<QbeBlock> _continueBlocks = new ();
    private Stack<QbeBlock> _breakBlocks = new ();
    private Stack<QbeBlock> _blocks = new ();
    private QbeBlock? _allocateBlock; // all allocations are done in this block.
    private QbeFunction? _currentFunction;

    private bool TryFindStruct(List<string> parts, out (QbeAggregateType qbe, StructType type) result)
    {
        return TryFindStructInUnit(_thisUnit, parts, out result);
    }
    
    private bool TryFindStructInUnit(CompilationUnit unit, List<string> parts, out (QbeAggregateType qbe, StructType type) result)
    {
        if (parts.Count == 1)
        {
            if (unit == _thisUnit)
            {
                if (_structs.TryGetValue(parts[0], out var structInfo))
                {
                    result = structInfo;
                    return true;
                }
            }
            
            var structType =
                unit.Exports.FirstOrDefault(x => x.Identifier == parts[0] && x.Type is StructType)?.Type as StructType;

            if (structType != null)
            {
                result = (CreateQbeAggregate(structType, unit), structType);
                return true;
            }
            
            result = default;
            return false;
        }

        if (unit.Reliances.All(x => x.Alias != parts[0]))
        {
            result = default;
            return false;
        }
        
        var reliance = unit.Reliances.First(x => x.Alias == parts[0]);
        var relianceUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == reliance.Identifier)?.Unit;
        if (relianceUnit == null)
        {
            result = default;
            return false;
        }

        return TryFindStructInUnit(relianceUnit, parts.Skip(1).ToList(), out result);
    }

    private QbeAggregateType CreateQbeAggregate(StructType structType, CompilationUnit unit)
    {
        var aggregate =
            new QbeAggregateType(unit.MangleName(structType.Name, structType));

        foreach (var field in structType.Fields)
        {
            var fieldType = GetQbeType(field.Type!, true);
            if (fieldType == null)
            {
                _logger.LogError("Unsupported field type: " + field.Type!.GetString(0), _sourceFile,
                    field.Type!.Location);
                continue;
            }

            aggregate.Add(fieldType);
        }

        if (!_structs.ContainsKey(aggregate.Identifier))
        {
            _structs[aggregate.Identifier] = (aggregate, structType);
            _module.AddType(aggregate);
        }
        
        return aggregate;
    }
    
    private bool TryFindFunction(List<string> parts, out (QbeFunction qbe, FunctionType type) result, out bool isFunctionPointer)
    {
        isFunctionPointer = false;
        foreach (var scope in _scopes)
        {
            if (scope.TryGetValue(parts[0], out var func) && func.type is FunctionType functionType && func.qbe is QbeLocalRef or QbeGlobalRef)
            {
                var identifier = func.qbe is QbeLocalRef localRef ? localRef.Identifier : ((QbeGlobalRef)func.qbe).Identifier;
                
                QbeValue loaded;
                if (func.qbe is QbeGlobalRef)
                {
                    loaded = _blocks.Peek().Load(QbePrimitive.Pointer(), func.qbe);
                    identifier = ((QbeLocalRef)loaded).Identifier;
                }
                else
                {
                    loaded = _blocks.Peek().Load(QbePrimitive.Pointer(), func.qbe);
                    identifier = ((QbeLocalRef)loaded).Identifier;
                }
                isFunctionPointer = true;
                
                result = (new QbeFunction(identifier, QbeFunctionFlags.None,
                    functionType.ReturnType != null ? GetQbeType(functionType.ReturnType) : null,
                    functionType.IsVariadic,
                    functionType.ParameterTypes.Select((t, i) =>
                        new QbeArgument(GetQbeType(t)!, $"arg_{i}")).ToArray()), functionType);

                return true;
            }
        }
        
        return TryFindFunctionInUnit(_thisUnit, parts, out result);
    }

    private bool TryFindFunctionInUnit(CompilationUnit unit, List<string> parts,
        out (QbeFunction qbe, FunctionType type) result)
    {
        if (parts.Count == 1)
        {
            if (unit == _thisUnit)
            {
                if (_functions.TryGetValue(parts[0], out var func))
                {
                    result = func;
                    return true;
                }
            }
            
            var functionExport =
                unit.Exports.FirstOrDefault(x => x.Identifier == parts[0] && x.Type is FunctionType);

            var functionType = functionExport?.Type as FunctionType;
            var functionNode = functionExport?.DefinitionNode as FunctionDefinition;
            
            if (functionType != null)
            {
                result = (new QbeFunction(unit.MangleName(parts[0], functionNode), QbeFunctionFlags.None,
                    functionType.ReturnType != null ? GetQbeType(functionType.ReturnType) : null,
                    functionType.IsVariadic,
                    functionType.ParameterTypes.Select((t, i) =>
                        new QbeArgument(GetQbeType(t)!, $"arg_{i}")).ToArray()), functionType);
                return true;
            }

            result = default;
            return false;
        }

        if (unit.Reliances.All(x => x.Alias != parts[0]))
        {
            result = default;
            return false;
        }

        var reliance = unit.Reliances.First(x => x.Alias == parts[0]);
        var relianceUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == reliance.Identifier)?.Unit;
        if (relianceUnit == null)
        {
            result = default;
            return false;
        }

        return TryFindFunctionInUnit(relianceUnit, parts.Skip(1).ToList(), out result);
    }

    public CodeGeneratorResult Transform(Root node)
    {
        _module = new QbeModule(!_is64Bit);
        Visit(node);
        if (_logger.HasErrors())
        {
            return new CodeGeneratorResult(string.Empty, false);
        }
        
        return new CodeGeneratorResult(_module.Emit(), true);
    }

    public void Visit(Root node)
    {
        _scopes.Push(new());
        AddLibcFunctions();
        
        foreach (var import in node.Imports)
        {
            Visit(import);
        }

        foreach (var structDef in node.Structs)
        {
            _structs[structDef.Name] = (_module.AddType(_thisUnit.MangleName(structDef.Name, node)), structDef.StructType);
        }
        
        foreach (var structDef in node.Structs)
        {
            Visit(structDef);
        }

        foreach (var globalVar in node.GlobalVariables)
        {
            Visit(globalVar);
        }

        foreach (var function in node.Functions)
        {
            DefineFunction(function);
        }
        
        foreach (var function in node.Functions)
        {
            Visit(function);
        }
    }

    private void AddLibcFunctions()
    {
        _functions["printf"] = (new QbeFunction("printf", QbeFunctionFlags.None, QbePrimitive.Int32(false), true,
                [new QbeArgument(QbePrimitive.Pointer(), "format")]),
            new FunctionType([new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8))],
                new PrimaryType(PrimaryType.PrimaryTypeKind.I32),
                true, true, false));

        _functions["memcpy"] = (new QbeFunction("memcpy", QbeFunctionFlags.None, QbePrimitive.Pointer(), false,
            [
                new QbeArgument(QbePrimitive.Pointer(), "dest"),
                new QbeArgument(QbePrimitive.Pointer(), "src"),
                new QbeArgument(QbePrimitive.Int64(false), "n")
            ]),
            new FunctionType(
                [
                    new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8)),
                    new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8)),
                    new PrimaryType(PrimaryType.PrimaryTypeKind.U64)
                ],
                new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8)),
                false, true, false));

        _functions["memset"] = (new QbeFunction("memset", QbeFunctionFlags.None, QbePrimitive.Pointer(), false,
            [
                new QbeArgument(QbePrimitive.Pointer(), "dest"),
                new QbeArgument(QbePrimitive.Int32(false), "value"),
                new QbeArgument(QbePrimitive.Int64(false), "n")
            ]),
            new FunctionType(
                [
                    new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8)),
                    new PrimaryType(PrimaryType.PrimaryTypeKind.U8),
                    new PrimaryType(PrimaryType.PrimaryTypeKind.U64)
                ],
                new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8)),
                false, true, false));

        _functions["malloc"] = (new QbeFunction("malloc", QbeFunctionFlags.None, QbePrimitive.Pointer(), false,
                [new QbeArgument(QbePrimitive.Int64(false), "size")]),
            new FunctionType(
                [new PrimaryType(PrimaryType.PrimaryTypeKind.U64)],
                new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8)),
                false, true, false));

        _functions["free"] = (new QbeFunction("free", QbeFunctionFlags.None, null, false,
                [new QbeArgument(QbePrimitive.Pointer(), "ptr")]),
            new FunctionType(
                [new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8))],
                new VoidType(),
                false, true, false));
    }

    public void Visit(AlignOfExpression node)
    {
        
    }

    public void Visit(AssignmentExpression node)
    {
        var value = VisitExpression(node.Right);
        var left = VisitExpression(node.Left, true);
        if (value == null || left == null)
        {
            _logger.LogError("Invalid assignment expression.", _sourceFile, node.Location);
            return;
        }
        
        if (left.PrimitiveEnum is QbePrimitive)
        {
            _blocks.Peek().Store(left, value);
        }
        else
        {
            // For non-primitive types, we need to do a memcpy since they're passed by reference.
            MemCopy(left, value, value.PrimitiveEnum!.ByteSize(!_is64Bit));
        }
    }

    public void Visit(BinaryExpression node)
    {
        
    }

    public void Visit(BlockStatement node)
    {
        _scopes.Push(new());
        foreach (var statement in node.Statements)
        {
            statement.Accept(this);
        }
        _scopes.Pop();
    }

    public void Visit(CastExpression node)
    {
        
    }

    public void Visit(FunctionCallExpression node)
    {
        if (!TryFindFunction(node.Identifier, out var function, out var isFunctionPointer))
        {
            _logger.LogError("Function not found: " + string.Join(".", node.Identifier), _sourceFile, node.Location);
            return;
        }

        bool isExtern = !_module.HasFunction(function.qbe.Identifier);
        
        _blocks.Peek().Call(function.qbe.Identifier, isFunctionPointer, function.qbe.ReturnType, function.qbe.Arguments.Count,
            GetFunctionCallArguments(node.Arguments, isExtern));
    }

    public void Visit(FunctionDefinition node)
    {
        
    }

    private void DefineFunction(FunctionDefinition function)
    {
        var returnType = GetQbeType(function.ReturnType);
        if (returnType == null && function.ReturnType is not VoidType)
        {
            _logger.LogError("Unsupported return type: " + function.ReturnType.GetString(0), _sourceFile, function.ReturnType.Location);
            return;
        }
        
        var parameterTypes = new List<IQbeTypeDefinition>();
        foreach (var param in function.Parameters)
        {
            var paramType = GetQbeType(param.Type);
            if (paramType == null)
            {
                _logger.LogError("Unsupported parameter type: " + param.Type.GetString(0), _sourceFile, param.Type.Location);
                return;
            }
            parameterTypes.Add(paramType);
        }
        
        string symbol = _thisUnit.MangleName(function.Name, function);
        if (function.Attributes.Any(x => x.Name == "symbol"))
        {
            symbol = function.Attributes.First(x => x.Name == "symbol").Arguments[0];
        }
        
        QbeFunctionFlags flags = QbeFunctionFlags.None;
        if (function.Attributes.Any(x => x.Name == "export" || x.Name == "public"))
        {
            flags = QbeFunctionFlags.Export;
        }

        if (function.Attributes.Any(x => x.Name == "entry"))
        {
            flags = QbeFunctionFlags.Export;
            symbol = "main";
        }

        int count = 0;
        bool isExtern = function.Attributes.Any(x => x.Name == "extern");
        if (isExtern)
        {
            // this is a external function, linked later.
            // We use new here, since that means it won't be added to the module, and thus won't be emitted, which is what we want for extern functions.
            _functions[function.Name] = (new QbeFunction(symbol, QbeFunctionFlags.None, returnType, function.IsVariadic,
                parameterTypes.Select(x => new QbeArgument(x, $"arg_{count++}")).ToArray()), function.FunctionType);

            return; // we don't need to generate code for this function, since it's external.
        }
        else
        {
            _functions[function.Name] = (_module.AddFunction(symbol, flags, returnType,
                function.IsVariadic, parameterTypes.Select(x => new QbeArgument(x, $"arg_{count++}")).ToArray()), function.FunctionType);
        }

        _scopes.Push(new());

        _currentFunction = _functions[function.Name].qbe;
        _allocateBlock = _currentFunction.BuildEntryBlock();
        var afterAllocBlock = _currentFunction.BuildBlock("after_alloc");
        _blocks.Push(afterAllocBlock);
        
        for (int i = 0; i < function.Parameters.Count; i++)
        {
            var param = function.Parameters[i];
            var qbeParam = _functions[function.Name].qbe.Arguments[i];

            if (param.Type is not StructType && param.Type is not NamedType && param.Type is not PointerType)
            {
                _scopes.Peek()[param.Name] = (_allocateBlock.Allocate(qbeParam.Primitive, !_is64Bit), param.Type);
                _blocks.Peek().Store(_scopes.Peek()[param.Name].qbe,
                    new QbeLocalRef(qbeParam.Primitive, qbeParam.Identifier));
            }
            else if (param.Type is PointerType)
            {
                // For pointer types, we can pass them directly since they're already references.
                _scopes.Peek()[param.Name] = (new QbeLocalRef(qbeParam.Primitive, qbeParam.Identifier), param.Type);
            }
            else
            {
                // For struct types we do not need to load them, since qbe already loads them 
                _scopes.Peek()[param.Name] = (new QbeLocalRef(qbeParam.Primitive, qbeParam.Identifier), param.Type);
            }
        }
        
        if (function.Body == null)
        {
            _logger.LogError("Function body cannot be null.", _sourceFile, function.Location);
            return;
        }
        
        function.Body.Accept(this);
        
        if (!_blocks.Peek().HasTerminator())
        {
            _blocks.Peek().Return();
        }
        
        _allocateBlock.Jump(afterAllocBlock); // Connect all allocations to the main function body.
        
        _scopes.Pop();
        _currentFunction = null;
        _allocateBlock = null;
    }

    public void Visit(ImportStatement node)
    {
        if (_imports.ContainsKey(node.Path))
        {
            _logger.LogError("Module already imported: " + node.Path, _sourceFile, node.Location);
            return;
        }
        
        var unit = _resolvedReliances.FirstOrDefault(r => r.Identifier == node.Path)?.Unit;
        if (unit == null)
        {
            _logger.LogError("Failed to find compilation unit for import: " + node.Path, _sourceFile, node.Location);
            return;
        }
        
        _imports[node.Alias] = unit;
    }

    public void Visit(LiteralExpression node)
    {
        
    }

    public void Visit(PostfixExpression node)
    {
        // Convert to i = i + 1 or i = i - 1 depending on the operator, then visit the resulting assignment expression.
        var one = new QbeLiteral(QbePrimitive.Int32(false), 1);
        var leftValue = VisitExpression(node.Primary, true);
        if (leftValue == null)
        {
            _logger.LogError("Invalid postfix expression.", _sourceFile, node.Location);
            return;
        }
        
        var binaryOperator = node.Operator switch
        {
            Language.Operator.Increment => Language.Operator.Plus,  
            Language.Operator.Decrement => Language.Operator.Minus,
            _ => throw new InvalidOperationException("Invalid postfix operator")
        };

        var binaryExpr = new BinaryExpression(node.Location, node.Primary, binaryOperator,
            new LiteralExpression(node.Location, 1ul, GetNodeType(node.Primary, out _)!));
        
        var assignmentExpr = new AssignmentExpression(node.Location, (LeftValue)node.Primary, binaryExpr);
        Visit(assignmentExpr);
    }

    public void Visit(ReturnStatement node)
    {
        var value = node.Expression != null ? VisitExpression(node.Expression) : null;
        _blocks.Peek().Return(value);
    }

    public void Visit(SizeOfExpression node)
    {
        
    }

    public void Visit(StringExpression node)
    {
        
    }

    public void Visit(StructDefinition node)
    {
        var structType = _structs[node.Name];
        foreach (var field in node.Fields)
        {
            var fieldType = GetQbeType(field.Type!, true);
            if (fieldType == null)
            {
                _logger.LogError("Unsupported field type: " + field.Type!.GetString(0), _sourceFile, field.Type!.Location);
                continue;
            }
            structType.qbe.Add(fieldType);
        }
    }

    public void Visit(StructInitializerExpression node)
    {
        
    }

    public void Visit(VariableDeclaration node)
    {
        var varType = node.Type != null ? GetQbeType(node.Type) : null;
        if (varType == null && node.Initializer == null)
        {
            _logger.LogError("Variable type cannot be inferred without an initializer.", _sourceFile, node.Location);
            return;
        }

        QbeValue? inferredValue = null;
        if (varType == null && _currentFunction != null)
        {
            inferredValue = VisitExpression(node.Initializer!);
            varType = inferredValue?.PrimitiveEnum;
        }
        else
        {
            if (_currentFunction == null && varType == null)
            {
                _logger.LogError("Global variable must have an explicit type or an initializer.", _sourceFile, node.Location);
                return;
            }
        }

        if (_currentFunction == null)
        {
            if (node.Initializer != null && node.Initializer is not LiteralExpression)
            {
                _logger.LogError("Global variable initializers must be literals.", _sourceFile, node.Initializer.Location);
                return;
            }

            QbeGlobalRef global = null; 
            if (node.Initializer != null)
            {
                var literal = (LiteralExpression)node.Initializer!;

                if (literal.Type is PrimaryType primaryType)
                {
                    if (primaryType.Kind == PrimaryType.PrimaryTypeKind.F32 || primaryType.Kind == PrimaryType.PrimaryTypeKind.F64)
                        global = _module.AddGlobal(varType!, (double)literal.FloatValue);
                    else 
                        global = _module.AddGlobal(varType!, (long)literal.IntegerValue);
                }
                else if (literal.Type is PointerType pointerType)
                {
                    global = _module.AddGlobal(varType!, literal.IntegerValue);
                }
                else
                {
                    _logger.LogError("Unsupported global variable initializer type: " + literal.Type.GetString(0),
                        _sourceFile, literal.Type.Location);
                    return;
                }
            }
            else
            {
                global = _module.AddGlobal(varType!, 0);
            }
            
            _scopes.Peek()[node.Name] = (global, node.Type ?? GetNodeType(node.Initializer!, out _)!);
        }
        else
        {
            var value = inferredValue ?? (node.Initializer != null ? VisitExpression(node.Initializer) : null);

            if (value is QbeGlobalRef globalRef)
            {
                // For global variables, we can just use the global reference directly, since it's already a pointer.
                _scopes.Peek()[node.Name] = (globalRef, node.Type ?? GetNodeType(node.Initializer!, out _)!);
            }
            else
            {
                var local = _allocateBlock.Allocate(varType, !_is64Bit);
                _scopes.Peek()[node.Name] = (local, (node.Type ?? GetNodeType(node.Initializer!, out _))!);

                // Store the initializer value if it exists, otherwise call memcpy
                if (value != null)
                {
                    if (value.PrimitiveEnum is QbePrimitive)
                    {
                        _blocks.Peek().Store(local, value);
                    }
                    else
                    {
                        MemCopy(local, value, varType.ByteSize(!_is64Bit));
                    }
                }
            }
        }
    }

    private void MemCopy(QbeValue local, QbeValue value, long byteSize)
    {
        var memcpyFunc = _functions["memcpy"];
        local.PrimitiveEnum = QbePrimitive.Pointer();
        value.PrimitiveEnum = QbePrimitive.Pointer();

        _blocks.Peek().Call(memcpyFunc.qbe.Identifier, false, null, 3, local, value,
            new QbeLiteral(QbePrimitive.Int64(false), byteSize));
    }

    public void Visit(VariableExpression node)
    {
        
    }

    public void Visit(AddressOfExpression node)
    {
        
    }

    public void Visit(DereferenceExpression node)
    {
        
    }

    public void Visit(MemberAccessExpression node)
    {
        
    }

    public void Visit(UnaryExpression node)
    {
        
    }

    public void Visit(MethodCallExpression node)
    {
        var method = GetNodeType(node.Method, out var isExternal);
        // method is a function pointer, we need to load it and call it.
        if (method == null || method is not FunctionType functionType)
        {
            _logger.LogError("Unable to determine type of method call.", _sourceFile,
                node.Method.Location);
            return;
        }
        
        var args = GetFunctionCallArguments(node.Arguments, functionType.IsExternal);
        if (args.Any(x => x == null))
        {
            _logger.LogError("Invalid argument in method call.", _sourceFile, node.Location);
            return;
        }
        
        QbeLocalRef called;
        
        if (isExternal)
        {
            var unit = GetUnitFromType(method);
            var export = unit.Exports.FirstOrDefault(e => e.Type == method);
            if (export == null)
            {
                _logger.LogError("Unable to find method in exports of module.", _sourceFile,
                    node.Method.Location);
                return;
            }

            called = _blocks.Peek().Call(unit.MangleName(export.Identifier, export.DefinitionNode), false,
                functionType.ReturnType != null ? GetQbeType(functionType.ReturnType) : null,
                functionType.ParameterTypes.Count, args!);
        }
        else
        {
            var funcPtr = VisitExpression(node.Method) as QbeLocalRef;
            if (funcPtr == null)
            {
                _logger.LogError("Unable to load function pointer for method call.", _sourceFile,
                    node.Method.Location);
                return;
            }
            
            called = _blocks.Peek().Call(funcPtr.Identifier, true,
                functionType.ReturnType != null ? GetQbeType(functionType.ReturnType) : null,
                functionType.ParameterTypes.Count, args!);
        }
        
        if (called != null && functionType.ReturnType is PrimaryType)
        {
            called.PrimitiveEnum = GetQbeType(functionType.ReturnType, true) as QbePrimitive;
            if (EnsureTypeSize(functionType.ReturnType, ref called)) return;
        }
    }

    public void Visit(IfStatement node)
    {
        var afterBlock = _currentFunction!.BuildBlock("if_after");
        var mainBlock = _currentFunction.BuildBlock("if_then");

        // Region: build the chain of false-branch targets
        QbeBlock firstFalseBlock = BuildFalseBlock(node, 0, afterBlock);

        var condition = VisitExpression(node.MainBranch.Condition);
        _blocks.Peek().JumpIfNotZero(condition, mainBlock, firstFalseBlock);

        _blocks.Push(mainBlock);
        node.MainBranch.Body.Accept(this);
        if (!_blocks.Peek().HasTerminator())
            _blocks.Peek().Jump(afterBlock);

        var currentFalseBlock = firstFalseBlock;
        for (int i = 0; i < node.ElseIfBranches.Count; i++)
        {
            var branch = node.ElseIfBranches[i];
            var bodyBlock = _currentFunction.BuildBlock("else_if_then");
            QbeBlock nextFalseBlock = BuildFalseBlock(node, i + 1, afterBlock);

            _blocks.Push(currentFalseBlock);
            var elseIfCondition = VisitExpression(branch.Condition);
            _blocks.Peek().JumpIfNotZero(elseIfCondition, bodyBlock, nextFalseBlock);

            _blocks.Push(bodyBlock);
            branch.Body.Accept(this);
            if (!_blocks.Peek().HasTerminator())
                _blocks.Peek().Jump(afterBlock);

            currentFalseBlock = nextFalseBlock;
        }

        if (node.ElseBranch != null)
        {
            _blocks.Push(currentFalseBlock);
            node.ElseBranch.Accept(this);
            if (!_blocks.Peek().HasTerminator())
                _blocks.Peek().Jump(afterBlock);
        }

        _blocks.Push(afterBlock);
    }

    private QbeBlock BuildFalseBlock(IfStatement node, int index, QbeBlock afterBlock)
    {
        if (index < node.ElseIfBranches.Count)
            return _currentFunction!.BuildBlock("else_if_check");
        if (node.ElseBranch != null)
            return _currentFunction!.BuildBlock("if_else");
        return afterBlock;
    }

    public void Visit(WhileStatement node)
    {
        var conditionBlock = _currentFunction!.BuildBlock("while_condition");
        var bodyBlock = _currentFunction.BuildBlock("while_body");
        var afterBlock = _currentFunction.BuildBlock("while_after");

        _blocks.Peek().Jump(conditionBlock);

        _blocks.Push(conditionBlock);
        var condition = VisitExpression(node.Condition);
        _blocks.Peek().JumpIfNotZero(condition, bodyBlock, afterBlock);
        
        _blocks.Push(bodyBlock);
        _continueBlocks.Push(conditionBlock);
        _breakBlocks.Push(afterBlock);
        node.Body.Accept(this);
        _breakBlocks.Pop();
        _continueBlocks.Pop();

        if (!_blocks.Peek().HasTerminator())
            _blocks.Peek().Jump(conditionBlock);

        _blocks.Pop();
        _blocks.Push(afterBlock);
    }

    public void Visit(ForStatement node)
    {
        if (node.Initializer != null)
        {
            node.Initializer.Accept(this);
        }
        
        var conditionBlock = _currentFunction!.BuildBlock("for_condition");
        var bodyBlock = _currentFunction.BuildBlock("for_body");
        var incrementBlock = _currentFunction.BuildBlock("for_increment");
        var afterBlock = _currentFunction.BuildBlock("for_after");
        
        _blocks.Peek().Jump(conditionBlock);
        _blocks.Push(conditionBlock);
        
        var condition = node.Condition != null ? VisitExpression(node.Condition) : new QbeLiteral(QbePrimitive.Int32(false), 1);
        _blocks.Peek().JumpIfNotZero(condition, bodyBlock, afterBlock);
        
        _blocks.Push(bodyBlock);
        _continueBlocks.Push(incrementBlock);
        _breakBlocks.Push(afterBlock);
        node.Body.Accept(this);
        _breakBlocks.Pop();
        _continueBlocks.Pop();
        
        if (!_blocks.Peek().HasTerminator())
            _blocks.Peek().Jump(incrementBlock);
        
        _blocks.Push(incrementBlock);
        if (node.Increment != null)
        {
            node.Increment.Accept(this);
        }
        
        if (!_blocks.Peek().HasTerminator())
            _blocks.Peek().Jump(conditionBlock);
        
        _blocks.Pop();

        _blocks.Push(afterBlock);
    }

    public void Visit(BreakStatement node)
    {
        if (_breakBlocks.Count == 0)
        {
            _logger.LogError("Break statement not within a loop.", _sourceFile, node.Location);
            return;
        }
        
        _blocks.Peek().Jump(_breakBlocks.Peek());
    }

    public void Visit(ContinueStatement node)
    {
        if (_continueBlocks.Count == 0)
        {
            _logger.LogError("Continue statement not within a loop.", _sourceFile, node.Location);
            return;
        }
        
        _blocks.Peek().Jump(_continueBlocks.Peek());
    }

    private IQbeTypeDefinition? GetQbeType(TypeNode? typeNode, bool isAggregateType = false)
    {
        if (typeNode is PrimaryType primaryType)
        {
            return primaryType.Kind switch
            {
                // for some ungodly reason, qbe only supports i8, u8, i16 and u16 as part of aggregate types, so we have to use i32 and u32 for them when they're not part of an aggregate type.
                PrimaryType.PrimaryTypeKind.I8 => isAggregateType ? QbePrimitive.Byte(true) : QbePrimitive.Int32(true),
                PrimaryType.PrimaryTypeKind.U8 => isAggregateType ? QbePrimitive.Byte(false) : QbePrimitive.Int32(false),
                PrimaryType.PrimaryTypeKind.I16 => isAggregateType ? QbePrimitive.Int16(true) : QbePrimitive.Int32(true),
                PrimaryType.PrimaryTypeKind.U16 => isAggregateType ? QbePrimitive.Int16(false) : QbePrimitive.Int32(false),
                PrimaryType.PrimaryTypeKind.I32 => QbePrimitive.Int32(true),
                PrimaryType.PrimaryTypeKind.U32 => QbePrimitive.Int32(false),
                PrimaryType.PrimaryTypeKind.I64 => QbePrimitive.Int64(true),
                PrimaryType.PrimaryTypeKind.U64 => QbePrimitive.Int64(false),
                PrimaryType.PrimaryTypeKind.F32 => QbePrimitive.Float(),
                PrimaryType.PrimaryTypeKind.F64 => QbePrimitive.Double(),
                _ => null
            };
        }

        if (typeNode is PointerType)
        {
            return new QbePrimitive(QbePrimitiveEnum.Pointer, false);
        }
        
        if (typeNode is NamedType namedType)
        {
            if (!TryFindStruct(namedType.Parts, out var structType))
            {
                _logger.LogError("Struct type not found: " + string.Join("::", namedType.Parts), _sourceFile, typeNode.Location);
                return null;
            }
            
            return structType.qbe;
        }
        
        if (typeNode is StructType structTypeNode)
        {
            if (_structs.TryGetValue(structTypeNode.Name, out var structType))
            {
                return structType.qbe;
            }
            
            // Fuck it just generate it on the fly lol
            return CreateQbeAggregate(structTypeNode, GetUnitFromType(structTypeNode));
        }
        
        if (typeNode is VoidType)
        {
            return null;
        }

        if (typeNode is FunctionType)
        {
            // function types are pointers
            return QbePrimitive.Pointer();
        }
        
        _logger.LogError("Unsupported type: " + typeNode?.GetString(0), _sourceFile, typeNode?.Location ?? new SourceLocation(0, 0));
        return null;
    }
    
    private TypeNode? GetNodeType(Node node, out bool isExternal) // isExternal is true if it does not come from this module
    {
        isExternal = false;
        if (node is LiteralExpression literalExpr)
        {
            return literalExpr.Type;
        }
        
        if (node is StringExpression stringExpr)
        {
            return new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8));
        }
        
        if (node is VariableExpression variableExpr)
        {
            if (variableExpr.Name.Count > 1)
            {
                throw new NotImplementedException("im lazyyyyy");
            }
            
            foreach (var scope in _scopes.Reverse())
            {
                if (scope.TryGetValue(variableExpr.Name[0], out var value))
                {
                    return value.type;
                }
            }
            
            if (_functions.TryGetValue(variableExpr.Name[0], out var function))
            {
                return function.type;
            }
            
            if (_structs.TryGetValue(variableExpr.Name[0], out var structType))
            {
                return structType.type;
            }
            
            if (_imports.TryGetValue(variableExpr.Name[0], out var import))
            {
                return new ModuleType(import);
            }
        }
        
        if (node is MemberAccessExpression memberAccessExpr)
        {
            var externTargetType = false;
            var targetType = GetNodeType(memberAccessExpr.Object, out externTargetType);
            if (targetType is ModuleType moduleType)
            {
                var unit = moduleType.Unit;

                var subReliance = unit.Reliances.FirstOrDefault(r => r.Alias == memberAccessExpr.MemberName);
                if (subReliance != null)
                {
                    var subUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == subReliance.Identifier);
                    if (subUnit != null)
                        return new ModuleType(subUnit.Unit);
                }

                var export = unit.Exports.FirstOrDefault(e => e.Identifier == memberAccessExpr.MemberName);
                isExternal = unit != _thisUnit;
                return ResolveExportedType(unit, export!);
            }
            
            if (targetType is PointerType pointerType)
            {
                targetType = pointerType.BaseType;
            }
            
            if (targetType is NamedType namedType)
            {
                if (!TryFindStruct(namedType.Parts, out var structType))
                {
                    _logger.LogError("Struct type not found: " + string.Join("::", namedType.Parts), _sourceFile, memberAccessExpr.Location);
                    return null;
                }

                var field = structType.type.Fields.FirstOrDefault(x => x.Name == memberAccessExpr.MemberName);
                if (field != null)
                {
                    return field.Type;
                }
            }
            else if (targetType is StructType structType)
            {
                var field = structType.Fields.FirstOrDefault(x => x.Name == memberAccessExpr.MemberName);
                if (field != null)
                {
                    return field.Type;
                }
            }
        }

        if (node is StructInitializerExpression structInitExpr)
        {
            if (!TryFindStruct(structInitExpr.StructName, out var structType))
            {
                _logger.LogError("Struct type not found: " + string.Join("::", structInitExpr.StructName), _sourceFile, structInitExpr.Location);
                return null;
            }
            
            return structType.type;
        }

        if (node is DereferenceExpression dereferenceExpr)
        {
            var targetType = GetNodeType(dereferenceExpr.Expression, out isExternal);
            if (targetType is PointerType pointerType)
            {
                return pointerType.BaseType;
            }
        }
        
        if (node is AddressOfExpression addressOfExpr)
        {
            var targetType = GetNodeType(addressOfExpr.Expression, out isExternal);
            if (targetType != null)
            {
                return new PointerType(targetType);
            }
        }
        
        if (node is FunctionCallExpression functionCallExpr)
        {
            if (!TryFindFunction(functionCallExpr.Identifier, out var function, out var isFunctionPointer))
            {
                _logger.LogError("Function not found: " + string.Join(".", functionCallExpr.Identifier), _sourceFile, functionCallExpr.Location);
                return null;
            }
            
            return function.type.ReturnType;
        }
        
        if (node is BinaryExpression binaryExpr)
        {
            var leftType = GetNodeType(binaryExpr.Left, out _);
            return leftType;
        }
        
        if (node is CastExpression castExpr)
        {
            return GetNodeType(castExpr.TargetType, out isExternal);
        }
        
        if (node is TypeNode typeNode)
        {
            return typeNode;
        }

        if (node is MethodCallExpression methodCallExpr)
        {
            var objectType = GetNodeType(methodCallExpr.Method, out _);
            if (objectType == null)
            {
                _logger.LogError("Unable to determine type of method call object.", _sourceFile,
                    methodCallExpr.Method.Location);
                return null;
            }
            
            if (objectType is not FunctionType functionType)
            {
                _logger.LogError("Method call target is not a function.", _sourceFile, methodCallExpr.Method.Location);
                return null;
            }

            return functionType.ReturnType;
        }
        
        _logger.LogError("Unable to determine type of node: " + node.GetString(0), _sourceFile, node.Location);
        return null;
    }

    private TypeNode? ResolveExportedType(CompilationUnit unit, Export export)
    {
        var subCodeGenerator = new CodeGenerator(_logger, _sourceFile, _is64Bit, _resolvedReliances, unit);
        return subCodeGenerator.ResolveExportedSymbol(export.Identifier)?.Type;
    }
    
    private Node? ResolveExportedNode(CompilationUnit unit, Export export)
    {
        var subCodeGenerator = new CodeGenerator(_logger, _sourceFile, _is64Bit, _resolvedReliances, unit);
        return subCodeGenerator.ResolveExportedSymbol(export.Identifier)?.DefinitionNode;
    }

    private Export? ResolveExportedSymbol(string exportIdentifier)
    {
        return _thisUnit.Exports.FirstOrDefault(e => e.Identifier == exportIdentifier);
    }

    private QbeValue VisitExpression(Expression expression, bool isLValue = false)
    {
        if (expression is LiteralExpression literal)
        {
            if (literal.Type is PrimaryType primaryType)
            {
                var qbeType = GetQbeType(literal.Type) as QbePrimitive;
                if (qbeType == null)
                {
                    _logger.LogError("Unsupported literal type: " + literal.Type.GetString(0), _sourceFile, literal.Type.Location);
                    return null;
                }

                var literalNode = null as QbeLiteral;
                
                if (qbeType.PrimitiveEnum.IsInteger())
                {
                    literalNode = new QbeLiteral(qbeType, (long)literal.IntegerValue);
                }
                
                if (qbeType.PrimitiveEnum.IsFloat())
                {
                    literalNode = new QbeLiteral(qbeType, literal.FloatValue);
                }
                
                if (literalNode == null)
                {
                    _logger.LogError("Unsupported literal type: " + literal.Type.GetString(0), _sourceFile, literal.Type.Location);
                    return null;
                }
                
                var copiedNode = _blocks.Peek().Copy(literalNode);
                copiedNode.PrimitiveEnum = GetQbeType(literal.Type, true) as QbePrimitive ?? qbeType;
                if (EnsureTypeSize(literal.Type, ref copiedNode)) return null;
                return copiedNode;
            }
            else if (literal.Type is PointerType pointerType)
            {
                var qbeType = GetQbeType(literal.Type) as QbePrimitive;
                if (qbeType == null)
                {
                    _logger.LogError("Unsupported literal type: " + literal.Type.GetString(0), _sourceFile, literal.Type.Location);
                    return null;
                }

                var literalNode = new QbeLiteral(qbeType, (long)literal.IntegerValue);
                var copiedNode = _blocks.Peek().Copy(literalNode);
                copiedNode.PrimitiveEnum = GetQbeType(literal.Type, true) as QbePrimitive ?? qbeType;
                if (EnsureTypeSize(literal.Type, ref copiedNode)) return null;
                return copiedNode;
            }
            else
            {
                _logger.LogError("Unsupported literal type: " + literal.Type.GetString(0), _sourceFile,
                    literal.Type.Location);
                return null;
            }
        }

        if (expression is StringExpression stringExpr)
        {
            if (_strings.ContainsKey(stringExpr.Value))
                return _strings[stringExpr.Value];

            var global = _module.AddGlobal(stringExpr.Value);
            _strings[stringExpr.Value] = global;
            return global;
        }

        if (expression is UnaryExpression unaryExpr)
        {
            var operand = VisitExpression(unaryExpr.Operand);
            if (operand == null)
            {
                _logger.LogError("Invalid unary expression.", _sourceFile, unaryExpr.Location);
                return null;
            }

            var result = unaryExpr.Operator switch
            {
                Language.Operator.Plus => operand,
                Language.Operator.Minus => _blocks.Peek().Sub(new QbeLiteral(operand.PrimitiveEnum, 0), operand),
                Language.Operator.BitwiseNot => _blocks.Peek().Not(operand),
                Language.Operator.Not => CreateNot(operand),
                _ => null
            };
            
            if (result == null)
            {
                _logger.LogError("Unsupported unary operator: " + unaryExpr.Operator, _sourceFile, unaryExpr.Location);
                return null;
            }
            
            return result;
        }
        
        if (expression is VariableExpression variableExpr)
        {
            if (variableExpr.Name.Count > 1)
            {
                throw new NotImplementedException("im lazyyyyy2");
            }
            
            foreach (var scope in _scopes.Reverse())
            {
                if (scope.TryGetValue(variableExpr.Name[0], out var value))
                {                    
                    bool isStruct = value.type is NamedType or StructType;

                    if (value.qbe is QbeGlobalRef globalRef)
                    {
                        if (isLValue || isStruct || _strings.ContainsValue(globalRef))
                            return globalRef with { PrimitiveEnum = QbePrimitive.Pointer() };
                        
                        var loadedGlobal = _blocks.Peek().Load(GetQbeType(value.type), globalRef);
                        if (EnsureTypeSize(value.type, ref loadedGlobal)) return null;
                        return loadedGlobal;
                    }

                    isStruct = isStruct || value.type is PointerType;
                    
                    if (isLValue || isStruct)
                        return value.qbe with { PrimitiveEnum = QbePrimitive.Pointer() };
                    
                    var loaded = _blocks.Peek().Load(value.qbe.PrimitiveEnum, value.qbe);
                    if (EnsureTypeSize(value.type, ref loaded)) return null;
                    
                    return loaded;
                }
            }
            
            if (_imports.TryGetValue(variableExpr.Name[0], out var unit))
            {
                _logger.LogError("Tried using module name as variable: " + string.Join("::", variableExpr.Name), _sourceFile, variableExpr.Location);
                return null;
            }
            
            if (TryFindFunction(variableExpr.Name, out var function, out var isFunctionPointer))
            {
                var copy = _blocks.Peek().Copy(new QbeGlobalRef(function.qbe.Identifier));
                return copy;
            }

            _logger.LogError("Undefined variable: " + string.Join("::", variableExpr.Name), _sourceFile, variableExpr.Location);
            return null;
        }

        if (expression is BinaryExpression binaryExpr)
        {
            var left = VisitExpression(binaryExpr.Left);
            var right = VisitExpression(binaryExpr.Right);
            
            if (left == null || right == null)
            {
                _logger.LogError("Invalid binary expression.", _sourceFile, binaryExpr.Location);
                return null;
            }
            
            var result = binaryExpr.Operator switch
            {
                Language.Operator.Plus => _blocks.Peek().Add(left, right),
                Language.Operator.Minus => _blocks.Peek().Sub(left, right),
                Language.Operator.Asterisk => _blocks.Peek().Mul(left, right),
                Language.Operator.Slash => _blocks.Peek().Div(left, right),
                Language.Operator.Percent => _blocks.Peek().Rem(left, right),
                Language.Operator.DoubleEquals => _blocks.Peek().Equality(EqualityType.Equal, left, right),
                Language.Operator.NotEquals => _blocks.Peek().Equality(EqualityType.Inequal, left, right),
                Language.Operator.GreaterThan => _blocks.Peek().Equality(EqualityType.GreaterThan, left, right),
                Language.Operator.LessThan => _blocks.Peek().Equality(EqualityType.LessThan, left, right),
                Language.Operator.GreaterThanOrEqual => _blocks.Peek().Equality(EqualityType.GreaterThanOrEqual, left, right),
                Language.Operator.LessThanOrEqual => _blocks.Peek().Equality(EqualityType.LessThanOrEqual, left, right),
                Language.Operator.BitwiseOr => _blocks.Peek().Or(left, right),
                Language.Operator.BitwiseAnd => _blocks.Peek().And(left, right),
                Language.Operator.Or => CreateOr(left, right),
                Language.Operator.And => CreateAnd(left, right),
                _ => null
            };
            
            if (result == null)
            {
                _logger.LogError("Unsupported binary operator: " + binaryExpr.Operator, _sourceFile, binaryExpr.Location);
                return null;
            }
            
            return result;
        }

        if (expression is StructInitializerExpression structInitExpr)
        {
            if (!TryFindStruct(structInitExpr.StructName, out var structType))
            {
                _logger.LogError("Struct type not found: " + string.Join("::", structInitExpr.StructName), _sourceFile, structInitExpr.Location);
                return null;
            }
            
            var local = _allocateBlock.Allocate(structType.qbe, !_is64Bit);

            foreach (var fieldInit in structInitExpr.FieldInitializers)
            {
                var fieldInfo = structType.type.Fields.FirstOrDefault(x => x.Name == fieldInit.Item1);
                if (fieldInfo == null)
                {
                    _logger.LogError("Undefined field in struct initializer: " + fieldInit.Item1, _sourceFile,
                        fieldInit.Item2.Location);
                    return null;
                }

                var fieldValue = VisitExpression(fieldInit.Item2);
                if (fieldValue == null)
                {
                    _logger.LogError("Invalid struct initializer for field: " + fieldInit.Item1, _sourceFile,
                        fieldInit.Item2.Location);
                    return null;
                }
                fieldValue.PrimitiveEnum = GetQbeType(fieldInfo.Type, true) as QbePrimitive ?? fieldValue.PrimitiveEnum;
                
                var fieldIndex = structType.type.Fields.Index().First(x => x.Item.Name == fieldInit.Item1).Item1;
                var fieldPtr = _blocks.Peek().GetFieldPtr(local, structType.qbe, fieldIndex, !_is64Bit);
                if (fieldInfo.Type is PrimaryType || fieldInfo.Type is PointerType || fieldInfo.Type is FunctionType)
                    _blocks.Peek().Store(fieldPtr, fieldValue);
                else
                    MemCopy(fieldPtr, fieldValue, fieldValue.PrimitiveEnum.ByteSize(!_is64Bit));
            }

            return local;
        }

        if (expression is MemberAccessExpression memberAccessExpr)
        {
            var target = VisitExpression(memberAccessExpr.Object, true);
            if (target == null)
            {
                _logger.LogError("Invalid member access target.", _sourceFile, memberAccessExpr.Object.Location);
                return null;
            }

            var nodeType = GetNodeType(memberAccessExpr.Object, out var externTarget);
            
            if (memberAccessExpr.IsPointerAccess)
            {
                target = _blocks.Peek().Load(QbePrimitive.Pointer(), target);
            }
            else if (nodeType is PointerType pointerType)
            {
                _logger.LogError("Cannot access member of a pointer type without using '->' operator.", _sourceFile, memberAccessExpr.Location);
                return null;
            }

            (QbeAggregateType qbe, StructType type) resolvedStruct = nodeType switch
            {
                StructType st => (CreateQbeAggregate(st, GetUnitFromType(st)), st),
                NamedType nt when TryFindStruct(nt.Parts, out var s) => s,
                PointerType { BaseType: NamedType nt2 } when TryFindStruct(nt2.Parts, out var s2) => s2,
                PointerType { BaseType: StructType st2 } => (CreateQbeAggregate(st2, GetUnitFromType(st2)), st2),
                _ => default
            };

            if (resolvedStruct == default)
            {
                _logger.LogError("Member access on non-struct type.", _sourceFile, memberAccessExpr.Location);
                return null;
            }

            var (fieldIndex, field) = resolvedStruct!.type.Fields
                .Index()
                .FirstOrDefault(x => x.Item.Name == memberAccessExpr.MemberName);

            if (field == null)
            {
                _logger.LogError("Undefined field: " + memberAccessExpr.MemberName, _sourceFile,
                    memberAccessExpr.Location);
                return null;
            }

            var qbeStructType = resolvedStruct.qbe;
            var fieldPtr = _blocks.Peek().GetFieldPtr(target, qbeStructType, fieldIndex, !_is64Bit);

            if (isLValue || field.Type is NamedType or StructType)
                return fieldPtr;

            var fieldQbeType = GetQbeType(field.Type, true);
            if (fieldQbeType == null)
            {
                _logger.LogError("Unsupported field type in member access: " + field.Type?.GetString(0), _sourceFile,
                    memberAccessExpr.Location);
                return null;
            }

            var loaded = _blocks.Peek().Load(fieldQbeType, fieldPtr);
            if (fieldQbeType is QbePrimitive)
                if (EnsureTypeSize(field.Type, ref loaded)) return null;
            
            return loaded;
        }

        if (expression is DereferenceExpression dereferenceExpr)
        {
            // dereference can be an lvalue.
            var target = VisitExpression(dereferenceExpr.Expression, true);
            if (target == null)
            {
                _logger.LogError("Invalid dereference target.", _sourceFile, dereferenceExpr.Expression.Location);
                return null;
            }

            if (isLValue)
            {
                // Load the pointer
                var loadedPtr = _blocks.Peek().Load(QbePrimitive.Pointer(), target);
                return loadedPtr;
            }

            var targetType = GetNodeType(dereferenceExpr.Expression, out _);
            if (targetType is PointerType pointerType)
            {
                var pointeeType = GetQbeType(pointerType.BaseType);
                if (pointeeType == null)
                {
                    _logger.LogError("Unsupported pointer base type: " + pointerType.BaseType.GetString(0), _sourceFile,
                        pointerType.BaseType.Location);
                    return null;
                }
                
                var loaded = _blocks.Peek().Load(pointeeType, target);
                if (EnsureTypeSize(pointerType.BaseType, ref loaded)) return null;
                return loaded;
            }
        }
        
        if (expression is AddressOfExpression addressOfExpr)
        {
            var target = VisitExpression(addressOfExpr.Expression, true);
            if (target == null)
            {
                _logger.LogError("Invalid address-of target.", _sourceFile, addressOfExpr.Expression.Location);
                return null;
            }
            
            var pointer = _allocateBlock.Allocate(QbePrimitive.Pointer(), !_is64Bit);
            _blocks.Peek().Store(pointer, target);
            return pointer;
        }

        if (expression is FunctionCallExpression functionCallExpr)
        {
            if (!TryFindFunction(functionCallExpr.Identifier, out var function, out var isFunctionPointer))
            {
                _logger.LogError("Function not found: " + string.Join(".", functionCallExpr.Identifier), _sourceFile, functionCallExpr.Location);
                return null;
            }

            var args = GetFunctionCallArguments(functionCallExpr.Arguments, !_module.HasFunction(function.qbe.Identifier));
            if (args.Any(x => x == null))
            {
                _logger.LogError("Invalid argument in function call.", _sourceFile, functionCallExpr.Location);
                return null;
            }

            var called = _blocks.Peek().Call(function.qbe.Identifier, isFunctionPointer, function.qbe.ReturnType, function.qbe.Arguments.Count, args!);
            if (called != null)
            {
                if (function.type.ReturnType is PrimaryType)
                {
                    called.PrimitiveEnum = GetQbeType(function.type.ReturnType, true) as QbePrimitive;
                    if (EnsureTypeSize(function.type.ReturnType, ref called)) return null;
                }
            }

            return called;
        }

        if (expression is CastExpression castExpr)
        {
            var value = VisitExpression(castExpr.Expression);
            if (value == null)
            {
                _logger.LogError("Invalid cast expression.", _sourceFile, castExpr.Expression.Location);
                return null;
            }

            var targetType = GetQbeType(castExpr.TargetType) as QbePrimitive;
            if (targetType == null)
            {
                _logger.LogError("Unsupported cast target type: " + castExpr.TargetType.GetString(0), _sourceFile,
                    castExpr.TargetType.Location);
                return null;
            }

            return _blocks.Peek().Convert(value, targetType); // Convert from the value's type to the target type.
        }
        
        if (expression is MethodCallExpression methodCallExpr)
        {
            // A method is attached to an import, not an object.
            var method = GetNodeType(methodCallExpr.Method, out var isExtern);
            if (method == null || method is not FunctionType functionType)
            {
                _logger.LogError("Unable to determine type of method call.", _sourceFile,
                    methodCallExpr.Method.Location);
                return null;
            }

            QbeLocalRef called;

            if (isExtern)
            {
                var unit = GetUnitFromType(method);
                var export = unit.Exports.FirstOrDefault(e => e.Type == method);
                if (export == null)
                {
                    _logger.LogError("Unable to find method in exports of module.", _sourceFile,
                        methodCallExpr.Method.Location);
                    return null;
                }

                var args = GetFunctionCallArguments(methodCallExpr.Arguments,
                    !_module.HasFunction(unit.MangleName(export.Identifier, export.DefinitionNode)));
                if (args.Any(x => x == null))
                {
                    _logger.LogError("Invalid argument in method call.", _sourceFile, methodCallExpr.Location);
                    return null;
                }

                called = _blocks.Peek().Call(unit.MangleName(export.Identifier, export.DefinitionNode),
                    false,
                    functionType.ReturnType != null ? GetQbeType(functionType.ReturnType) : null,
                    functionType.ParameterTypes.Count, args!);
            }
            else
            {
                var args = GetFunctionCallArguments(methodCallExpr.Arguments, false);
                if (args.Any(x => x == null))
                {   
                    _logger.LogError("Invalid argument in method call.", _sourceFile, methodCallExpr.Location);
                    return null;
                }

                var methodPtr = VisitExpression(methodCallExpr.Method) as QbeLocalRef;
                if (methodPtr == null)
                {
                    _logger.LogError("Invalid method pointer in method call.", _sourceFile,
                        methodCallExpr.Method.Location);
                    return null;
                }

                called = _blocks.Peek().Call(methodPtr.Identifier, true,
                    functionType.ReturnType != null ? GetQbeType(functionType.ReturnType) : null,
                    functionType.ParameterTypes.Count, args!);
            }

            if (called != null && functionType.ReturnType is PrimaryType)
            {
                called.PrimitiveEnum = GetQbeType(functionType.ReturnType, true) as QbePrimitive;
                if (EnsureTypeSize(functionType.ReturnType, ref called)) return null;
            }

            return called;
        }

        _logger.LogError("Unsupported expression type: " + expression.GetString(0), _sourceFile, expression.Location);
        return null;
    }

    private CompilationUnit? FindUnitWithType(TypeNode typeToFind, CompilationUnit currentUnit)
    {
        if (currentUnit == _thisUnit)
        {
            if (_structs.Values.Any(s => s.type == typeToFind))
            {
                return currentUnit;
            }
            
            if (_functions.Values.Any(f => f.type == typeToFind))
            {
                return currentUnit;
            }
        }

        foreach (var export in currentUnit.Exports)
        {
            if (export.Type == typeToFind)
            {
                return currentUnit;
            }
            
            if (export.Type is StructType exSt && typeToFind is StructType st)
            {
                if (exSt.Name == st.Name && exSt.Fields.Count == st.Fields.Count &&
                    exSt.Fields.Zip(st.Fields, (f1, f2) => f1.Name == f2.Name && f1.Type.GetString(0) == f2.Type.GetString(0)).All(x => x))
                {
                    return currentUnit;
                }
            }
        }
        
        foreach (var reliance in currentUnit.Reliances)
        {
            var subUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == reliance.Identifier)?.Unit;
            if (subUnit == null) continue;

            var found = FindUnitWithType(typeToFind, subUnit);
            if (found != null) return found;
        }
        
        return null;
    }
    
    private CompilationUnit GetUnitFromType(TypeNode typeToFind)
    {
        return FindUnitWithType(typeToFind, _thisUnit) ?? throw new Exception("Unable to find unit for type: " + typeToFind.GetString(0));
    }

    private QbeValue CreateNot(QbeValue operand)
    {
        // operand == 0
        return _blocks.Peek().Equality(EqualityType.Equal, operand, new QbeLiteral(QbePrimitive.Int32(false), 0));
    }

    private QbeValue CreateAnd(QbeValue left, QbeValue right)
    {
        // (left != 0) & (right != 0)
        var leftCond = _blocks.Peek().Equality(EqualityType.Inequal, left, new QbeLiteral(QbePrimitive.Int32(false), 0));
        var rightCond = _blocks.Peek().Equality(EqualityType.Inequal, right, new QbeLiteral(QbePrimitive.Int32(false), 0));
        return _blocks.Peek().And(leftCond, rightCond);
    }

    private QbeValue CreateOr(QbeValue left, QbeValue right)
    {
        // (left != 0) | (right != 0)
        var leftCond = _blocks.Peek().Equality(EqualityType.Inequal, left, new QbeLiteral(QbePrimitive.Int32(false), 0));
        var rightCond = _blocks.Peek().Equality(EqualityType.Inequal, right, new QbeLiteral(QbePrimitive.Int32(false), 0));
        return _blocks.Peek().Or(leftCond, rightCond);
    }

    private QbeValue[] GetFunctionCallArguments(List<Expression> arguments, bool isExtern)
    {
        var args = new QbeValue[arguments.Count];
        for (int i = 0; i < arguments.Count; i++)
        {
            // Check if the type is a struct or not, if it is we do not need to load it
            var argValue = VisitExpression(arguments[i]);
            if (argValue == null)
            {
                _logger.LogError("Invalid argument expression.", _sourceFile, arguments[i].Location);
                return args;
            }
            var argType = GetNodeType(arguments[i], out _);

            if (argType is NamedType or StructType)
            {
                var structQbeType = GetQbeType(argType) as QbeAggregateType;
                if (structQbeType == null)
                {
                    _logger.LogError("Unsupported struct type in extern function argument: " + argType.GetString(0),
                        _sourceFile, argType.Location);
                    return args;
                }

                argValue.PrimitiveEnum = structQbeType;
            }


            args[i] = argValue;
        }

        return args;
    }

    private bool EnsureTypeSize(TypeNode type, ref QbeLocalRef loaded)
    {
        loaded = EnsureTypeSize(type, loaded) as QbeLocalRef;
        return loaded == null;
    }
    
    private bool EnsureTypeSize(TypeNode type, ref QbeValue value)
    {
        var ensured = EnsureTypeSize(type, value);
        if (ensured == null) return true;
        value = ensured;
        return false;
    }

    private QbeValue? EnsureTypeSize(TypeNode type, QbeValue value)
    {
        if (type is PrimaryType primaryType)
        {
            var kind = primaryType.Kind;
            if (kind == PrimaryType.PrimaryTypeKind.I8 || kind == PrimaryType.PrimaryTypeKind.U8 ||
                kind == PrimaryType.PrimaryTypeKind.I16 || kind == PrimaryType.PrimaryTypeKind.U16)
            {
                // Qbe does not support these values natively, so instead we have to extend them to i32/u32 and then truncate them back when necessary.
                var qbeType = GetQbeType(primaryType) as QbePrimitive;
                if (qbeType == null)
                {
                    _logger.LogError("Unsupported type: " + type.GetString(0), _sourceFile,
                        type.Location);
                    return null;
                }

                return _blocks.Peek().Convert(value, qbeType) as QbeLocalRef;
            }
        }

        return value;
    }
}