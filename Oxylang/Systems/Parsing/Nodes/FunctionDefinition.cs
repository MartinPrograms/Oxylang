namespace Oxylang.Systems.Parsing.Nodes;

public class FunctionDefinition : Node
{
    public string Name { get; }
    public IReadOnlyList<VariableDeclaration> Parameters { get; }
    public IReadOnlyList<Attribute> Attributes { get; }
    public IReadOnlyList<GenericType> GenericTypes { get; }
    public TypeNode ReturnType { get; }
    public BlockStatement? Body { get; }
    public bool IsVariadic { get; }
    public FunctionType FunctionType => new FunctionType(Location, Parameters.Select(x => x.Type!).ToList(), ReturnType, IsVariadic);
    public FunctionDefinition(SourceLocation location, string name, IReadOnlyList<VariableDeclaration> parameters, IReadOnlyList<Attribute> attributes, IReadOnlyList<GenericType> genericTypes, TypeNode returnType, BlockStatement? body, bool isVariadic) : base(location)
    {
        Name = name;
        Parameters = parameters;
        Attributes = attributes;
        GenericTypes = genericTypes;
        ReturnType = returnType;
        Body = body;
        IsVariadic = isVariadic;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}FunctionDefinition: {Name}\n";
        if (Parameters.Count > 0)
        {
            result += $"{indent}    Parameters:\n";
            foreach (var param in Parameters)
            {
                result += param.GetString(depth + 2) + "\n";
            }
        }
        result += $"{indent}    ReturnType:\n{ReturnType.GetString(depth + 2)}\n";
        
        if (Body != null)
            result += $"{indent}    Body:\n{Body.GetString(depth + 2)}\n";
        
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
        
        if (IsVariadic)
        {
            result += $"{indent}    IsVariadic: true\n";
        }
        
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}