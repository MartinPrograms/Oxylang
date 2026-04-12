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
    public TypeNode() : base(new (0,0))
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
    
    // Override the == operator to compare types structurally
    public static bool operator ==(TypeNode? left, TypeNode? right)
    {
        if (ReferenceEquals(left, right)) return true;
        if (left is null || right is null) return false;
        return left.GetString(0) == right.GetString(0);
    }

    public static bool operator !=(TypeNode? left, TypeNode? right)
    {
        return !(left == right);
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
    
    public PrimaryType(PrimaryTypeKind kind) : base()
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
    public List<string> Parts { get; }
    public List<TypeNode> TypeArguments { get; }
    
    public NamedType(List<string> parts, List<TypeNode> typeArguments) : base()
    {
        Parts = parts;
        TypeArguments = typeArguments;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}NamedType: {string.Join("::", Parts)}\n";
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
    
    public PointerType(TypeNode baseType) : base()
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
    
    public ArrayType(TypeNode elementType, int length) : base()
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
    
    public StructType(string name, IReadOnlyList<VariableDeclaration> fields) : base()
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
    public bool IsExternal { get; }
    public bool IsExtensionMethod { get; }

    public FunctionType(IReadOnlyList<TypeNode> parameterTypes, TypeNode returnType, bool isVariadic, bool isExternal, bool isExtensionMethod) : base()
    {
        ParameterTypes = parameterTypes;
        ReturnType = returnType;
        IsVariadic = isVariadic;
        IsExternal = isExternal;
        IsExtensionMethod = isExtensionMethod;
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
        result += $"{indent}    IsExternal: {IsExternal}";
        result += $"{indent}    IsExtensionMethod: {IsExtensionMethod}";
        return result;
    }
}

public class VoidType : TypeNode
{
    public VoidType() : base()
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
    
    public GenericType(string name) : base()
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
    
    public ModuleType(CompilationUnit unit) : base()
    {
        Unit = unit;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}ModuleType: {Unit.Identifier}";
    }
}