using System.Drawing;
using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;
using QbeGenerator;

namespace Oxylang.Systems.Transformers;

public record CodeGeneratorResult(string Code, bool Success);

public class CodeGenerator(ILogger _logger, SourceFile _sourceFile, bool _is64Bit) : ITransformer<CodeGeneratorResult>
{
    private QbeModule _module;

    private Dictionary<string, (QbeType qbe, StructType type)> _structs = new ();
    private Dictionary<string, (QbeFunction qbe, FunctionType type)> _functions = new ();
    private Dictionary<string, QbeValue> _strings = new ();

    private Stack<Dictionary<string, (QbeValue qbe, TypeNode type)>> _scopes = new ();
    private Stack<QbeBlock> _continueBlocks = new ();
    private Stack<QbeBlock> _breakBlocks = new ();
    private Stack<QbeBlock> _blocks = new ();

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
        AddLibcFunctions();
        
        foreach (var import in node.Imports)
        {
            Visit(import);
        }

        foreach (var structDef in node.Structs)
        {
            _structs[structDef.Name] = (_module.AddType(MangleName(structDef.Name, node)), structDef.StructType);
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
            new FunctionType(new(0, 0),
                [new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8))],
                new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.I32),
                true));

        _functions["memcpy"] = (new QbeFunction("memcpy", QbeFunctionFlags.None, QbePrimitive.Pointer(), false,
            [
                new QbeArgument(QbePrimitive.Pointer(), "dest"),
                new QbeArgument(QbePrimitive.Pointer(), "src"),
                new QbeArgument(QbePrimitive.Int64(false), "n")
            ]),
            new FunctionType(new(0, 0),
                [
                    new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8)),
                    new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8)),
                    new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U64)
                ],
                new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8)),
                false));

        _functions["memset"] = (new QbeFunction("memset", QbeFunctionFlags.None, QbePrimitive.Pointer(), false,
            [
                new QbeArgument(QbePrimitive.Pointer(), "dest"),
                new QbeArgument(QbePrimitive.Int32(false), "value"),
                new QbeArgument(QbePrimitive.Int64(false), "n")
            ]),
            new FunctionType(new(0, 0),
                [
                    new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8)),
                    new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8),
                    new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U64)
                ],
                new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8)),
                false));

        _functions["malloc"] = (new QbeFunction("malloc", QbeFunctionFlags.None, QbePrimitive.Pointer(), false,
                [new QbeArgument(QbePrimitive.Int64(false), "size")]),
            new FunctionType(new(0, 0),
                [new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U64)],
                new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8)),
                false));

        _functions["free"] = (new QbeFunction("free", QbeFunctionFlags.None, null, false,
                [new QbeArgument(QbePrimitive.Pointer(), "ptr")]),
            new FunctionType(new(0, 0),
                [new PointerType(new(0, 0), new PrimaryType(new(0, 0), PrimaryType.PrimaryTypeKind.U8))],
                new VoidType(new(0, 0)),
                false));
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
        
        if (left.PrimitiveEnum is QbePrimitive and not { PrimitiveEnum: QbePrimitiveEnum.Pointer })
        {
            _blocks.Peek().Store(left, value);
        }
        else
        {
            if (value.PrimitiveEnum is QbePrimitive and not { PrimitiveEnum: QbePrimitiveEnum.Pointer })
            {
                var allocated = _blocks.Peek().Allocate(value.PrimitiveEnum, !_is64Bit);
                _blocks.Peek().Store(allocated, value);
                value = allocated;
                value.PrimitiveEnum = QbePrimitive.Pointer();
            }
            
            MemCopy(left, value, value.PrimitiveEnum.ByteSize(!_is64Bit));
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
        var identifier = node.Identifier;
        if (!_functions.TryGetValue(identifier, out var function))
        {
            _logger.LogError("Undefined function: " + identifier, _sourceFile, node.Location);
            return;
        }
        
        _blocks.Peek().Call(function.qbe.Identifier, function.qbe.ReturnType, function.qbe.Arguments.Count, node.Arguments.Select(x => VisitExpression(x)).ToArray());
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
        
        string symbol = MangleName(function.Name, function);
        if (function.Attributes.Any(x => x.Name == "symbol"))
        {
            symbol = function.Attributes.First(x => x.Name == "symbol").Arguments[0];
        }
        
        QbeFunctionFlags flags = QbeFunctionFlags.None;
        if (function.Attributes.Any(x => x.Name == "export"))
        {
            flags |= QbeFunctionFlags.Export;
        }

        if (function.Attributes.Any(x => x.Name == "entry"))
        {
            flags = QbeFunctionFlags.Export;
            symbol = "main";
            if (_module.HasMainFunction())
            {
                _logger.LogError("Multiple entry points defined. Only one function can be marked with the 'entry' attribute.", _sourceFile, function.Location);
            }
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

        var block = _functions[function.Name].qbe.BuildEntryBlock();
        _blocks.Push(block);
        
        for (int i = 0; i < function.Parameters.Count; i++)
        {
            var param = function.Parameters[i];
            var qbeParam = _functions[function.Name].qbe.Arguments[i];

            if (param.Type is not PrimaryType primaryType)
            {
                _scopes.Peek()[param.Name] = (new QbeLocalRef(qbeParam.Primitive, qbeParam.Identifier), param.Type!);
            }
            else
            {
                var copy = block.Copy(new QbeLocalRef(qbeParam.Primitive, qbeParam.Identifier));
                _scopes.Peek()[param.Name] = (copy, param.Type!);
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
        
        _scopes.Pop();
    }

    public void Visit(ImportStatement node)
    {
        
    }

    public void Visit(LiteralExpression node)
    {
        
    }

    public void Visit(PostfixExpression node)
    {
        
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
        if (varType == null)
        {
            inferredValue = VisitExpression(node.Initializer!);
            varType = inferredValue?.PrimitiveEnum;
        }
        var value = inferredValue ?? (node.Initializer != null ? VisitExpression(node.Initializer) : null);
        var local = _blocks.Peek().Allocate(varType, !_is64Bit);
        _scopes.Peek()[node.Name] = (local, (node.Type ?? GetNodeType(node.Initializer!))!);

        // Store the initializer value if it exists, otherwise call memcpy
        if (value != null)
        {
            if (node.Type is PrimaryType)
            {
                _blocks.Peek().Store(local, value);
            }
            else
            {
                MemCopy(local, value, varType.ByteSize(!_is64Bit));
            }
        }
    }

    private void MemCopy(QbeValue local, QbeValue value, long byteSize)
    {
        var memcpyFunc = _functions["memcpy"];
        _blocks.Peek().Call(memcpyFunc.qbe.Identifier, null, 3, local, value,
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

    public void Visit(IfStatement node)
    {
        
    }

    public void Visit(WhileStatement node)
    {
        
    }

    public void Visit(ForStatement node)
    {
        
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
            if (_structs.TryGetValue(namedType.Name, out var structType))
            {
                return structType.qbe;
            }
        }
        
        if (typeNode is StructType structTypeNode)
        {
            if (_structs.TryGetValue(structTypeNode.Name, out var structType))
            {
                return structType.qbe;
            }
        }
        
        if (typeNode is VoidType)
        {
            return null;
        }
        
        _logger.LogError("Unsupported type: " + typeNode?.GetString(0), _sourceFile, typeNode?.Location ?? new SourceLocation(0, 0));
        return null;
    }
    
    private TypeNode? GetNodeType(Node node)
    {
        if (node is VariableExpression variableExpr)
        {
            foreach (var scope in _scopes.Reverse())
            {
                if (scope.TryGetValue(variableExpr.Name, out var value))
                {
                    return value.type;
                }
            }
        }
        
        if (node is MemberAccessExpression memberAccessExpr)
        {
            var targetType = GetNodeType(memberAccessExpr.Object);
            if (targetType is PointerType pointerType)
            {
                targetType = pointerType.BaseType;
            }
            
            if (targetType is NamedType namedType)
            {
                if (_structs.TryGetValue(namedType.Name, out var structType))
                {
                    var field = structType.type.Fields.FirstOrDefault(x => x.Name == memberAccessExpr.MemberName);
                    if (field != null)
                    {
                        return field.Type;
                    }
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
            if (_structs.TryGetValue(structInitExpr.StructName, out var structType))
            {
                return structType.type;
            }
        }

        if (node is DereferenceExpression dereferenceExpr)
        {
            var targetType = GetNodeType(dereferenceExpr.Expression);
            if (targetType is PointerType pointerType)
            {
                return pointerType.BaseType;
            }
        }
        
        if (node is FunctionCallExpression functionCallExpr)
        {
            if (_functions.TryGetValue(functionCallExpr.Identifier, out var function))
            {
                return function.type.ReturnType;
            }
        }
        
        _logger.LogError("Unable to determine type of node: " + node.GetString(0), _sourceFile, node.Location);
        return null;
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

                if (qbeType.PrimitiveEnum.IsInteger())
                {
                    return new QbeLiteral(qbeType, (long)literal.IntegerValue);
                }
                
                if (qbeType.PrimitiveEnum.IsFloat())
                {
                    return new QbeLiteral(qbeType, literal.FloatValue);
                }
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
        
        if (expression is VariableExpression variableExpr)
        {
            foreach (var scope in _scopes.Reverse())
            {
                if (scope.TryGetValue(variableExpr.Name, out var value))
                {
                    bool isStruct = value.type is NamedType or StructType;
                    if (isLValue || isStruct)
                        return value.qbe;
                    
                    var loaded = _blocks.Peek().Load(value.qbe.PrimitiveEnum, value.qbe);
                    return loaded;
                }
            }

            _logger.LogError("Undefined variable: " + variableExpr.Name, _sourceFile, variableExpr.Location);
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
            var structType = _structs.FirstOrDefault(x => x.Key == structInitExpr.StructName).Value;
            if (structType == default)
            {
                _logger.LogError("Undefined struct type: " + structInitExpr.StructName, _sourceFile, structInitExpr.Location);
                return null;
            }
            
            var local = _blocks.Peek().Allocate(structType.qbe, !_is64Bit);
            for (int i = 0; i < structInitExpr.FieldInitializers.Count; i++)
            {
                var fieldInit = structInitExpr.FieldInitializers[i];
                var fieldType = structType.type.Fields[i].Type;
                var fieldValue = VisitExpression(fieldInit.Item2);
                if (fieldValue == null)
                {
                    _logger.LogError("Invalid struct initializer for field: " + fieldInit.Item1, _sourceFile,
                        fieldInit.Item2.Location);
                    return null;
                }

                var fieldPtr = _blocks.Peek().GetFieldPtr(local, structType.qbe, i, !_is64Bit);
                if (fieldType is PrimaryType || fieldType is PointerType)
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

            var nodeType = GetNodeType(memberAccessExpr.Object);
            
            if (memberAccessExpr.IsPointerAccess)
            {
                target = _blocks.Peek().Load(QbePrimitive.Pointer(), target);
            }
            else if (nodeType is PointerType pointerType)
            {
                _logger.LogError("Cannot access member of a pointer type without using '->' operator.", _sourceFile, memberAccessExpr.Location);
                return null;
            }

            StructType? resolvedStruct = nodeType switch
            {
                StructType st => st,
                NamedType nt when _structs.TryGetValue(nt.Name, out var s) => s.type,
                PointerType { BaseType: NamedType nt2 } when _structs.TryGetValue(nt2.Name, out var s2) => s2.type,
                PointerType { BaseType: StructType st2 } => st2,
                _ => null
            };

            if (resolvedStruct == null)
            {
                _logger.LogError("Member access on non-struct type.", _sourceFile, memberAccessExpr.Location);
                return null;
            }

            var (fieldIndex, field) = resolvedStruct.Fields
                .Index()
                .FirstOrDefault(x => x.Item.Name == memberAccessExpr.MemberName);

            if (field == null)
            {
                _logger.LogError("Undefined field: " + memberAccessExpr.MemberName, _sourceFile,
                    memberAccessExpr.Location);
                return null;
            }

            var qbeStructType = _structs[resolvedStruct.Name].qbe;
            var fieldPtr = _blocks.Peek().GetFieldPtr(target, qbeStructType, fieldIndex, !_is64Bit);

            if (isLValue)
                return fieldPtr;

            var fieldQbeType = GetQbeType(field.Type) as QbePrimitive;
            if (fieldQbeType == null)
            {
                _logger.LogError("Unsupported field type in member access: " + field.Type?.GetString(0), _sourceFile,
                    memberAccessExpr.Location);
                return null;
            }

            return _blocks.Peek().Load(fieldQbeType, fieldPtr);
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

            var targetType = GetNodeType(dereferenceExpr.Expression);
            if (targetType is PointerType pointerType)
            {
                var pointeeType = GetQbeType(pointerType.BaseType);
                if (pointeeType == null)
                {
                    _logger.LogError("Unsupported pointer base type: " + pointerType.BaseType.GetString(0), _sourceFile,
                        pointerType.BaseType.Location);
                    return null;
                }
                
                return _blocks.Peek().Load(pointeeType, target);
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

            return target;
        }

        if (expression is FunctionCallExpression functionCallExpr)
        {
            var identifier = functionCallExpr.Identifier;
            if (!_functions.TryGetValue(identifier, out var function))
            {
                _logger.LogError("Undefined function: " + identifier, _sourceFile, functionCallExpr.Location);
                return null;
            }

            var args = functionCallExpr.Arguments.Select(x => VisitExpression(x)).ToArray();
            if (args.Any(x => x == null))
            {
                _logger.LogError("Invalid argument in function call.", _sourceFile, functionCallExpr.Location);
                return null;
            }

            return _blocks.Peek().Call(function.qbe.Identifier, function.qbe.ReturnType, function.qbe.Arguments.Count, args!);
        }

        _logger.LogError("Unsupported expression type: " + expression.GetString(0), _sourceFile, expression.Location);
        return null;
    }
    
    private string MangleName(string name, Node node)
    {
        return $"_oxy{name}_{node.Location.Line}_{node.Location.Column}";
    }
}