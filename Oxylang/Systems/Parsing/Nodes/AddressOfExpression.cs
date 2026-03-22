namespace Oxylang.Systems.Parsing.Nodes;

public class AddressOfExpression : Expression
{
    public LeftValue Expression { get; }
    
    public AddressOfExpression(SourceLocation location, LeftValue expression) : base(location)
    {
        Expression = expression;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}AddressOfExpression:\n{Expression.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}