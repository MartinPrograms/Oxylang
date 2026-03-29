using System.Text;

namespace Oxylang.Systems.Parsing.Nodes;

public class ImportStatement : Node
{
    public string Path { get; private set; }
    public string Alias { get; }
    public List<Attribute> Attributes { get; }

    public ImportStatement(SourceLocation location, string path, string alias, List<Attribute> attributes) : base(location)
    {
        Path = path;
        Alias = alias;
        Attributes = attributes;
    }

    public override string GetString(int depth)
    {
        StringBuilder sb = new();
        sb.AppendLine("ImportStatement: ");
        sb.AppendLine($"{new string('-', 4)} Path: {Path}");
        sb.AppendLine($"{new string('-', 4)} Alias: {Alias}");
        if (Attributes.Count > 0)
        {
            sb.AppendLine($"{new string('-', 4)} Attributes:");
            foreach (var attr in Attributes)
            {
                sb.AppendLine($"{new string('-', 8)} {attr.Name}({string.Join(", ", attr.Arguments)})");
            }
        }
        return sb.ToString();
    }
    
    internal void UpdatePath(string newPath)
    {
        Path = newPath;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}