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
        
        if (parser.MatchKeyword(Language.Keyword.If) != null)
        {
            return ParseIfStatement();
        }
        
        if (parser.MatchKeyword(Language.Keyword.While) != null)
        {
            return ParseWhileStatement();
        }

        if (parser.MatchKeyword(Language.Keyword.For) != null)
        {
            return ParseForStatement();
        }
        
        if (parser.MatchKeyword(Language.Keyword.Continue) != null)
        {
            if (parser.MatchSyntax(Language.Syntax.Semicolon) == null)
            {
                _parser.Logger.LogError("Expected ';' after 'continue'", _parser.SourceFile, _parser.CurrentLocation);
                return null;
            }
            
            return new ContinueStatement(_parser.CurrentLocation);
        }
        
        if (parser.MatchKeyword(Language.Keyword.Break) != null)
        {
            if (parser.MatchSyntax(Language.Syntax.Semicolon) == null)
            {
                _parser.Logger.LogError("Expected ';' after 'break'", _parser.SourceFile, _parser.CurrentLocation);
                return null;
            }
            
            return new BreakStatement(_parser.CurrentLocation);
        }
        
        // As a last resort, try to parse an expression statement. This allows for function calls and other expressions to be used as statements.
        return ParseExpressionStatement();
    }

    private Node? ParseForStatement()
    {
        Node? initializer = null;
        Expression? condition = null;
        Expression? increment = null;
        
        if (_parser.MatchSyntax(Language.Syntax.LeftParen) == null)
        {
            _parser.Logger.LogError("Expected '(' after 'for'", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        if (_parser.MatchSyntax(Language.Syntax.Semicolon) == null)
        {
            if (_parser.MatchKeyword(Language.Keyword.Let) != null)
            {
                initializer = ParseVariableDeclaration();
            }
            else
            {
                initializer = _parser.ParseExpression();
                if (initializer == null)
                {
                    _parser.Logger.LogError("Failed to parse for loop initializer", _parser.SourceFile, _parser.CurrentLocation);
                    return null;
                }
            }
        }
        
        if (_parser.MatchSyntax(Language.Syntax.Semicolon) == null)
        {
            condition = _parser.ParseExpression();
            if (condition == null)
            {
                _parser.Logger.LogError("Failed to parse for loop condition", _parser.SourceFile, _parser.CurrentLocation);
                return null;
            }
            
            if (_parser.MatchSyntax(Language.Syntax.Semicolon) == null)
            {
                _parser.Logger.LogError("Expected ';' after for loop condition", _parser.SourceFile, _parser.CurrentLocation);
                return null;
            }
        }
        
        if (_parser.MatchSyntax(Language.Syntax.RightParen) == null)
        {
            increment = _parser.ParseExpression();
            if (increment == null)
            {
                _parser.Logger.LogError("Failed to parse for loop increment", _parser.SourceFile, _parser.CurrentLocation);
                return null;
            }
            
            if (_parser.MatchSyntax(Language.Syntax.RightParen) == null)
            {
                _parser.Logger.LogError("Expected ')' after for loop increment", _parser.SourceFile, _parser.CurrentLocation);
                return null;
            }
        }
        
        if (_parser.MatchSyntax(Language.Syntax.LeftBrace) == null)
        {
            _parser.Logger.LogError("Expected '{' after for loop header", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        var bodyParser = _parser.ParseBlock();
        if (bodyParser == null)
        {
            _parser.Logger.LogError("Failed to parse for loop body", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        return new ForStatement(_parser.CurrentLocation, initializer, condition, increment, bodyParser);
    }

    private Node? ParseWhileStatement()
    {
        // we are now after the while keyword, so we expect (expr) { block }
        if (_parser.MatchSyntax(Language.Syntax.LeftParen) == null)
        {
            _parser.Logger.LogError("Expected '(' after 'while'", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        var conditionParser = _parser.ParseExpression();
        if (conditionParser == null)
        {
            _parser.Logger.LogError("Failed to parse while condition", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        if (_parser.MatchSyntax(Language.Syntax.RightParen) == null)
        {
            _parser.Logger.LogError("Expected ')' after while condition", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        if (_parser.MatchSyntax(Language.Syntax.LeftBrace) == null)
        {
            _parser.Logger.LogError("Expected '{' after 'while'", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        var bodyParser = _parser.ParseBlock();
        if (bodyParser == null)
        {
            _parser.Logger.LogError("Failed to parse while body", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        return new WhileStatement(_parser.CurrentLocation, conditionParser, bodyParser);
    }

    private Node? ParseIfStatement()
    {
        // we are now after the if keyword, so we expect (expr) { block } else or else if (expr) { block } or just else { block }
        if (_parser.MatchSyntax(Language.Syntax.LeftParen) == null)
        {
            _parser.Logger.LogError("Expected '(' after 'if'", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        var conditionParser = _parser.ParseExpression();
        if (conditionParser == null)
        {
            _parser.Logger.LogError("Failed to parse if condition", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        if (_parser.MatchSyntax(Language.Syntax.RightParen) == null)
        {
            _parser.Logger.LogError("Expected ')' after if condition", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }
        
        if (_parser.MatchSyntax(Language.Syntax.LeftBrace) == null)
        {
            _parser.Logger.LogError("Expected '{' after 'if'", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        var thenBlockParser = _parser.ParseBlock();
        if (thenBlockParser == null)
        {
            _parser.Logger.LogError("Failed to parse if then block", _parser.SourceFile, _parser.CurrentLocation);
            return null;
        }

        List<IfStatement.Branch> elseIfBranches = new List<IfStatement.Branch>();
        Node? elseBranch = null;
        while (_parser.MatchKeyword(Language.Keyword.Else) != null)
        {
            if (_parser.MatchKeyword(Language.Keyword.If) != null)
            {
                if (_parser.MatchSyntax(Language.Syntax.LeftParen) == null)
                {
                    _parser.Logger.LogError("Expected '(' after 'else if'", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }

                var elseIfConditionParser = _parser.ParseExpression();
                if (elseIfConditionParser == null)
                {
                    _parser.Logger.LogError("Failed to parse else if condition", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }
                
                if (_parser.MatchSyntax(Language.Syntax.RightParen) == null)
                {
                    _parser.Logger.LogError("Expected ')' after else if condition", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }
                
                if (_parser.MatchSyntax(Language.Syntax.LeftBrace) == null)
                {
                    _parser.Logger.LogError("Expected '{' after 'else if'", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }
                
                var elseIfBlockParser = _parser.ParseBlock();
                if (elseIfBlockParser == null)
                {
                    _parser.Logger.LogError("Failed to parse else if block", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }
                
                elseIfBranches.Add(new IfStatement.Branch(elseIfConditionParser, elseIfBlockParser));
            }
            else
            {
                if (_parser.MatchSyntax(Language.Syntax.LeftBrace) == null)
                {
                    _parser.Logger.LogError("Expected '{' after 'else'", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }
                
                var elseBlockParser = _parser.ParseBlock();
                if (elseBlockParser == null)
                {
                    _parser.Logger.LogError("Failed to parse else block", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }
                
                elseBranch = elseBlockParser;
                break; // else must be the last branch, so we break out of the loop
            }
        }

        return new IfStatement(_parser.CurrentLocation, new IfStatement.Branch(conditionParser, thenBlockParser),
            elseIfBranches, elseBranch as BlockStatement);
    }

    private Node? ParseExpressionStatement()
    {
        var expressionParser = _parser.ParseExpression();
        if (expressionParser == null)
        {
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