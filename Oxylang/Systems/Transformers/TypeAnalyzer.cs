using System.Diagnostics;
using System.Globalization;
using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Transformers;

public record AnalysisResult
{
    public bool Success { get; init; }
}

// Analyzes types, including type checking, type inference, and type compatibility.
// Produces errors for type mismatches, incompatible types, and other type-related issues. 
public class TypeAnalyzer(ILogger _logger, SourceFile _sourceFile, List<ResolvedReliance> _resolvedReliances, CompilationUnit _thisUnit) : ITransformer<AnalysisResult>
{
    private bool _success = true;
    
    record FieldInfo(string Name, TypeNode Type);
    record StructInfo(string Name, List<GenericType> Generics, List<FieldInfo> Fields);

    record FunctionInfo(
        string Name,
        TypeNode ReturnType,
        List<GenericType> Generics,
        List<(string Name, TypeNode Type)> Parameters,
        bool IsVariadic,
        bool IsExternal);

    List<StructInfo> _structs = new();
    List<FunctionInfo> _functions = new();
    Dictionary<FunctionInfo, FunctionType> _functionTypes = new();
    Dictionary<StructType, FunctionInfo[]> _structExtensions = new();
    Dictionary<string, CompilationUnit> _importAliases = new();
    Stack<Dictionary<string, TypeNode>> _scopeStack = new();
    FunctionInfo? _currentFunction = null;
    
    private void Error(string message, SourceLocation location)
    {
        _success = false;
        _logger.Log(new Log(LogLevel.Error, message, _sourceFile, location));
    }

    private bool TryFindFunction(List<string> identifier, out FunctionInfo? function)
    {
        // Function pointers use variables
        foreach (var scope in _scopeStack)
        {
            if (scope.TryGetValue(identifier.Last(), out var type) && type is FunctionType functionType)
            {
                function = new FunctionInfo(identifier.Last(), functionType.ReturnType, new List<GenericType>(),
                    functionType.ParameterTypes.Select((t, i) => ($"param{i}", t)).ToList(), functionType.IsVariadic, functionType.IsExternal);
                return true;
            }
        }
        
        return TryFindFunctionInUnit(_thisUnit, identifier, out function);
    }

    private bool TryFindFunctionInUnit(CompilationUnit unit, List<string> identifier, out FunctionInfo? function)
    {
        if (identifier.Count == 1)
        {
            if (unit == _thisUnit)
            {
                // We *can* look at private functions in the current unit, since we're already inside the unit, so we check both exports and private functions.
                var privateFunc = _functions.FirstOrDefault(x => x.Name == identifier[0]);  
                if (privateFunc != null)
                {
                    function = privateFunc;
                    return true;
                }
            }
            
            var type = unit.Exports.FirstOrDefault(x => x.Identifier == identifier[0] && x.Type is FunctionType)?.Type as FunctionType;
            if (type == null)
            {
                function = null;
                return false;
            }

            function = new FunctionInfo(identifier[0], type.ReturnType, new List<GenericType>(),
                type.ParameterTypes.Select((t, i) => ($"param{i}", t)).ToList(), type.IsVariadic, type.IsExternal);
            
            return true;
        }
        
        var importAlias = identifier[0];
        if (unit.Reliances.All(r => r.Alias != importAlias))
        {
            function = null;
            return false;
        }

        var reliance = unit.Reliances.First(r => r.Alias == importAlias);
        var nextUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == reliance.Identifier)?.Unit;
        if (nextUnit == null)
        {
            function = null;
            return false;
        }
        
