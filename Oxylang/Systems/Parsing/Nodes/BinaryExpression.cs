namespace Oxylang.Systems.Parsing.Nodes;

public class BinaryExpression : Expression
{
    public Expression Left { get; }
    public Language.Operator Operator { get; }
    public Expression Right { get; }

    public BinaryExpression(SourceLocation location, Expression left, Language.Operator @operator, Expression right) : base(location)
    {
        Left = left;
        Operator = @operator;
        Right = right;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}BinaryExpression: {Operator}\n{Left.GetString(depth + 1)}\n{Right.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}