namespace Oxylang.Systems.Parsing.Nodes;

public class AlignOfExpression : Expression
{
    public TypeNode Type { get; }
    
    public AlignOfExpression(SourceLocation location, TypeNode type) : base(location)
    {
        Type = type;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}AlignOfExpression:\n{Type.GetString(depth + 1)}";
    }

    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}