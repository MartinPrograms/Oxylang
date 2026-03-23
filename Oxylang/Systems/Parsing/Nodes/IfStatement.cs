namespace Oxylang.Systems.Parsing.Nodes;

public class IfStatement : Node
{
    public record Branch(Expression Condition, BlockStatement Body);
    public Branch MainBranch { get; }
    public List<Branch> ElseIfBranches { get; }
    public BlockStatement? ElseBranch { get; }
    
    public IfStatement(SourceLocation location, Branch mainBranch, List<Branch> elseIfBranches, BlockStatement? elseBranch) : base(location)
    {
        MainBranch = mainBranch;
        ElseIfBranches = elseIfBranches;
        ElseBranch = elseBranch;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}IfStatement:\n{indent}Main Branch:\n{indent}Condition:\n{MainBranch.Condition.GetString(depth + 2)}\n{indent}Body:\n{MainBranch.Body.GetString(depth + 2)}\n";
        if (ElseIfBranches.Count > 0)
        {
            result += $"{indent}Else If Branches:\n";
            foreach (var elseIfBranch in ElseIfBranches)
            {
                result += $"{indent}Condition:\n{elseIfBranch.Condition.GetString(depth + 2)}\n{indent}Body:\n{elseIfBranch.Body.GetString(depth + 2)}\n";
            }
        }
        if (ElseBranch != null)
        {
            result += $"{indent}Else Branch:\n{ElseBranch.GetString(depth + 1)}\n";
        }
        return result;
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}