using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Transformers;

public record RelianceBuilderResult(bool IsSuccess, List<Reliance> Reliances, List<Export> Exports);

public class RelianceBuilder(ILogger _logger, SourceFile _sourceFile) : ITransformer<RelianceBuilderResult>
{
    private List<Reliance> _reliances = new();
    private List<Export> _exports = new();
    
    public RelianceBuilderResult Transform(Root node)
    {
        node.Accept(this);
        return new RelianceBuilderResult(true, _reliances, _exports);
    }

    public void Visit(Root node)
    {
        foreach (var statement in node.GlobalVariables)
        {
            statement.Accept(this);
        }
        
        foreach (var statement in node.Functions)
        {
            statement.Accept(this);
        }
        
        foreach (var statement in node.Structs)
        {
            statement.Accept(this);
        }
        
        foreach (var statement in node.Imports)
        {
            statement.Accept(this);
        }
    }

    public void Visit(AlignOfExpression node)
    {
        
    }

    public void Visit(AssignmentExpression node)
    {
        
    }

    public void Visit(BinaryExpression node)
    {
        
    }

    public void Visit(BlockStatement node)
    {
        
    }

    public void Visit(CastExpression node)
    {
        
    }

    public void Visit(FunctionCallExpression node)
    {
        
    }

    public void Visit(FunctionDefinition node)
    {
        if (node.Attributes.Any(x => x.Name == "public"))
        {
            _exports.Add(new Export(node.Name, node.FunctionType, node));
        }
    }

    public void Visit(ImportStatement node)
    {
        var path = node.Path;
        if (!Path.IsPathFullyQualified(path) && path.EndsWith(".oxy"))
        {
            path = Path.Combine(Environment.CurrentDirectory, path);
        }

        node.UpdatePath(path);
        
        _reliances.Add(new Reliance(path, node.Alias));
        
        if (node.Attributes.Any(x => x.Name == "public"))
        {
            _exports.Add(new Export(node.Alias, new ModuleType(node.Location, null), node));
        }
    }

    public void Visit(LiteralExpression node)
    {
        
    }

    public void Visit(PostfixExpression node)
    {
        
    }

    public void Visit(ReturnStatement node)
    {
        
    }

    public void Visit(SizeOfExpression node)
    {
        
    }

    public void Visit(StringExpression node)
    {
        
    }

    public void Visit(StructDefinition node)
    {
        if (node.Attributes.Any(x => x.Name == "public"))
        {
            _exports.Add(new Export(node.Name, node.StructType, node));
        }
    }

    public void Visit(StructInitializerExpression node)
    {
        
    }

    public void Visit(VariableDeclaration node)
    {
        if (node.Attributes.Any(x => x.Name == "public"))
        {
            if (node.Type == null)
            {
                _logger.LogError("Public variable declaration must have an explicit type.", _sourceFile, node.Location);
                return;
            }
            
            _exports.Add(new Export(node.Name, node.Type, node));
        }
    }

    public void Visit(VariableExpression node)
    {
        
    }

    public void Visit(AddressOfExpression node)
    {
        
    }

    public void Visit(DereferenceExpression node)
    {
        
    }

    public void Visit(MemberAccessExpression node)
    {
        
    }

    public void Visit(UnaryExpression node)
    {
        
    }

    public void Visit(MethodCallExpression node)
    {
        
    }

    public void Visit(IfStatement node)
    {
        
    }

    public void Visit(WhileStatement node)
    {
        
    }

    public void Visit(ForStatement node)
    {
        
    }

    public void Visit(BreakStatement node)
    {
        
    }

    public void Visit(ContinueStatement node)
    {
        
    }
}