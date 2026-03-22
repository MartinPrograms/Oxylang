namespace Oxylang.Systems.Parsing.Nodes;

public class CastExpression : Expression
{
    public TypeNode TargetType { get; }
    public Expression Expression { get; }
    
    public CastExpression(SourceLocation location, TypeNode targetType, Expression expression) : base(location)
    {
        TargetType = targetType;
        Expression = expression;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}CastExpression:\n{TargetType.GetString(depth + 1)}\n{Expression.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}