namespace Oxylang.Systems.Parsing.Nodes;

public class StructInitializerExpression : Expression
{
    public List<string> StructName { get; }
    public List<TypeNode> GenericArguments { get; }
    public List<(string, Expression)> FieldInitializers { get; }
    
    public StructInitializerExpression(SourceLocation location, List<string> structName, List<TypeNode> genericArguments, List<(string, Expression)> fieldInitializers) : base(location)
    {
        StructName = structName;
        GenericArguments = genericArguments;
        FieldInitializers = fieldInitializers;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}StructInitializerExpression: {StructName}\n";
        if (GenericArguments.Count > 0)
        {
            result += $"{indent}Generic Arguments:\n";
            foreach (var arg in GenericArguments)
            {
                result += $"{indent}- {arg}\n";
            }
        }
        if (FieldInitializers.Count > 0)
        {
            result += $"{indent}Field Initializers:\n";
            foreach (var (field, initializer) in FieldInitializers)
            {
                result += $"{indent}- {field}:\n {initializer.GetString(depth + 1)}\n";
            }
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}