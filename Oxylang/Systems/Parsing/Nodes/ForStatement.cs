namespace Oxylang.Systems.Parsing.Nodes;

public class ForStatement : Node
{
    public Node? Initializer { get; }
    public Expression? Condition { get; }
    public Expression? Increment { get; }
    public BlockStatement Body { get; }
    
    public ForStatement(SourceLocation location, Node? initializer, Expression? condition, Expression? increment, BlockStatement body) : base(location)
    {
        Initializer = initializer;
        Condition = condition;
        Increment = increment;
        Body = body;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}ForStatement:\n";
        if (Initializer != null)
        {
            result += $"{indent}Initializer:\n{Initializer.GetString(depth + 1)}\n";
        }
        if (Condition != null)
        {
            result += $"{indent}Condition:\n{Condition.GetString(depth + 1)}\n";
        }
        if (Increment != null)
        {
            result += $"{indent}Increment:\n{Increment.GetString(depth + 1)}\n";
        }
        result += $"{indent}Body:\n{Body.GetString(depth + 1)}";
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}