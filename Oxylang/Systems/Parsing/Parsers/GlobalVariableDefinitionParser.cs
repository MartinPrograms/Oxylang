using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Parsing;

public class GlobalVariableDefinitionParser : IParser<List<VariableDeclaration>>
{
    private Parser parser;

    public List<VariableDeclaration> Parse(Parser parser)
    {
        this.parser = parser;
        List<VariableDeclaration> variables = new();
        parser.Reset();
        
        while (!parser.IsAtEnd())
        {
            var variable = ParseGlobalVariable();
            if (variable == null)
            {
                parser.Advance();
                continue;
            }
            variables.Add(variable);
        }
        
        return variables;
    }
    
    private VariableDeclaration? ParseGlobalVariable()
    {
        var attributes = parser.ParseAttributes();
        if (parser.MatchKeyword(Language.Keyword.Let) == null) return null;
        if (!parser.IsGlobalScope()) return null;

        var variableDeclaration = parser.ParseVariableDeclaration();
        if (variableDeclaration == null)
        {
            parser.Logger.LogError("Invalid global variable declaration", parser.SourceFile,
                parser.CurrentLocation);
            return null;
        }

        if (parser.MatchSyntax(Language.Syntax.Semicolon) == null)
        {
            parser.Logger.LogError("Expected ';' after global variable declaration", parser.SourceFile,
                parser.CurrentLocation);
            return null;
        }

        return new VariableDeclaration(
            variableDeclaration.Name,
            variableDeclaration.Type,
            attributes,
            variableDeclaration.Initializer,
            variableDeclaration.Location
        );
    }
}