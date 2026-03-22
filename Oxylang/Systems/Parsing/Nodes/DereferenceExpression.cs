namespace Oxylang.Systems.Parsing.Nodes;

public class DereferenceExpression : Expression
{
    public Expression Expression { get; }
    
    public DereferenceExpression(SourceLocation location, Expression expression) : base(location)
    {
        Expression = expression;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}DereferenceExpression:\n{Expression.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}