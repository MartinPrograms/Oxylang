namespace Oxylang.Systems.Parsing.Nodes;

public class MemberAccessExpression : LeftValue
{
    public Expression Object { get; }
    public string MemberName { get; }
    public bool IsPointerAccess { get; }
    
    public MemberAccessExpression(SourceLocation location, Expression @object, string memberName, bool isPointerAccess) : base(location)
    {
        Object = @object;
        MemberName = memberName;
        IsPointerAccess = isPointerAccess;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string accessType = IsPointerAccess ? "->" : ".";
        return $"{indent}MemberAccessExpression: {accessType}{MemberName}\n{Object.GetString(depth + 1)}";
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}