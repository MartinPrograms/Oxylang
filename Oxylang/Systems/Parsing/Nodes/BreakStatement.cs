namespace Oxylang.Systems.Parsing.Nodes;

public class BreakStatement : Node
{
    public BreakStatement(SourceLocation location) : base(location)
    {
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}BreakStatement";
    }

    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}