using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Parsing;

public class StatementParser : IParser<Node?>
{
    private Parser _parser;
    
    public Node? Parse(Parser parser)
    {
        _parser = parser;
        
        if (parser.MatchKeyword(Language.Keyword.Let) != null)
        {
            return ParseVariableDeclaration();
        }
        
        if (parser.MatchKeyword(Language.Keyword.Return) != null)
        {
            return ParseReturnStatement();
        }
        
        // As a last resort, try to parse an expression statement. This allows for function calls and other expressions to be used as statements.
        return ParseExpressionStatement();
    }

    private Node? ParseExpressionStatement()
    {
        var expressionParser = _parser.ParseExpression();
        if (expressionParser == null)
        {
            _parser.Logger.LogError("Failed to parse expression statement", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        if (_parser.MatchSyntax(Language.Syntax.Semicolon) == null)
        {
            _parser.Logger.LogError("Expected ';' after expression statement", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        return expressionParser;
    }

    private Node? ParseReturnStatement()
    {
        if (_parser.MatchSyntax(Language.Syntax.Semicolon) != null)
        {
            return new ReturnStatement(_parser.CurrentLocation, null);
        }

        var expressionParser = _parser.ParseExpression();
        if (expressionParser == null)
        {
            _parser.Logger.LogError("Failed to parse return expression", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        if (_parser.MatchSyntax(Language.Syntax.Semicolon) == null)
        {
            _parser.Logger.LogError("Expected ';' after return statement", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        return new ReturnStatement(_parser.CurrentLocation, expressionParser);
    }

    private Node? ParseVariableDeclaration()
    {
        var parsed = _parser.ParseVariableDeclaration();
        if (parsed == null)
        {
            _parser.Logger.LogError("Failed to parse variable declaration", _parser.SourceFile, _parser.CurrentLocation);
        }
        
        if (_parser.MatchSyntax(Language.Syntax.Semicolon) == null)
        {
            _parser.Logger.LogError("Expected ';' after variable declaration", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        return parsed;
    }
}