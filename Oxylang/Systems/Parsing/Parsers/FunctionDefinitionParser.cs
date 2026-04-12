using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Parsing;

public class FunctionDefinitionParser : IParser<List<FunctionDefinition>>
{
    Parser _parser;

    public List<FunctionDefinition> Parse(Parser parser)
    {
        _parser = parser;
        List<FunctionDefinition> functions = new();
        _parser.Reset();
        while (!_parser.IsAtEnd())
        {
            var function = ParseFunctionDefinition();
            if (function == null)
            {
                _parser.Advance();
                continue;
            }

            functions.Add(function);
        }

        return functions;
    }

    private FunctionDefinition? ParseFunctionDefinition()
    {
        var attributes = _parser.ParseAttributes();
        if (_parser.MatchKeyword(Language.Keyword.Fn) == null) return null;

        var nameToken = _parser.MatchIdentifier();
        if (nameToken == null)
        {
            // We probably stumbled upon a function type fn (...) -> .., so we should just return null and let the type parser handle it
            return null;
        }

        List<string> genericParameters = new();
        if (_parser.MatchOperator(Language.Operator.LessThan) != null)
        {
            do
            {
                var genericTypeToken = _parser.MatchIdentifier();
                if (genericTypeToken == null)
                {
                    _parser.Logger.LogError("Expected identifier for generic type parameter", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }

                genericParameters.Add(genericTypeToken.Value);
            } while (_parser.MatchSyntax(Language.Syntax.Comma) != null);

            if (_parser.CloseGeneric() == null)
            {
                _parser.Logger.LogError("Expected '>' to close generic parameter list", _parser.SourceFile,
                    _parser.CurrentLocation);
                return null;
            }
        }

        if (_parser.MatchSyntax(Language.Syntax.LeftParen) == null)
        {
            _parser.Logger.LogError("Expected '(' after function name", _parser.SourceFile,
                _parser.CurrentLocation);
            return null;
        }

        List<VariableDeclaration> parameters = new();
        bool isVariadic = false;
        if (_parser.MatchSyntax(Language.Syntax.RightParen) == null)
        {
            do
            {
                if (_parser.MatchOperator(Language.Operator.Variadic) != null)
                {
                    isVariadic = true;
                    break;
                }
                
                var parameter = _parser.ParseVariableDeclaration();
                if (parameter == null)
                {
                    _parser.Logger.LogError("Expected parameter declaration", _parser.SourceFile,
                        _parser.CurrentLocation);
                    return null;
                }

                parameters.Add(parameter);
            } while (_parser.MatchSyntax(Language.Syntax.Comma) != null);
            
            if (_parser.MatchSyntax(Language.Syntax.RightParen) == null)
            {
                _parser.Logger.LogError("Expected ')' after parameter list", _parser.SourceFile,
                    _parser.CurrentLocation);
                return null;
            }
        }

        TypeNode? returnType = null;
        if (_parser.MatchOperator(Language.Operator.Arrow) != null)
        {
            returnType = _parser.ParseType();
            if (returnType == null)
            {
                _parser.Logger.LogError("Expected return type after '->'", _parser.SourceFile,
                    _parser.CurrentLocation);
                return null;
            }
        }
        else
        {
            returnType = new VoidType();
        }
        
        if (_parser.MatchSyntax(Language.Syntax.Semicolon) != null)
        {
            return new FunctionDefinition(nameToken.Location, nameToken.Value, parameters, attributes,
                genericParameters.Select(x => new GenericType(x)).ToList(), returnType, null, isVariadic);
        }
        
        if (_parser.MatchSyntax(Language.Syntax.LeftBrace) == null)
        {
            _parser.Logger.LogError("Expected '{' to start function body", _parser.SourceFile,
                _parser.CurrentLocation);
            return null;
        }
        
        var body = _parser.ParseBlock();
        if (body == null)
        {
            _parser.Logger.LogError("Expected function body", _parser.SourceFile,
                _parser.CurrentLocation);
            return null;
        }
        
        return new FunctionDefinition(nameToken.Location, nameToken.Value, parameters, attributes,
            genericParameters.Select(x => new GenericType(x)).ToList(), returnType, body, isVariadic);
    }
}