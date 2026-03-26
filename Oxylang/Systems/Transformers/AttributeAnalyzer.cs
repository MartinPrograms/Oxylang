using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;
using Attribute = Oxylang.Systems.Parsing.Attribute;

namespace Oxylang.Systems.Transformers;

public record AttributeAnalyzerResult(bool IsSuccess);

record AttributeDefinition(string Name);

public class AttributeAnalyzer(ILogger _logger, SourceFile _sourceFile) : ITransformer<AttributeAnalyzerResult>
{
    private readonly List<AttributeDefinition> _attributes =
    [
        new("entry"), // define entry point
        new("export"), // export symbol
        new("extern"), // declare external symbol (linked from another library, like a C function from libc)
        new("symbol"), // define symbol name (overrides name mangling)
        new("public"), // make symbol public (exported and visible to other modules, but not exported in the final binary)
        new("callconv"), // specify calling convention (e.g. cdecl, stdcall, fastcall)
    ];
    
    private void VerifyAttributes(IReadOnlyList<Attribute> nodeAttributes, Node node)
    {
        foreach (var attr in nodeAttributes)
        {
            if (!_attributes.Any(a => a.Name == attr.Name))
            {
                Fail($"Unknown attribute '{attr.Name}' on {node.GetType().Name} at {node.Location}");
            }
        }

        if (node is FunctionDefinition funcDef)
        {
            // Check if the function has both 'entry' and 'export' attributes, which is not allowed
            if (nodeAttributes.Any(a => a.Name == "entry") && nodeAttributes.Any(a => a.Name == "export"))
            {
                Fail($"Function '{funcDef.Name}' cannot have both 'entry' and 'export' attributes at {node.Location}");
            }
        }

        if (node is StructDefinition structDef)
        {
            // Check if the struct has the 'export', 'entry', 'extern', 'callconv', or 'symbol' attributes, which are not allowed
            if (nodeAttributes.Any(a => a.Name == "export") || nodeAttributes.Any(a => a.Name == "extern") ||
                nodeAttributes.Any(a => a.Name == "callconv") || nodeAttributes.Any(a => a.Name == "symbol") || 
                nodeAttributes.Any(x=> x.Name == "entry"))
            {
                Fail(
                    $"Struct '{structDef.Name}' cannot have 'export', 'entry', 'extern', 'callconv', or 'symbol' attributes at {node.Location}");
            }
        }

        if (node is VariableDeclaration varDecl)
        {
            // Check if the variable has the 'entry', 'callconv', or 'symbol' attributes, which are not allowed
            if (nodeAttributes.Any(a => a.Name == "entry") || nodeAttributes.Any(a => a.Name == "callconv") ||
                nodeAttributes.Any(a => a.Name == "symbol"))
            {
                Fail(
                    $"Variable '{varDecl.Name}' cannot have 'entry', 'callconv', or 'symbol' attributes at {node.Location}");
            }
        }
    }
    
    private bool _success = true;

    private void Fail(string message)
    {
        _logger.LogError(message, _sourceFile, new(0, 0));
        _success = false;
    }
    
    public AttributeAnalyzerResult Transform(Root node)
    {
        node.Accept(this);
        return new AttributeAnalyzerResult(_success);
    }

    public void Visit(Root node)
    {
        foreach (var structDef in node.Structs)
        {
            structDef.Accept(this);
        }
        
        foreach (var funcDef in node.Functions)
        {
            funcDef.Accept(this);
        }
        
        foreach (var globalVar in node.GlobalVariables)
        {
            globalVar.Accept(this);
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
        VerifyAttributes(node.Attributes, node);
    }

    public void Visit(ImportStatement node)
    {
        
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
        VerifyAttributes(node.Attributes, node);
    }

    public void Visit(StructInitializerExpression node)
    {
        
    }

    public void Visit(VariableDeclaration node)
    {
        VerifyAttributes(node.Attributes, node);
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