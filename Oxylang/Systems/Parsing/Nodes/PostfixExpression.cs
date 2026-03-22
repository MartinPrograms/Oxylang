namespace Oxylang.Systems.Parsing.Nodes;

public class PostfixExpression : Expression
{
    public Expression Primary { get; }
    public Language.Operator Operator { get; }

    public PostfixExpression(SourceLocation location, Expression primary, Language.Operator @operator) : base(location)
    {
        Primary = primary;
        Operator = @operator;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}PostfixExpression: {Operator}\n";
        result += Primary.GetString(depth + 1) + "\n";
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}