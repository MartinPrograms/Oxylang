namespace Oxylang.Systems.Parsing.Nodes;

public class WhileStatement : Node
{
    public Expression Condition { get; }
    public BlockStatement Body { get; }
    
    public WhileStatement(SourceLocation location, Expression condition, BlockStatement body) : base(location)
    {
        Condition = condition;
        Body = body;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}WhileStatement:\n{indent}Condition:\n{Condition.GetString(depth + 1)}\n{indent}Body:\n{Body.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}