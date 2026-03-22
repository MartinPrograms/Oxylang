using System.Collections.ObjectModel;
using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Parsing;

public class ImportParser : IParser<IReadOnlyList<ImportStatement>>
{
    private Parser parser;
    public IReadOnlyList<ImportStatement> Parse(Parser parser)
    {
        this.parser = parser;
        parser.Reset();
        List<ImportStatement> imports = new();
        while (!parser.IsAtEnd())
        {
            var import = ParseImport();
            if (import == null)
            {
                parser.Advance();
                continue;
            }
            imports.Add(import);
        }
        
        return new ReadOnlyCollection<ImportStatement>(imports);
    }

    private ImportStatement? ParseImport()
    {
        if (parser.MatchKeyword(Language.Keyword.Import) == null) return null;

        var pathToken = parser.MatchString();
        if (pathToken == null)
        {
            parser.Logger.LogError("Expected string literal for import path", parser.SourceFile,
                parser.CurrentLocation);
            return null;
        }
        
        string path = pathToken.Value;

        if (parser.MatchKeyword(Language.Keyword.As) == null)
        {
            parser.Logger.LogError("Expected 'as' keyword after import path", parser.SourceFile,
                parser.CurrentLocation);
            return null;
        }
        
        var aliasToken = parser.MatchIdentifier();
        if (aliasToken == null)
        {
            parser.Logger.LogError("Expected identifier for import alias", parser.SourceFile,
                parser.CurrentLocation);
            return null;
        }
        
        string alias = aliasToken.Value;
        
        if (parser.MatchSyntax(Language.Syntax.Semicolon) == null)
        {
            parser.Logger.LogError("Expected ';' after import statement", parser.SourceFile,
                parser.CurrentLocation);
            return null;
        }
        
        return new ImportStatement(pathToken.Location, path, alias);
    }
}