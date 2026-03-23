using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Transformers;

public class DesugarTransformer : IAstTransformer
{
    public Node Visit(Root node)
    {
        List<ImportStatement> newImports = new List<ImportStatement>();
        foreach (var import in node.Imports)
        {
            var newImport = (ImportStatement)import.Visit(this);
            newImports.Add(newImport);
        }
        
        List<StructDefinition> newStructs = new List<StructDefinition>();
        foreach (var structDef in node.Structs)
        {
            var newStruct = (StructDefinition)structDef.Visit(this);
            newStructs.Add(newStruct);
        }
        
        List<FunctionDefinition> newFunctions = new List<FunctionDefinition>();
        foreach (var function in node.Functions)
        {
            var newFunction = (FunctionDefinition)function.Visit(this);
            newFunctions.Add(newFunction);
        }
        
        List<VariableDeclaration> newGlobals = new List<VariableDeclaration>();
        foreach (var global in node.GlobalVariables)
        {
            var newGlobal = (VariableDeclaration)global.Visit(this);
            newGlobals.Add(newGlobal);
        }

        return new Root(newImports, newStructs, newGlobals, newFunctions);
    }

    public Node Visit(AlignOfExpression node)
    {
        return node;
    }

    public Node Visit(AssignmentExpression node)
    {
        return node;
    }

    public Node Visit(BinaryExpression node)
    {
        // Check if compound assignment operator, if so desugar it to a normal assignment with the corresponding binary operator
        if (node.Operator == Language.Operator.AsteriskEquals || node.Operator == Language.Operator.SlashEquals || node.Operator == Language.Operator.PercentEquals ||
            node.Operator == Language.Operator.PlusEquals || node.Operator == Language.Operator.MinusEquals)
        {
            var left = node.Left;
            var right = node.Right;
            
            Language.Operator binaryOperator = node.Operator switch
            {
                Language.Operator.AsteriskEquals => Language.Operator.Asterisk,
                Language.Operator.SlashEquals => Language.Operator.Slash,
                Language.Operator.PercentEquals => Language.Operator.Percent,
                Language.Operator.PlusEquals => Language.Operator.Plus,
                Language.Operator.MinusEquals => Language.Operator.Minus,
                _ => throw new InvalidOperationException("Invalid compound assignment operator")
            };
            
            return new AssignmentExpression(node.Location, (LeftValue) left, new BinaryExpression(node.Location, left, binaryOperator, right));
        }
        
        return node;
    }

    public Node Visit(BlockStatement node)
    {
        List<Node> newStatements = new List<Node>();
        foreach (var statement in node.Statements)
        {
            var newStatement = statement.Visit(this);
            newStatements.Add(newStatement);
        }
        return new BlockStatement(node.Location, newStatements);
    }

    public Node Visit(CastExpression node)
    {
        return node;
    }

    public Node Visit(FunctionCallExpression node)
    {
        return node;
    }

    public Node Visit(FunctionDefinition node)
    {
        BlockStatement? newBody = (BlockStatement?)node.Body?.Visit(this);
        return new FunctionDefinition(node.Location, node.Name, node.Parameters, node.Attributes, node.GenericTypes,
            node.ReturnType, newBody, node.IsVariadic);
    }

    public Node Visit(ImportStatement node)
    {
        return node;
    }

    public Node Visit(LiteralExpression node)
    {
        return node;
    }

    public Node Visit(PostfixExpression node)
    {
        return node;
    }

    public Node Visit(ReturnStatement node)
    {
        return node;
    }

    public Node Visit(SizeOfExpression node)
    {
        return node;
    }

    public Node Visit(StringExpression node)
    {
        return node;
    }

    public Node Visit(StructDefinition node)
    {
        return node;
    }

    public Node Visit(StructInitializerExpression node)
    {
        return node;
    }

    public Node Visit(VariableDeclaration node)
    {
        return node;
    }

    public Node Visit(VariableExpression node)
    {
        return node;
    }

    public Node Visit(AddressOfExpression node)
    {
        return node;
    }

    public Node Visit(DereferenceExpression node)
    {
        return node;
    }

    public Node Visit(MemberAccessExpression node)
    {
        return node;
    }

    public Node Visit(IfStatement node)
    {
        // Desugar if statements with else if branches into nested if statements
        BlockStatement elseBranch = node.ElseBranch ?? new BlockStatement(node.Location, new List<Node>());
        foreach (var elseIfBranch in node.ElseIfBranches.AsEnumerable().Reverse())
        {
            elseBranch = new BlockStatement(node.Location, new List<Node>
            {
                new IfStatement(node.Location, elseIfBranch, [], elseBranch)
            });
        }
        
        return new IfStatement(node.Location, node.MainBranch, [], elseBranch);
    }

    public Node Visit(WhileStatement node)
    {
        return node;
    }

    public Node Visit(ForStatement node)
    {
        return node;
    }

    public Node Visit(BreakStatement node)
    {
        return node;
    }

    public Node Visit(ContinueStatement node)
    {
        return node;
    }
}