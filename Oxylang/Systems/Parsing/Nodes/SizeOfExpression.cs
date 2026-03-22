namespace Oxylang.Systems.Parsing.Nodes;

public class SizeOfExpression : Expression
{
    public TypeNode Type { get; }
    
    public SizeOfExpression(SourceLocation location, TypeNode Type) : base(location)
    {
        this.Type = Type;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}SizeOfExpression:\n{Type.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}