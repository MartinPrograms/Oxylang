using System.Text;

namespace Oxylang.Systems.Parsing.Nodes;

public class ImportStatement : Node
{
    public string Path { get; }
    public string Alias { get; }

    public ImportStatement(SourceLocation location, string path, string alias) : base(location)
    {
        Path = path;
        Alias = alias;
    }

    public override string GetString(int depth)
    {
        StringBuilder sb = new();
        sb.AppendLine("ImportStatement: ");
        sb.AppendLine($"{new string('-', 4)} Path: {Path}");
        sb.AppendLine($"{new string('-', 4)} Alias: {Alias}");
        return sb.ToString();
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}