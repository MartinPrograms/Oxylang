namespace Oxylang.Systems.Parsing.Nodes;

public class StructDefinition : Node
{
    public string Name { get; }
    public IReadOnlyList<Attribute> Attributes { get; }
    public IReadOnlyList<GenericType> GenericTypes { get; }
    public IReadOnlyList<VariableDeclaration> Fields { get; }
    public StructType StructType { get; }
    
    public StructDefinition(string name, IReadOnlyList<Attribute> attributes, IReadOnlyList<GenericType> genericTypes, IReadOnlyList<VariableDeclaration> fields, SourceLocation location) : base(location)
    {
        Name = name;
        Attributes = attributes;
        GenericTypes = genericTypes;
        Fields = fields;
        StructType = new StructType(location, name, fields);
    }

    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}StructDefinition: {Name}\n";
        if (Attributes.Count > 0)
        {
            result += $"{indent}    Attributes:\n";
            foreach (var attr in Attributes)
            {
                result += $"{indent}        {attr.Name}({string.Join(", ", attr.Arguments)})\n";
            }
        }
        foreach (var generic in GenericTypes)
        {
            result += $"{indent}    GenericType: {generic.Name}\n";
        }
        if (Fields.Count > 0)
        {
            result += $"{indent}    Fields:\n";
            foreach (var field in Fields)
            {
                result += field.GetString(depth + 2) + "\n";
            }
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}