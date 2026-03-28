namespace Oxylang.Systems.Parsing.Nodes;

public class UnaryExpression : Expression
{
    public Language.Operator Operator { get; }
    public Expression Operand { get; }
    
    public UnaryExpression(SourceLocation location, Language.Operator op, Expression operand) : base(location)
    {
        Operator = op;
        Operand = operand;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}UnaryExpression: {Operator}\n{Operand.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}