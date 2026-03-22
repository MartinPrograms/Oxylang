namespace Oxylang.Systems.Parsing.Nodes;

public class StringExpression : Expression
{
    public string Value { get; }
    public StringExpression(SourceLocation location, string value) : base(location)
    {
        Value = value;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}StringExpression: \"{Value}\"";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}