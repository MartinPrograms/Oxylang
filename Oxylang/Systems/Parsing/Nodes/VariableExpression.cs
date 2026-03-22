namespace Oxylang.Systems.Parsing.Nodes;

public class VariableExpression : LeftValue
{
    public string Name { get; }
    
    public VariableExpression(SourceLocation location, string name) : base(location)
    {
        Name = name;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}VariableExpression: {Name}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}