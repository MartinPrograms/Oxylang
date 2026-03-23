namespace Oxylang.Systems.Parsing.Nodes;

public class ContinueStatement : Node
{
    public ContinueStatement(SourceLocation location) : base(location)
    {
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        return $"{indent}ContinueStatement";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}