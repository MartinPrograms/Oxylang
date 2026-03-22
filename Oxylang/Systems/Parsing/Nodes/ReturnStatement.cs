namespace Oxylang.Systems.Parsing.Nodes;

public class ReturnStatement : Node
{
    public Expression? Expression { get; set; }
    
    public ReturnStatement(SourceLocation location, Expression? expression) : base(location)
    {
        Expression = expression;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}ReturnStatement:\n";
        if (Expression != null)
        {
            result += Expression.GetString(depth + 1) + "\n";
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}