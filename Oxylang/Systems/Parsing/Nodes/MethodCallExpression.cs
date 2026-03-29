namespace Oxylang.Systems.Parsing.Nodes;

// Represents a call to an imported method
public class MethodCallExpression : Expression
{
    public MemberAccessExpression Method { get; }
    public List<Expression> Arguments { get; }
    
    public MethodCallExpression(SourceLocation location, MemberAccessExpression method, List<Expression> arguments) : base(location)
    {
        Method = method;
        Arguments = arguments;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}MethodCallExpression:\n";
        result += $"{indent}    Method:\n{Method.GetString(depth + 2)}\n";
        if (Arguments.Count > 0)
        {
            result += $"{indent}    Arguments:\n";
            foreach (var arg in Arguments)
            {
                result += $"{arg.GetString(depth + 2)}\n";
            }
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}