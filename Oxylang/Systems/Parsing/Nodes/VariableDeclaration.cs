namespace Oxylang.Systems.Parsing.Nodes;

public class VariableDeclaration : Node
{
    public string Name { get; }
    public TypeNode? Type { get; }
    public IReadOnlyList<Attribute> Attributes { get; }
    public Expression? Initializer { get; }
    
    public VariableDeclaration(string name, TypeNode? type, List<Attribute> attributes, Expression? initializer, SourceLocation location) : base(location)
    {
        Name = name;
        Type = type;
        Attributes = attributes;
        Initializer = initializer;
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}VariableDeclaration: {Name}\n";
        
        if (Type != null)
            result += $"{indent}    Type:\n{Type?.GetString(depth + 2)}\n";
        
        if (Attributes.Count > 0)
        {
            result += $"{indent}    Attributes:\n";
            foreach (var attr in Attributes)
            {
                result += $"{indent}        {attr.Name}({string.Join(", ", attr.Arguments)})\n";
            }
        }
        
        if (Initializer != null)
        {
            result += $"{indent}    Initializer:\n{Initializer.GetString(depth + 2)}\n";
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}