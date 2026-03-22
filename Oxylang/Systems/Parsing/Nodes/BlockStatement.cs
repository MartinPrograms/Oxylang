namespace Oxylang.Systems.Parsing.Nodes;

public class BlockStatement : Node
{
    public IReadOnlyList<Node> Statements { get; }

    public BlockStatement(SourceLocation location, IReadOnlyList<Node> statements) : base(location)
    {
        Statements = statements;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}BlockStatement:\n";
        foreach (var statement in Statements)
        {
            result += statement.GetString(depth + 1) + "\n";
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}