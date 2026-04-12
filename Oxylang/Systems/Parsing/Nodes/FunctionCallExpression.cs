namespace Oxylang.Systems.Parsing.Nodes;

public class FunctionCallExpression : Expression
{
    public List<string> Identifier { get; }
    public List<TypeNode> GenericArgs { get; }
    public List<Expression> Arguments { get; }
    
    public FunctionCallExpression(SourceLocation currentLocation, List<string> identifier, List<TypeNode> genericArgs, List<Expression> arguments) : base(currentLocation)
    {
        Identifier = identifier;
        GenericArgs = genericArgs;
        Arguments = arguments;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}FunctionCallExpression: {Identifier}\n";
        if (GenericArgs.Count > 0)
        {
            result += $"{indent}Generic Arguments:\n";
            foreach (var genericArg in GenericArgs)
            {
                result += genericArg.GetString(depth + 2) + "\n";
            }
        }
        if (Arguments.Count > 0)
        {
            result += $"{indent}Arguments:\n";
            foreach (var argument in Arguments)
            {
                result += argument.GetString(depth + 2) + "\n";
            }
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}