        return TryFindFunctionInUnit(nextUnit, identifier.Skip(1).ToList(), out function);
    }
    
    private bool TryFindStruct(List<string> identifier, out StructInfo? structInfo)
    {
        return TryFindStructInUnit(_thisUnit, identifier.ToList(), out structInfo);
    }

    private bool TryFindStructInUnit(CompilationUnit unit, List<string> toList, out StructInfo structInfo)
    {
        if (toList.Count == 1)
        {
            if (unit == _thisUnit)
            {
                // We *can* look at private structs in the current unit, since we're already inside the unit, so we check both exports and private structs.
                var privateStruct = _structs.FirstOrDefault(x => x.Name == toList[0]);  
                if (privateStruct != null)
                {
                    structInfo = privateStruct;
                    return true;
                }
            }
            
            var type = unit.Exports.FirstOrDefault(x => x.Identifier == toList[0] && x.Type is StructType)?.Type as StructType;
            if (type == null)
            {
                structInfo = null;
                return false;
            }

            structInfo = new StructInfo(toList[0], new List<GenericType>(),
                type.Fields.Select(f => new FieldInfo(f.Name, f.Type)).ToList());
            
            return true;
        }
        
        if (unit.Reliances.All(r => r.Alias != toList[0]))
        {
            structInfo = null;
            return false;
        }
        
        var reliance = unit.Reliances.First(r => r.Alias == toList[0]);
        var subUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == reliance.Identifier)?.Unit;
        if (subUnit == null)
        {
            structInfo = null;
            return false;
        }

        return TryFindStructInUnit(subUnit, toList.Skip(1).ToList(), out structInfo);
    }

    public AnalysisResult Transform(Root node)
    {
        Stopwatch sw = Stopwatch.StartNew();
        Visit(node);
        if (_success)
        {
            _logger.LogInfo("Type analysis completed successfully in " + sw.ElapsedMilliseconds + "ms.", _sourceFile, node.Location);
        }
        return new AnalysisResult { Success = _success };
    }

    public void Visit(Root node)
    {
        _scopeStack.Push(new Dictionary<string, TypeNode>()); // Global scope
        
        foreach (var import in node.Imports)
        {
            Visit(import);
        }

        foreach (var structDef in node.Structs)
        {
            DefineStruct(structDef); // We pre define structs, so that they can be used by other structs, global variables, and functions.
        }
        
        foreach (var structDef in node.Structs)
        {
            Visit(structDef); // Here we actually visit the struct definitions, which will check for things like duplicate field names, and other struct-related issues.
        }

        foreach (var function in node.Functions)
        {
            DefineFunction(function); // Again, we pre define, so that functions can call each other, and themselves recursively.
        }

        foreach (var globalVar in node.GlobalVariables)
        {
            Visit(globalVar); // Global variables *are* linear (top to bottom)
        }

        List<StructType> allStructs = new();
        allStructs.AddRange(node.Structs.Select(x => x.StructType));
        allStructs.AddRange(_resolvedReliances.SelectMany(x =>
            x.Unit.Exports.Where(y => y.Type is StructType).Select(z => z.Type as StructType))!);
        
        foreach (var structType in allStructs)
        {
            var extensions = ResolveExtensionFunctions(structType, _thisUnit);
            _structExtensions[structType] = extensions;
        }

        foreach (var function in node.Functions)
        {
            Visit(function);
        }
    }
    
    private bool IsFunctionExtension(StructType structType, FunctionType functionType)
    {
        if (functionType.ParameterTypes.Count == 0)
            return false;

        if (!functionType.IsExtensionMethod)
            return false;

        var firstParam = functionType.ParameterTypes[0];
        if (firstParam is PointerType pt)
        {
            return AreTypesCompatible(pt.BaseType, structType);
        }
        
        return AreTypesCompatible(firstParam, structType);
    }

    private FunctionInfo[] ResolveExtensionFunctions(StructType definition, CompilationUnit unit)
    {
        List<FunctionInfo> extensions = new();
        if (unit == _thisUnit)
        {
            extensions.AddRange(_functions.Where(f => IsFunctionExtension(definition, _functionTypes[f])));
        }
        
        foreach (var export in unit.Exports.Where(e => e.Type is FunctionType))
        {
            if (IsFunctionExtension(definition, (export.Type as FunctionType)!))
            {
                var functionType = export.Type as FunctionType;
                extensions.Add(new FunctionInfo(export.Identifier, functionType!.ReturnType, new List<GenericType>(),
                    functionType.ParameterTypes.Select((t, i) => ($"param{i}", t)).ToList(), functionType.IsVariadic, functionType.IsExternal));
            }
        }
        
        foreach (var reliance in unit.Reliances)
        {
            var subUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == reliance.Identifier)?.Unit;
            if (subUnit != null)
            {
                extensions.AddRange(ResolveExtensionFunctions(definition, subUnit));
            }
        }

        var extensionNames = new HashSet<string>();
        foreach (var extension in extensions)
        {
            if (extensionNames.Contains(extension.Name))
            {
                Error($"Struct '{definition.Name}' has multiple extension functions named '{extension.Name}', which is not allowed.", definition.Location);
            }
            else
            {
                extensionNames.Add(extension.Name);
            }
        }
        
        return extensions.ToArray();
    }

    private void DefineFunction(FunctionDefinition function)
    {
        if (_functions.Any(x => x.Name == function.Name))
        {
            Error($"Function '{function.Name}' is already defined.", function.Location);
            return;
        }
        
        var info = new FunctionInfo(function.Name, function.ReturnType, function.GenericTypes.ToList(),
            function.Parameters.Select(x => (x.Name, ResolveType(x.Type!))).ToList(), function.IsVariadic, function.IsExternal);

        var type = function.FunctionType;
        _functions.Add(info);
        _functionTypes[info] = type;
        
        
        var exportedFunction =
            _thisUnit.Exports.FirstOrDefault(e => e.Identifier == function.Name && e.Type is FunctionType);
        
        if (exportedFunction != null && exportedFunction.Type is FunctionType exportedFunctionType)
        {
            var index = _thisUnit.Exports.IndexOf(exportedFunction);
            _thisUnit.Exports[index] = new Export(exportedFunction.Identifier,
                new FunctionType(function.Parameters.Select(p => ResolveType(p.Type!)).ToList(),
                    ResolveType(function.ReturnType)!, function.IsVariadic, function.IsExternal, function.Parameters.Count > 0 && function.Parameters[0].Name == "this"), exportedFunction.DefinitionNode);
            
        }
    }

    private void DefineStruct(StructDefinition structDef)
    {
        if (_structs.All(x => x.Name != structDef.Name))
        {
            _structs.Add(new StructInfo(structDef.Name, structDef.GenericTypes.ToList(),
                structDef.Fields.Select(x => new FieldInfo(x.Name, x.Type)).ToList()));
            return;
        }

        Error($"Struct '{structDef.Name}' is already defined.", structDef.Location);
    }

    public void Visit(AlignOfExpression node)
    {
        var operandType = ResolveType(node.Type);
        if (operandType == null)
        {
            Error($"Could not resolve type of operand in alignof expression.", node.Type.Location);
        }
        
        // Success, the type of alignof is always u64, so we don't need to do any further checks.
    }

    public void Visit(AssignmentExpression node)
    {
        var leftType = ResolveType(node.Left);
        var rightType = ResolveType(node.Right);
        if (leftType == null || rightType == null)
        {
            Error($"Could not resolve type of assignment expression.", node.Location);
            return;
        }
        
        if (!AreTypesCompatible(leftType, rightType))
        {
            Error($"Types of left and right sides of assignment expression are not compatible ('{leftType.GetString(0)}' vs '{rightType.GetString(0)}').", node.Location);
        }
        
        // Success
    }

    public void Visit(BinaryExpression node)
    {
        var leftType = ResolveType(node.Left);
        var rightType = ResolveType(node.Right);
        if (leftType == null || rightType == null)
        {
            Error($"Could not resolve type of binary expression.", node.Location);
            return;
        }
        
        if (!AreTypesCompatible(leftType, rightType))
        {
            Error($"Types of left and right sides of binary expression are not compatible ('{leftType.GetString(0)}' vs '{rightType.GetString(0)}').", node.Location);
        }
        
        // Success
    }

    public void Visit(BlockStatement node)
    {
        _scopeStack.Push(new());
        foreach (var statement in node.Statements)
        {
            statement.Accept(this);
        }
        _scopeStack.Pop();
    }

    public void Visit(CastExpression node)
    {
        var targetType = ResolveType(node.TargetType);
        var expressionType = ResolveType(node.Expression);
        if (targetType == null || expressionType == null)
        {
            Error($"Could not resolve type of cast expression.", node.Location);
            return;
        }
        
        // For simplicity, we allow all casts, but you could add rules here to restrict certain casts if you wanted to.
    }

    public void Visit(FunctionCallExpression node)
    {
        if (!TryFindFunction(node.Identifier, out var function))
        {
            Error($"Function '{string.Join("::",node.Identifier)}' is not defined.", node.Location);
            return;
        }

        var substitutions = BuildSubstitutions(function, node);
        function = new FunctionInfo(function.Name, SubstituteGenerics(function.ReturnType, substitutions),
            function.Generics,
            function.Parameters.Select(p => (p.Name, SubstituteGenerics(p.Type, substitutions))).ToList(),
            function.IsVariadic, function.IsExternal);
        
        for (int i = 0; i < node.Arguments.Count; i++)
        {
            var argumentType = ResolveType(node.Arguments[i]);
            if (argumentType == null)
            {
                Error($"Could not resolve type of argument {i} in call to function '{string.Join("::", node.Identifier)}'.", node.Arguments[i].Location);
                return;
            }
            
            if (i >= function.Parameters.Count)
            {
                if (!function.IsVariadic)
                {
                    Error($"Too many arguments in call to function '{string.Join("::", node.Identifier)}'. Expected {function.Parameters.Count}, got {node.Arguments.Count}.", node.Location);
                }
                break;
            }
            
            var parameterType = function.Parameters[i].Type;
            if (!AreTypesCompatible(parameterType, argumentType))
            {
                Error(
                    $"Type of argument {i} in call to function '{string.Join("::", node.Identifier)}' is not compatible with parameter type '{parameterType.GetString(0)}'.",
                    node.Arguments[i].Location);
            }
            
            // Success types are compatible
        }
        
        // Check for too few arguments
        if (node.Arguments.Count < function.Parameters.Count)
        {
            Error($"Too few arguments in call to function '{string.Join("::", node.Identifier)}'. Expected {function.Parameters.Count}, got {node.Arguments.Count}.", node.Location);
        }
        
        // Success, this is a valid function call
    }

    public void Visit(FunctionDefinition node)
    {
        var functionInfo = _functions.First(x => x.Name == node.Name);
        // Push the function's generic types and parameters into the scope, so that they can be used by the function body.
        var scope = new Dictionary<string, TypeNode>();
        foreach (var generic in functionInfo.Generics)
        {
            scope[generic.Name] = generic;
        }
        foreach (var parameter in functionInfo.Parameters)
        {
            if (scope.ContainsKey(parameter.Name))
            {
                Error($"Parameter '{parameter.Name}' is already defined in function '{node.Name}'.", node.Location);
            }
            else
            {
                scope[parameter.Name] = parameter.Type;
            }
        }
        
        _scopeStack.Push(scope);
        _currentFunction = functionInfo;
        
        if (node.Body != null)
        {
            Visit(node.Body);
        }
        
        _currentFunction = null;
        _scopeStack.Pop();
    }

    public void Visit(ImportStatement node)
    {
        if (_importAliases.ContainsKey(node.Alias))
        {
            Error($"Import alias '{node.Alias}' is already defined.", node.Location);
        }
        else
        {
            _importAliases[node.Alias] = _resolvedReliances.First(x => x.Identifier == node.Path).Unit;
        }
    }

    public void Visit(LiteralExpression node)
    {
        // The type of a literal expression is determined by its Type property, which is already set during parsing, so we don't need to do any further analysis here.
    }

    public void Visit(PostfixExpression node)
    {
        var operandType = ResolveType(node.Primary);
        if (operandType == null)
        {
            Error($"Could not resolve type of operand in postfix expression.", node.Primary.Location);
            return;
        }
        
        // For simplicity, we allow postfix expressions on any type, but you could add rules here to restrict certain types if you wanted to (for example, only allowing postfix increment and decrement on numeric types).
    }

    public void Visit(ReturnStatement node)
    {
        if (_currentFunction == null)
        {
            Error("Return statement is not inside a function.", node.Location);
            return;
        }
        
        if (node.Expression == null)
        {
            if (_currentFunction.ReturnType is not VoidType)
            {
                Error($"Return statement in function '{_currentFunction.Name}' must return a value of type '{_currentFunction.ReturnType.GetString(0)}'.", node.Location);
            }
            return;
        }
        
        var returnType = ResolveType(node.Expression);
        if (returnType == null)
        {
            Error("Could not resolve type of return expression.", node.Expression.Location);
            return;
        }
        
        if (!AreTypesCompatible(_currentFunction.ReturnType, returnType))
        {
            Error($"Type of return expression is not compatible with function return type '{_currentFunction.ReturnType.GetString(0)}'.", node.Expression.Location);
        }
        
        // Success, the return type is compatible with the function's declared return type.
    }

    public void Visit(SizeOfExpression node)
    {
        var operandType = ResolveType(node.Type);
        if (operandType == null)
        {
            Error($"Could not resolve type of operand in sizeof expression.", node.Type.Location);
        }
        
        // Success, the type of sizeof is always u64, so we don't need to do any further checks.
    }

    public void Visit(StringExpression node)
    {
        // The type of a string expression is always a pointer to u8, so we don't need to do any further analysis here.
    }

    public void Visit(StructDefinition node)
    {
        var structInfo = _structs.First(x => x.Name == node.Name);
        // Push both the struct's generic types and its fields into the scope, so that they can be used by the struct's fields (for example, for recursive structs like linked lists).
        var scope = new Dictionary<string, TypeNode>();
        foreach (var generic in structInfo.Generics)
        {
            scope[generic.Name] = generic;
        }
        
        var fieldNames = new HashSet<string>();
        foreach (var field in node.Fields)
        {
            if (fieldNames.Contains(field.Name))
            {
                Error($"Field '{field.Name}' is already defined in struct '{node.Name}'.", field.Location);
            }
            else
            {
                fieldNames.Add(field.Name);
            }
        }
        
        // Verify that the types of the fields are valid (not void, not undefined references)
        foreach (var field in node.Fields)
        {
            var type = ResolveType(field.Type!);
            if (type == null)
            {
                Error(
                    $"Type '{field.Type!.GetString(0)}' of field '{field.Name}' in struct '{node.Name}' is not defined.",
                    field.Type.Location);
            }
            else if (type is VoidType)
            {
                Error(
                    $"Field '{field.Name}' in struct '{node.Name}' cannot be of type 'void'.",
                    field.Type!.Location);
            }
            
            // Success!
        }
    }

    private TypeNode? ResolveType(TypeNode type)
    {
        if (type is GenericType genericType)
        {
            // Check if the generic type is defined in the current scope (for example, as a generic parameter of the struct or function we're currently analyzing).
            foreach (var scope in _scopeStack)
            {
                if (scope.TryGetValue(genericType.Name, out var resolvedType))
                {
                    return resolvedType;
                }
            }
            
            Error($"Generic type '{string.Join("::", genericType.Name)}' is not defined in the current scope.", genericType.Location);
            return null;
        }

        if (type is StructType structType)
        {
            var structInfo = _structs.FirstOrDefault(x => x.Name == structType.Name);
            if (structInfo == null)
            {
                Error($"Struct type '{structType.Name}' is not defined.", structType.Location);
                return null;
            }
            
            return structType;
        }

        if (type is NamedType namedType)
        {
            if (TryFindStruct(namedType.Parts, out var structInfo))
            {
                var substitutions = new Dictionary<string, TypeNode>();
                if (structInfo.Generics.Count > 0)
                {
                    if (namedType.TypeArguments.Count != structInfo.Generics.Count)
                    {
                        Error(
                            $"Struct '{string.Join("::", namedType.Parts)}' expects {structInfo.Generics.Count} generic argument(s), got {namedType.TypeArguments.Count}.",
                            namedType.Location);
                    }
                    else
                    {
                        for (int i = 0; i < structInfo.Generics.Count; i++)
                        {
                            var resolved = ResolveType(namedType.TypeArguments[i]);
                            if (resolved != null)
                                substitutions[structInfo.Generics[i].Name] = resolved;
                        }
                    }
                }

                return new StructType(namedType.Parts.Last(), structInfo.Fields
                    .Select(f => new VariableDeclaration(f.Name, SubstituteGenerics(f.Type, substitutions), new(), null,
                        f.Type!.Location))
                    .ToList());
            }
            
            if (TryFindFunction(namedType.Parts, out var functionInfo))
            {
                var substitutions = new Dictionary<string, TypeNode>();
                if (functionInfo.Generics.Count > 0)
                {
                    if (namedType.TypeArguments.Count != functionInfo.Generics.Count)
                    {
                        Error(
                            $"Function '{string.Join("::", namedType.Parts)}' expects {functionInfo.Generics.Count} generic argument(s), got {namedType.TypeArguments.Count}.",
                            namedType.Location);
                    }
                    else
                    {
                        for (int i = 0; i < functionInfo.Generics.Count; i++)
                        {
                            var resolved = ResolveType(namedType.TypeArguments[i]);
                            if (resolved != null)
                                substitutions[functionInfo.Generics[i].Name] = resolved;
                        }
                    }
                }

                return new FunctionType(functionInfo.Parameters.Select(p => SubstituteGenerics(p.Type, substitutions)).ToList(),
                    SubstituteGenerics(functionInfo.ReturnType, substitutions), functionInfo.IsVariadic, functionInfo.IsExternal, functionInfo.Parameters.Count > 0 && functionInfo.Parameters[0].Name == "this");
            }
            
            Error($"Type '{string.Join("::", namedType.Parts)}' is not defined.", namedType.Location);
            return null;
        }

        if (type is NamedType)
        {
            Error($"Type '{string.Join("::", ((NamedType)type).Parts)}' is not defined.", type.Location);
            return null;
        }
        
        return type; // For primitive types like int, float, etc., we assume they are always valid and just return them as is.
    }

    private TypeNode? ResolveType(Expression expression)
    {
        if (expression is VariableExpression variableExpression)
        {
            if (variableExpression.Name.Count == 1)
            {

                foreach (var scope in _scopeStack)
                {
                    if (scope.TryGetValue(variableExpression.Name[0], out var resolvedType))
                    {
                        return resolvedType;
                    }
                }

                if (_importAliases.TryGetValue(variableExpression.Name[0], out var unit))
                    return new ModuleType(unit);
                
                if (TryFindFunction(variableExpression.Name, out var functionInfo))
                {
                    return new FunctionType(functionInfo.Parameters.Select(p => p.Type).ToList(), functionInfo.ReturnType, functionInfo.IsVariadic, functionInfo.IsExternal, functionInfo.Parameters.Count > 0 && functionInfo.Parameters[0].Name == "this");
                }

                Error($"Variable '{string.Join("::", variableExpression.Name)}' is not defined in the current scope.",
                    variableExpression.Location);
            }
            else
            {
                // This could be either a struct or a function, so we check both.
                if (TryFindStruct(variableExpression.Name, out var structInfo))
                {
                    return new StructType( variableExpression.Name.Last(),
                        structInfo.Fields.Select(f => new VariableDeclaration(f.Name, f.Type, new(), null, f.Type!.Location)).ToList());
                }
                
                if (TryFindFunction(variableExpression.Name, out var functionInfo))
                {
                    return new FunctionType(
                        functionInfo.Parameters.Select(p => p.Type).ToList(), functionInfo.ReturnType, functionInfo.IsVariadic, functionInfo.IsExternal, functionInfo.Parameters.Count > 0 && functionInfo.Parameters[0].Name == "this");
                }

                Error($"Identifier '{string.Join("::", variableExpression.Name)}' is not defined in the current scope.", variableExpression.Location);
            }

            return null;
        }

        if (expression is LiteralExpression literalExpression)
            return literalExpression.Type;

        if (expression is StringExpression stringExpression)
            return new PointerType(new PrimaryType(PrimaryType.PrimaryTypeKind.U8));
        
        if (expression is FunctionCallExpression functionCallExpression)
        {
            if (!TryFindFunction(functionCallExpression.Identifier, out var functionInfo))
            {
                Error($"Function '{string.Join("::", functionCallExpression.Identifier)}' is not defined.", functionCallExpression.Location);
                return null;
            }
            
            var substitutions = BuildSubstitutions(functionInfo!, functionCallExpression);
            var returnType = SubstituteGenerics(functionInfo.ReturnType, substitutions);
            
            return returnType;
        }
        
        if (expression is CastExpression castExpression)
        {
            var targetType = ResolveType(castExpression.TargetType);
            if (targetType == null)
            {
                Error($"Could not resolve target type of cast expression.", castExpression.TargetType.Location);
                return null;
            }
            return targetType;
        }

        if (expression is UnaryExpression unaryExpression)
        {
            var operandType = ResolveType(unaryExpression.Operand);
            if (operandType == null)
            {
                Error($"Could not resolve type of operand in unary expression.", unaryExpression.Operand.Location);
                return null;
            }
            
            return operandType;
        }
        
        if (expression is BinaryExpression binaryExpression)
        {
            var leftType = ResolveType(binaryExpression.Left);
            var rightType = ResolveType(binaryExpression.Right);
            if (leftType == null || rightType == null)
            {
                Error($"Could not resolve type of binary expression.", binaryExpression.Location);
                return null;
            }
            
            if (!AreTypesCompatible(leftType, rightType))
            {
                Error($"Types of left and right sides of binary expression are not compatible ('{leftType.GetString(0)}' vs '{rightType.GetString(0)}').", binaryExpression.Location);
                return null;
            }
            
            // For simplicity, we assume that the result type of a binary expression is the same as the types of its operands (which must be compatible).
            return leftType;
        }
        
        if (expression is PostfixExpression postfixExpression)
        {
            var operandType = ResolveType(postfixExpression.Primary);
            if (operandType == null)
            {
                Error($"Could not resolve type of operand in postfix expression.", postfixExpression.Primary.Location);
                return null;
            }
            
            // For simplicity, we assume that the result type of a postfix expression is the same as the type of its operand.
            return operandType;
        }

        if (expression is StructInitializerExpression structInitializerExpression)
        {
            var name = structInitializerExpression.StructName;
            if (!TryFindStruct(name, out var structInfo))
            {
                Error($"Struct '{name}' is not defined.", structInitializerExpression.Location);
                return null;
            }

            if (structInitializerExpression.GenericArguments.Count != structInfo!.Generics.Count)
            {
                Error(
                    $"Struct '{name}' expects {structInfo.Generics.Count} generic argument(s), got {structInitializerExpression.GenericArguments.Count}.",
                    structInitializerExpression.Location);
                return null;
            }
            
            if (structInitializerExpression.GenericArguments.Count > 0)
            {
                var substitutions = BuildSubstitutions(structInfo, structInitializerExpression);
                structInfo = new StructInfo(structInfo.Name, structInfo.Generics, structInfo.Fields
                    .Select(f => new FieldInfo(f.Name, SubstituteGenerics(f.Type, substitutions)))
                    .ToList());
            }

            var initializers = structInitializerExpression.FieldInitializers;
            bool hasErrors = false;
            int initializerCount = initializers.Count;
            if (initializerCount > structInfo.Fields.Count)
            {
                Error($"Too many field initializers in struct initializer for struct '{name}'. Expected at most {structInfo.Fields.Count}, got {initializerCount}.", structInitializerExpression.Location);
                hasErrors = true;
            }
            
            foreach (var initializer in initializers)
            {
                var fieldInfo = structInfo.Fields.FirstOrDefault(x => x.Name == initializer.Item1);
                if (fieldInfo == null)
                {
                    Error($"Struct '{name}' does not have a field named '{initializer.Item1}'.", initializer.Item2.Location);
                    hasErrors = true;
                    continue;
                }
                
                var initializerType = ResolveType(initializer.Item2);
                if (initializerType == null)
                {
                    Error(
                        $"Could not resolve type of initializer for field '{initializer.Item1}' in struct initializer for struct '{name}'.",
                        initializer.Item2.Location);
                    hasErrors = true;
                    continue;
                }
                
                if (!AreTypesCompatible(fieldInfo.Type, initializerType))
                {
                    Error(
                        $"Type of initializer for field '{initializer.Item1}' in struct initializer for struct '{name}' is not compatible with field type '{fieldInfo.Type.GetString(0)}'.",
                        initializer.Item2.Location);
                    hasErrors = true;
                    continue;
                }
                
                // Success, this field initializer is valid.
            }
            
            if (hasErrors)
            {
                return null;
            }

            List<VariableDeclaration> fields = new();
            foreach (var field in structInfo.Fields)
            {
                var initializer = initializers.FirstOrDefault(x => x.Item1 == field.Name);
                if (initializer != default)
                {
                    fields.Add(new VariableDeclaration(field.Name, field.Type, new (), initializer.Item2, initializer.Item2.Location));
                }
                else
                {
                    // No initializer provided for this field, so we just use a default value (for example, 0 for integers, null for pointers, etc.). The exact default value is not important for type analysis, since we're only interested in the type of the field, which is already defined in the struct definition.
                    fields.Add(new VariableDeclaration(field.Name, field.Type, new (), null, structInitializerExpression.Location));
                }
            }
            
            if (name.Count > 1)
            {
                if (!TryFindStruct(name.ToList(), out var parentStructInfo))
                {
                    Error($"Struct '{string.Join("::", name)}' is not defined.", structInitializerExpression.Location);
                    return null;
                }

                return new StructType(parentStructInfo.Name,
                    parentStructInfo.Fields.Select(x =>
                            new VariableDeclaration(x.Name, x.Type, new(), null, structInitializerExpression.Location))
                        .ToList());
            }
            
            return new StructType(name[0], fields);
        }

        if (expression is MemberAccessExpression memberAccessExpression)
        {
            var leftType = ResolveType(memberAccessExpression.Object);
            if (leftType is ModuleType moduleType)
            {
                var unit = moduleType.Unit;

                var subReliance = unit.Reliances.FirstOrDefault(r => r.Alias == memberAccessExpression.MemberName);
                if (subReliance != null)
                {
                    var subUnit = _resolvedReliances.FirstOrDefault(r => r.Identifier == subReliance.Identifier)?.Unit;
                    if (subUnit != null)
                        return new ModuleType(subUnit);
                }

                var export = unit.Exports.FirstOrDefault(e => e.Identifier == memberAccessExpression.MemberName);
                if (export == null)
                {
                    Error($"Module '{unit.Identifier}' has no exported member '{memberAccessExpression.MemberName}'.", memberAccessExpression.Location);
                    return null;
                }

                return ResolveExportedType(unit, export);
            }
            
            if (memberAccessExpression.IsPointerAccess)
            {
                if (leftType is PointerType pointerType)
                {
                    leftType = pointerType.BaseType;
                }
                else
                {
                    Error($"Cannot use '->' operator on non-pointer type '{leftType.GetString(0)}'.", memberAccessExpression.Location);
                }
            }
            
            if (leftType is NamedType namedType)
            {
                if (TryFindStruct(namedType.Parts, out var structInfo))
                {
                    var substitutions = new Dictionary<string, TypeNode>();
                    if (structInfo!.Generics.Count > 0)
                    {
                        if (namedType.TypeArguments.Count != structInfo.Generics.Count)
                        {
                            Error(
                                $"Struct '{string.Join("::", namedType.Parts)}' expects {structInfo.Generics.Count} generic argument(s), got {namedType.TypeArguments.Count}.",
                                namedType.Location);
                        }
                        else
                        {
                            for (int i = 0; i < structInfo.Generics.Count; i++)
                            {
                                var resolved = ResolveType(namedType.TypeArguments[i]);
                                if (resolved != null)
                                    substitutions[structInfo.Generics[i].Name] = resolved;
                            }
                            
                            leftType = SubstituteGenerics(leftType, substitutions);
                        }
                    }

                    leftType = new StructType(namedType.Parts.Last(), structInfo.Fields
                        .Select(f => new VariableDeclaration(f.Name, SubstituteGenerics(f.Type, substitutions), new(),
                            null, f.Type!.Location))
                        .ToList());
                }

                if (TryFindFunction(namedType.Parts, out var functionInfo))
                {
                    var substitutions = new Dictionary<string, TypeNode>();
                    if (functionInfo!.Generics.Count > 0)
                    {
                        if (namedType.TypeArguments.Count != functionInfo.Generics.Count)
                        {
                            Error(
                                $"Function '{string.Join("::", namedType.Parts)}' expects {functionInfo.Generics.Count} generic argument(s), got {namedType.TypeArguments.Count}.",
                                namedType.Location);
                        }
                        else
                        {
                            for (int i = 0; i < functionInfo.Generics.Count; i++)
                            {
                                var resolved = ResolveType(namedType.TypeArguments[i]);
                                if (resolved != null)
                                    substitutions[functionInfo.Generics[i].Name] = resolved;
                            }

                            leftType = SubstituteGenerics(leftType, substitutions);
                        }
                    }

                    leftType = new FunctionType(functionInfo.Parameters.Select(p => SubstituteGenerics(p.Type, substitutions)).ToList(),
                        SubstituteGenerics(functionInfo.ReturnType, substitutions), functionInfo.IsVariadic,
                        functionInfo.IsExternal,
                        functionInfo.Parameters.Count > 0 && functionInfo.Parameters[0].Name == "this");
                }
            }
            
            if (leftType is not StructType structType)
            {
                Error($"Cannot access member '{memberAccessExpression.MemberName}' of non-struct type '{leftType.GetString(0)}'.", memberAccessExpression.Location);
                return null;
            }

            if (_structExtensions.Keys.Any(x=>x.Name == structType.Name))
            {
                var extensions = _structExtensions.First(x => x.Key.Name == structType.Name).Value;
                if (extensions.Any(x => x.Name == memberAccessExpression.MemberName))
                {
                    var extension = extensions.First(x => x.Name == memberAccessExpression.MemberName);
                    return new FunctionType(extension.Parameters.Select(p => p.Type).ToList(), extension.ReturnType,
                        extension.IsVariadic, extension.IsExternal,
                        true);
                }
            }

            var fieldInfo = structType.Fields.FirstOrDefault(x => x.Name == memberAccessExpression.MemberName);
            if (fieldInfo == null)
            {
                Error($"Struct '{structType.Name}' does not have a field named '{memberAccessExpression.MemberName}'.",
                    memberAccessExpression.Location);
                return null;
            }
            
            return fieldInfo.Type;
        }
        
        if (expression is AddressOfExpression addressOfExpression)
        {
            var operandType = ResolveType(addressOfExpression.Expression);
            if (operandType == null)
            {
                Error($"Could not resolve type of operand in address-of expression.", addressOfExpression.Expression.Location);
                return null;
            }
            
            return new PointerType(operandType);
        }
        
        if (expression is DereferenceExpression dereferenceExpression)
        {
            var operandType = ResolveType(dereferenceExpression.Expression);
            if (operandType == null)
            {
                Error($"Could not resolve type of operand in dereference expression.", dereferenceExpression.Expression.Location);
                return null;
            }
            
            if (operandType is PointerType pointerType)
            {
                return pointerType.BaseType;
            }
            else
            {
                Error($"Cannot dereference non-pointer type '{operandType.GetString(0)}'.", dereferenceExpression.Location);
                return null;
            }
        }
        
        if (expression is SizeOfExpression sizeOfExpression)
        {
            var operandType = ResolveType(sizeOfExpression.Type);
            if (operandType == null)
            {
                Error($"Could not resolve type of operand in sizeof expression.", sizeOfExpression.Type.Location);
                return null;
            }

            return new PrimaryType(PrimaryType.PrimaryTypeKind.U64);
        }
        
        if (expression is AlignOfExpression alignOfExpression)
        {
            var operandType = ResolveType(alignOfExpression.Type);
            if (operandType == null)
            {
                Error($"Could not resolve type of operand in alignof expression.", alignOfExpression.Type.Location);
                return null;
            }

            return new PrimaryType(PrimaryType.PrimaryTypeKind.U64);
        }

        if (expression is MethodCallExpression methodCallExpression)
        {
            var objectType = ResolveType(methodCallExpression.Method);
            if (objectType == null)
            {
                Error($"Could not resolve type of object in method call expression.",
                    methodCallExpression.Method.Location);
                return null;
            }
            
            if (objectType is not FunctionType functionType)
            {
                Error($"Cannot call method on non-function type '{objectType.GetString(0)}'.",
                    methodCallExpression.Method.Location);
                return null;
            }

            return functionType.ReturnType;
        }

        return null;
    }
    
    private TypeNode? ResolveExportedType(CompilationUnit unit, Export export)
    {
        // Spin up a sub-analyzer for the dependency unit and ask it what the export's type is
        var subAnalyzer = new TypeAnalyzer(
            _logger, 
            new SourceFile(unit.FilePath, unit.SourceCode), 
            _resolvedReliances,
            unit);
        
        return subAnalyzer.ResolveExportedSymbol(export.Identifier);
    }

    private TypeNode? ResolveExportedSymbol(string name)
    {
        _thisUnit.Exports.FirstOrDefault(e => e.Identifier == name)?.Type.Accept(this);
        return _thisUnit.Exports.FirstOrDefault(e => e.Identifier == name)?.Type;
    }

    public void Visit(StructInitializerExpression node)
    {
        var type = ResolveType(node);
        if (type == null)
        {
            Error($"Could not resolve type of struct initializer expression.", node.Location);
            return;
        }
        
        // Success, the type of a struct initializer expression is the struct type that it's initializing, so we don't need to do any further checks here.
    }

    public void Visit(VariableDeclaration node)
    {
        TypeNode? explicitType = null;
        if (node.Type != null)
        {
            explicitType = ResolveType(node.Type);
            if (explicitType == null)
            {
                Error($"Type '{node.Type.GetString(0)}' of variable '{node.Name}' is not defined.", node.Type.Location);
            }
            else if (explicitType is VoidType)
            {
                Error($"Variable '{node.Name}' cannot be of type 'void'.", node.Type.Location);
            }
        }
        
        if (explicitType == null && node.Initializer == null)
        {
            Error($"Variable '{node.Name}' must have an explicit type or an initializer.", node.Location);
        }
        
        // Try to get the type of the initializer, if it exists.
        if (node.Initializer != null)
        {
            var initializerType = ResolveType(node.Initializer);
            if (initializerType == null)
            {
                Error($"Could not resolve type of initializer for variable '{node.Name}'.", node.Initializer.Location);
            }
            
            if (explicitType != null && !AreTypesCompatible(explicitType, initializerType))
            {
                Error($"Type of initializer for variable '{node.Name}' is not compatible with the explicit type '{explicitType.GetString(0)}'.", node.Initializer.Location);
            }

            node.Initializer.Accept(this);
        }
        
        // Even if it errors, we still want to add the variable to the scope, so we dont get a cascade of errors for every use after this declaration.
        var variableType = explicitType ?? ResolveType(node.Initializer!);
        _scopeStack.Peek()[node.Name] = variableType!;
    }

    private bool AreTypesCompatible(TypeNode explicitType, TypeNode initializerType)
    {
        if (explicitType is NamedType explicitNamed)
        {
            explicitType = ResolveType(explicitNamed);
        }
        
        if (initializerType is NamedType initializerNamed)
        {
            initializerType = ResolveType(initializerNamed);
        }
        
        if (explicitType == null || initializerType == null)
        {
            return false;
        }

        if (explicitType is StructType explicitStruct && initializerType is StructType initializerStruct)
        {
            return explicitStruct.Name == initializerStruct.Name;
        }
        
        if (explicitType is FunctionType explicitFunction && initializerType is FunctionType initializerFunction)
        {
            return explicitFunction.GetString(0) == initializerFunction.GetString(0);
        }
        
        if (explicitType is PointerType explicitPointer && initializerType is PointerType initializerPointer)
        {
            if (explicitPointer.BaseType is VoidType || initializerPointer.BaseType is VoidType)
            {
                // Allow implicit conversion between pointers to void and pointers to other types, since this is a common pattern in C for things like dynamic memory allocation and generic data structures.
                return true;
            }
            
            return AreTypesCompatible(explicitPointer.BaseType, initializerPointer.BaseType);
        }

        if (explicitType is PrimaryType explicitPrimary && initializerType is PrimaryType initializerPrimary)
        {
            return explicitPrimary.Kind == initializerPrimary.Kind;
        }

        return explicitType.GetString(0) == initializerType.GetString(0);
    }

    public void Visit(VariableExpression node)
    {
        if (node.Name.Count > 1)
        {
            Error("485784Im lazy, not implemented yet or something", node.Location);
            return;
        }
        
        foreach (var scope in _scopeStack)
        {
            if (scope.TryGetValue(node.Name[0], out var resolvedType))
            {
                return;
            }
        }
        
        if (TryFindFunction(node.Name, out var functionInfo))
        {
            return;
        }

        Error($"Variable '{string.Join("::",node.Name)}' is not defined in the current scope.", node.Location);
    }

    public void Visit(AddressOfExpression node)
    {
        var operandType = ResolveType(node.Expression);
        if (operandType == null)
        {
            Error($"Could not resolve type of operand in address-of expression.", node.Expression.Location);
            return;
        }
        
        // Success, the type of an address-of expression is always a pointer to the type of its operand, so we don't need to do any further checks here.
    }

    public void Visit(DereferenceExpression node)
    {
        var operandType = ResolveType(node.Expression);
        if (operandType == null)
        {
            Error($"Could not resolve type of operand in dereference expression.", node.Expression.Location);
            return;
        }
        
        if (operandType is PointerType pointerType)
        {
            // Success, the type of a dereference expression is always the base type of the pointer, so we don't need to do any further checks here.
        }
        else
        {
            Error($"Cannot dereference non-pointer type '{operandType.GetString(0)}'.", node.Location);
        }
    }

    public void Visit(MemberAccessExpression node)
    {
        var objectType = ResolveType(node.Object);
        if (objectType == null)
        {
            Error($"Could not resolve type of object in member access expression.", node.Object.Location);
            return;
        }
        
        if (node.IsPointerAccess)
        {
            if (objectType is PointerType pointerType)
            {
                objectType = pointerType.BaseType;
            }
            else
            {
                Error($"Cannot use '->' operator on non-pointer type '{objectType.GetString(0)}'.", node.Location);
                return;
            }
        }
        
        if (objectType is NamedType namedType)
        {
            if (TryFindStruct(namedType.Parts, out var structInfo))
            {
                var substitutions = new Dictionary<string, TypeNode>();
                if (structInfo!.Generics.Count > 0)
                {
                    if (namedType.TypeArguments.Count != structInfo.Generics.Count)
                    {
                        Error(
                            $"Struct '{string.Join("::", namedType.Parts)}' expects {structInfo.Generics.Count} generic argument(s), got {namedType.TypeArguments.Count}.",
                            namedType.Location);
                    }
                    else
                    {
                        for (int i = 0; i < structInfo.Generics.Count; i++)
                        {
                            var resolved = ResolveType(namedType.TypeArguments[i]);
                            if (resolved != null)
                                substitutions[structInfo.Generics[i].Name] = resolved;
                        }
                        
                        objectType = SubstituteGenerics(objectType, substitutions);
                    }
                }
            }

            if (TryFindFunction(namedType.Parts, out var functionInfo))
            {
                var substitutions = new Dictionary<string, TypeNode>();
                if (functionInfo!.Generics.Count > 0)
                {
                    if (namedType.TypeArguments.Count != functionInfo.Generics.Count)
                    {
                        Error(
                            $"Function '{string.Join("::", namedType.Parts)}' expects {functionInfo.Generics.Count} generic argument(s), got {namedType.TypeArguments.Count}.",
                            namedType.Location);
                    }
                    else
                    {
                        for (int i = 0; i < functionInfo.Generics.Count; i++)
                        {
                            var resolved = ResolveType(namedType.TypeArguments[i]);
                            if (resolved != null)
                                substitutions[functionInfo.Generics[i].Name] = resolved;
                        }

                        objectType = SubstituteGenerics(objectType, substitutions);
                    }
                }
            }
        }
        
        if (objectType is StructType structType)
        {
            var fieldInfo = structType.Fields.FirstOrDefault(x => x.Name == node.MemberName);
            if (fieldInfo == null)
            {
                Error($"Struct '{structType.Name}' does not have a field named '{node.MemberName}'.", node.Location);
                return;
            }
            return;
        }
        
        Error($"Cannot access member '{node.MemberName}' of non-struct type '{objectType.GetString(0)}'.", node.Location);
    }

    public void Visit(UnaryExpression node)
    {
        var operandType = ResolveType(node.Operand);
        if (operandType == null)
        {
            Error($"Could not resolve type of operand in unary expression.", node.Operand.Location);
            return;
        }
        
        if (operandType is not PrimaryType or PointerType)
        {
            Error($"Unary operator '{node.Operator}' cannot be applied to type '{operandType.GetString(0)}'.", node.Location);
        }
    }

    public void Visit(MethodCallExpression node)
    {
        var objectType = ResolveType(node.Method);
        if (objectType == null)
        {
            Error($"Could not resolve type of object in method call expression.", node.Method.Location);
            return;
        }
        
        if (objectType is not FunctionType functionType)
        {
            Error($"Cannot call non-function type '{objectType.GetString(0)}'.", node.Location);
            return;
        }
        
        if (functionType.ParameterTypes.Count != node.Arguments.Count)
        {
            Error($"Function expects {functionType.ParameterTypes.Count} argument(s), got {node.Arguments.Count}.", node.Location);
            return;
        }
        
        for (int i = 0; i < node.Arguments.Count; i++)
        {
            var argumentType = ResolveType(node.Arguments[i]);
            if (argumentType == null)
            {
                Error($"Could not resolve type of argument {i + 1} in method call expression.", node.Arguments[i].Location);
                continue;
            }
            
            if (!AreTypesCompatible(functionType.ParameterTypes[i], argumentType))
            {
                Error($"Type of argument {i + 1} in method call expression is not compatible with parameter type '{functionType.ParameterTypes[i].GetString(0)}'.", node.Arguments[i].Location);
            }
        }
        
        // Success, the method call expression is valid, so we don't need to do any further checks here.
    }

    public void Visit(IfStatement node)
    {
        // First visit the conditional
        var conditionType = ResolveType(node.MainBranch.Condition);
        if (conditionType == null)
        {
            Error($"Could not resolve type of condition in if statement.", node.MainBranch.Condition.Location);
        }

        conditionType!.Accept(this);
        
        // Then visit the main branch
        Visit(node.MainBranch.Body);
        
        // Then visit the else-if branches
        foreach (var elseIfBranch in node.ElseIfBranches)
        {
            var elseIfConditionType = ResolveType(elseIfBranch.Condition);
            if (elseIfConditionType == null)
            {
                Error($"Could not resolve type of condition in else-if branch of if statement.",
                    elseIfBranch.Condition.Location);
            }

            elseIfConditionType!.Accept(this);
            Visit(elseIfBranch.Body);
        }
        
        // Finally visit the else branch, if it exists
        if (node.ElseBranch != null)
        {
            Visit(node.ElseBranch);
        }
    }

    public void Visit(WhileStatement node)
    {
        var conditionType = ResolveType(node.Condition);
        if (conditionType == null)
        {
            Error($"Could not resolve type of condition in while statement.", node.Condition.Location);
        }

        conditionType?.Accept(this);
        Visit(node.Body);
    }

    public void Visit(ForStatement node)
    {
        _scopeStack.Push(new Dictionary<string, TypeNode>());
        if (node.Initializer != null)
        {
            node.Initializer.Accept(this);
        }
        
        if (node.Condition != null)
        {
            var conditionType = ResolveType(node.Condition);
            if (conditionType == null)
            {
                Error($"Could not resolve type of condition in for statement.", node.Condition.Location);
            }
            else
            {
                conditionType.Accept(this);
            }
        }
        
        if (node.Increment != null)
        {
            node.Increment.Accept(this);
        }
        
        Visit(node.Body);
        _scopeStack.Pop();
    }

    public void Visit(BreakStatement node)
    {
        
    }

    public void Visit(ContinueStatement node)
    {
        
    }

    private Dictionary<string, TypeNode> BuildSubstitutions(FunctionInfo function, FunctionCallExpression call)
    {
        var substitutions = new Dictionary<string, TypeNode>();

        if (call.GenericArgs.Count != function.Generics.Count)
        {
            Error(
                $"Function '{call.Identifier}' expects {function.Generics.Count} generic argument(s), got {call.GenericArgs.Count}.",
                call.Location);
            return substitutions;
        }

        for (int i = 0; i < function.Generics.Count; i++)
        {
            var resolved = ResolveType(call.GenericArgs[i]);
            if (resolved != null)
                substitutions[function.Generics[i].Name] = resolved;
        }

        return substitutions;
    }
    
    private Dictionary<string, TypeNode> BuildSubstitutions(StructInfo structInfo, StructInitializerExpression initializer)
    {
        var substitutions = new Dictionary<string, TypeNode>();

        if (initializer.GenericArguments.Count != structInfo.Generics.Count)
        {
            Error(
                $"Struct '{structInfo.Name}' expects {structInfo.Generics.Count} generic argument(s), got {initializer.GenericArguments.Count}.",
                initializer.Location);
            return substitutions;
        }

        for (int i = 0; i < structInfo.Generics.Count; i++)
        {
            var resolved = ResolveType(initializer.GenericArguments[i]);
            if (resolved != null)
                substitutions[structInfo.Generics[i].Name] = resolved;
        }

        return substitutions;
    }
    
    private TypeNode SubstituteGenerics(TypeNode type, Dictionary<string, TypeNode> substitutions)
    {
        if (type is GenericType genericType && substitutions.TryGetValue(genericType.Name, out var substituted))
            return substituted;

        if (type is StructType structType)
        {
            // Substitute into each generic argument of the struct type, e.g. Vector3<T> -> Vector3<F32>
            var substitutedFields = structType.Fields
                .Select(f => new VariableDeclaration(f.Name, SubstituteGenerics(f.Type!, substitutions), new(), f.Initializer, f.Location))
                .ToList();
            return new StructType(structType.Name, substitutedFields);
        }

        if (type is PointerType pointerType)
            return new PointerType( SubstituteGenerics(pointerType.BaseType, substitutions));

        if (type is FunctionType functionType)
        {
            var substitutedReturn = SubstituteGenerics(functionType.ReturnType, substitutions);
            var substitutedParams = functionType.ParameterTypes
                .Select(p => SubstituteGenerics(p, substitutions))
                .ToList();
            
            return new FunctionType( substitutedParams, substitutedReturn,
                functionType.IsVariadic, functionType.IsExternal, functionType.IsExtensionMethod);
        }
        
        if (type is NamedType namedType)
        {
            var substitutedArgs = namedType.TypeArguments
                .Select(arg => SubstituteGenerics(arg, substitutions))
                .ToList();
            
            return new NamedType(namedType.Parts, substitutedArgs);
        }

        return type; // Primitives, void, etc. pass through unchanged
    }
}