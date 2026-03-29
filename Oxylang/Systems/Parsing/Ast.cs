using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Parsing;

public abstract class Node
{
    public SourceLocation Location { get; }

    public Node(SourceLocation location)
    {
        Location = location;
    }

    public abstract string GetString(int depth);
    public abstract void Accept(IVisitor visitor);
    public abstract Node Visit(IAstTransformer visitor);
}

/// <summary>
/// Represents a node that evaluates to a value, such as a literal, variable, or expression.
/// </summary>
public abstract class Expression : Node
{
    public Expression(SourceLocation location) : base(location)
    {
    }
}

/// <summary>
/// Represents a node that can appear on the left side of an assignment, such as a variable or property access.
/// </summary>
public abstract class LeftValue : Expression
{
    public LeftValue(SourceLocation location) : base(location)
    {
    }
}

/// <summary>
/// Represents a node that defines a type, such as a struct definition or a type alias.
/// </summary>
public abstract class TypeNode : Node
{
    public TypeNode(SourceLocation location) : base(location)
    {
    }

    public override void Accept(IVisitor visitor)
    {
        // Does nothing.
    }
    
    public override Node Visit(IAstTransformer visitor)
    {
        // Does nothing.
        return this;
    }
}

/// <summary>
/// A primary type, like u8, i32 or f64.
/// </summary>
public class PrimaryType : TypeNode
{
    public enum PrimaryTypeKind
    {
        U8,
        I8,
        U16,
        I16,
        U32,
        I32,
        U64,
        I64,
        F32,
        F64
    }
    
    public PrimaryTypeKind Kind { get; }
    
    public PrimaryType(SourceLocation location, PrimaryTypeKind kind) : base(location)
    {
        Kind = kind;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}PrimaryType: {Kind}";
    }
}

public class NamedType : TypeNode
{
    public string Name { get; }
    public List<TypeNode> TypeArguments { get; }
    
    public NamedType(SourceLocation location, string name, List<TypeNode> typeArguments) : base(location)
    {
        Name = name;
        TypeArguments = typeArguments;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}NamedType: {Name}\n";
        if (TypeArguments.Count > 0)
        {
            result += $"{indent}    TypeArguments:\n";
            foreach (var arg in TypeArguments)
            {
                result += arg.GetString(depth + 2) + "\n";
            }
        }
        return result;
    }
}

public class PointerType : TypeNode
{
    public TypeNode BaseType { get; }
    
    public PointerType(SourceLocation location, TypeNode baseType) : base(location)
    {
        BaseType = baseType;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}PointerType:\n{BaseType.GetString(depth + 1)}";
    }
}

public class ArrayType : TypeNode
{
    public TypeNode ElementType { get; }
    public int Length { get; }
    
    public ArrayType(SourceLocation location, TypeNode elementType, int length) : base(location)
    {
        ElementType = elementType;
        Length = length;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}ArrayType (Length: {Length}):\n{ElementType.GetString(depth + 1)}";
    }
}

public class StructType : TypeNode
{
    public string Name { get; }
    public IReadOnlyList<VariableDeclaration> Fields { get; }
    
    public StructType(SourceLocation location, string name, IReadOnlyList<VariableDeclaration> fields) : base(location)
    {
        Name = name;
        Fields = fields;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}StructType: {Name}\n";
        foreach (var field in Fields)
        {
            result += field.GetString(depth + 1) + "\n";
        }
        return result;
    }
}

public class FunctionType : TypeNode
{
    public IReadOnlyList<TypeNode> ParameterTypes { get; }
    public TypeNode ReturnType { get; }
    public bool IsVariadic { get; }
    
    public FunctionType(SourceLocation location, IReadOnlyList<TypeNode> parameterTypes, TypeNode returnType, bool isVariadic) : base(location)
    {
        ParameterTypes = parameterTypes;
        ReturnType = returnType;
        IsVariadic = isVariadic;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}FunctionType:\n";
        if (ParameterTypes.Count > 0)
        {
            result += $"{indent}    Parameters:\n";
            foreach (var param in ParameterTypes)
            {
                result += param.GetString(depth + 2) + "\n";
            }
        }
        result += $"{indent}    ReturnType:\n{ReturnType.GetString(depth + 2)}\n";
        result += $"{indent}    IsVariadic: {IsVariadic}";
        return result;
    }
}

public class VoidType : TypeNode
{
    public VoidType(SourceLocation location) : base(location)
    {
    }   
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}VoidType";
    }
}

public class GenericType : TypeNode
{
    public string Name { get; }
    
    public GenericType(SourceLocation location, string name) : base(location)
    {
        Name = name;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}GenericTypeParameter: {Name}";
    }
}

public class ModuleType : TypeNode
{
    public CompilationUnit? Unit { get; } 
    
    public ModuleType(SourceLocation location, CompilationUnit unit) : base(location)
    {
        Unit = unit;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}ModuleType: {Unit.Identifier}";
    }
}