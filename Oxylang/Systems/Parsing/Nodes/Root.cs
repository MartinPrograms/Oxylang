namespace Oxylang.Systems.Parsing.Nodes;

public class Root : Node
{
    public IReadOnlyList<ImportStatement> Imports { get; }
    public IReadOnlyList<StructDefinition> Structs { get; }
    public IReadOnlyList<VariableDeclaration> GlobalVariables { get; }
    public IReadOnlyList<FunctionDefinition> Functions { get; }
    
    public Root(IReadOnlyList<ImportStatement> imports, IReadOnlyList<StructDefinition> structs, IReadOnlyList<VariableDeclaration> globalVariables, IReadOnlyList<FunctionDefinition> functions) : base(new SourceLocation(0, 0))
    {
        Imports = imports;
        Structs = structs;
        GlobalVariables = globalVariables;
        Functions = functions;
    }
    
    public override string GetString(int depth)
    {
        string indent = new string('-', depth * 4);
        string result = $"{indent}Root:\n";
        if (Imports.Count > 0)
        {
            result += $"{indent}    Imports:\n";
            foreach (var import in Imports)
            {
                result += import.GetString(depth + 2) + "\n";
            }
        }
        if (Structs.Count > 0)
        {
            result += $"{indent}    Structs:\n";
            foreach (var structDef in Structs)
            {
                result += structDef.GetString(depth + 2) + "\n";
            }
        }
        if (GlobalVariables.Count > 0)
        {
            result += $"{indent}    GlobalVariables:\n";
            foreach (var varDecl in GlobalVariables)
            {
                result += varDecl.GetString(depth + 2) + "\n";
            }
        }
        if (Functions.Count > 0)
        {
            result += $"{indent}    Functions:\n";
            foreach (var funcDef in Functions)
            {
                result += funcDef.GetString(depth + 2) + "\n";
            }
        }
        return result;
    }

    public override string ToString()
    {
        return GetString(0);
    }
    
    public override void Accept(IVisitor visitor) => visitor.Visit(this);
    public override Node Visit(IAstTransformer visitor) => visitor.Visit(this);
}