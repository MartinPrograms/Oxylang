namespace Oxylang.Systems.Parsing.Nodes;

public class LiteralExpression : Expression
{
    public ulong IntegerValue { get; }
    public double FloatValue { get; }
    public TypeNode Type { get; }
    
    public LiteralExpression(SourceLocation location, ulong integerValue, TypeNode type) : base(location)
    {
        IntegerValue = integerValue;
        Type = type;
    }
    
    public LiteralExpression(SourceLocation location, double floatValue, TypeNode type) : base(location)
    {
        FloatValue = floatValue;
        Type = type;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}LiteralExpression: ";
        if (Type is PrimaryType primaryType)
        {
            if (primaryType.Kind == PrimaryType.PrimaryTypeKind.F32 || primaryType.Kind == PrimaryType.PrimaryTypeKind.F64)
            {
                result += FloatValue + " (floating-point)";
            }
            else
            {
                result += IntegerValue + " (integer)";
            }
        }
        else if (Type is PointerType pointerType)
        {
            result += IntegerValue + " (pointer to " + pointerType.BaseType.GetString(0) + ")";
        }
        else if (Type is NamedType namedType)
        {
            result += IntegerValue + " (named type " + string.Join("::", namedType.Parts) + ")";
        }
        
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}