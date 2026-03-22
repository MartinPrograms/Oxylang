using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Parsing;

public class StructDefinitionParser : IParser<IReadOnlyList<StructDefinition>>
{
    private Parser _parser;
        
    public IReadOnlyList<StructDefinition> Parse(Parser parser)
    {
        _parser = parser;
        List<StructDefinition> structDefinitions = new();
        
        parser.Reset();
        
        while (!parser.IsAtEnd())
        {
            var structDef = ParseStructDefinition();
            if (structDef != null)
            {
                structDefinitions.Add(structDef);
            }
            else
            {
                parser.Advance();
            }
        }

        return structDefinitions;
    }

    private StructDefinition ParseStructDefinition()
    {
        // struct <identifier> { identifier: type; ... }
        var attributes = _parser.ParseAttributes();
        if (_parser.MatchKeyword(Language.Keyword.Struct) == null) return null;

        var nameToken = _parser.MatchIdentifier();
        if (nameToken == null)
        {
            _parser.Logger.LogError("Expected identifier for struct name", _parser.SourceFile,
                _parser.CurrentLocation);
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
                _parser.Logger.LogError("Expected '>' to close generic type parameter list", _parser.SourceFile,
                    _parser.CurrentLocation);
                return null;
            }
        }

        if (_parser.MatchSyntax(Language.Syntax.LeftBrace) == null)
        {
            _parser.Logger.LogError("Expected '{' after struct name", _parser.SourceFile,
                _parser.CurrentLocation);
            return null;
        }

        var fields = new List<VariableDeclaration>();
        while (_parser.MatchSyntax(Language.Syntax.RightBrace) == null)
        {
            var field = _parser.ParseVariableDeclaration();
            if (field == null)
            {
                _parser.Logger.LogError("Expected variable declaration for struct field", _parser.SourceFile,
                    _parser.CurrentLocation);
                return null;
            }
            fields.Add(field);
            if (_parser.MatchSyntax(Language.Syntax.Semicolon) == null)
            {
                _parser.Logger.LogError("Expected ';' after struct field declaration", _parser.SourceFile,
                    _parser.CurrentLocation);
                return null;
            }
        }
        
        return new StructDefinition(nameToken.Value, attributes, genericParameters.Select(x => new GenericType(nameToken.Location, x)).ToList(), fields, nameToken.Location);
    }
}