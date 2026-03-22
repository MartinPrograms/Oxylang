namespace Oxylang.Systems.Parsing.Nodes;

public class AssignmentExpression : Expression
{
    public LeftValue Left { get; }
    public Expression Right { get; }
    
    public AssignmentExpression(SourceLocation location, LeftValue left, Expression right) : base(location)
    {
        Left = left;
        Right = right;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}AssignmentStatement:\n{Left.GetString(depth + 1)}\n{Right.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}