using Oxylang.Systems;
using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;
using Oxylang.Systems.Transformers;

namespace Oxylang;

// A compilation unit can rely on other compilation units, which are represented by an identifier.
public record Reliance(string Identifier, string Alias);

// An export is a named type that can be used by other compilation units that rely on this one.
public record Export(string Identifier, TypeNode Type, Node DefinitionNode);

public record ResolvedReliance(string Identifier, CompilationUnit Unit);

public class CompilationUnit
{
    public CompilationUnit(ILogger logger, Root? astRoot)
    {
        _logger = logger;
        _astRoot = astRoot;
    }

    public string Identifier { get; }
    public string SourceCode { get; }
    public string FilePath { get; }
    public string DirectoryPath { get; } // for relative imports
    public List<Reliance> Reliances { get; } = new();
    public List<Export> Exports { get; } = new();
    
    private readonly ILogger _logger;
    private readonly List<Token> _tokens = new();
    private Root? _astRoot = null;
    
    public CompilationUnit(string identifier, string sourceCode, string filePath, string directoryPath, ILogger logger)
    {
        Identifier = identifier;
        SourceCode = sourceCode;
        FilePath = filePath;
        DirectoryPath = directoryPath;
        _logger = logger;
        
        var result = new Lexer(sourceCode, filePath).Run(logger);
        if (!result.IsSuccess)
        {
            logger.Log(new Log(LogLevel.Error, $"Failed to lex the source code of compilation unit '{identifier}'.",
                new SourceFile(filePath, sourceCode), new SourceLocation(1, 1)));
            return; // Failure
        }
        
        _tokens.AddRange(result.Value!);
    }

    public bool Parse()
    {
        var parser = new Systems.Parsing.Parser(_tokens, new SourceFile(FilePath, SourceCode));
        var parseResult = parser.Run(_logger);
        if (!parseResult.IsSuccess)
        {
            return false;
        }

        var root = parseResult.Value;
        root = new DesugarTransformer().Visit(root) as Root;
        if (root == null)
        {
            _logger.Log(new Log(LogLevel.Error, $"Failed to desugar the AST of compilation unit '{Identifier}'.",
                new SourceFile(FilePath, SourceCode), new SourceLocation(1, 1)));
            return false;
        }
        
        _astRoot = root;
        return true; // we dont check types yet, since they might rely on other units.
    }
    
    // Process the ast to find reliance & export information.
    public bool Process() 
    {
        if (_astRoot == null)
        {
            _logger.Log(new Log(LogLevel.Error, $"Cannot build reliances for compilation unit '{Identifier}' because it has not been parsed yet.",
                new SourceFile(FilePath, SourceCode), new SourceLocation(1, 1)));
            return false;
        }
        
        var oldDirectory = Environment.CurrentDirectory;
        Environment.CurrentDirectory = DirectoryPath; // Set the current directory to the unit's directory for relative imports.
        
        var relianceBuilder = new RelianceBuilder(_logger, new SourceFile(FilePath, SourceCode));
        var relianceBuildResult = relianceBuilder.Transform(_astRoot);
        if (!relianceBuildResult.IsSuccess)
        {
            return false;
        }

        Reliances.AddRange(relianceBuildResult.Reliances);
        Exports.AddRange(relianceBuildResult.Exports);
        
        Environment.CurrentDirectory = oldDirectory; // Restore the original current directory.
        
        return true;
    }
    
    // Compile the unit
    public (bool IsSuccess, string Result) Compile(List<ResolvedReliance> resolvedReliances)
    {
        var typeChecker = new TypeAnalyzer(_logger, new SourceFile(FilePath, SourceCode), resolvedReliances, this);
        var typeCheckResult = typeChecker.Transform(_astRoot!);
        if (!typeCheckResult.Success)
        {
            return (false, string.Empty);
        }
        
        var codegen = new CodeGenerator(_logger, new SourceFile(FilePath, SourceCode), true, resolvedReliances, this);
        var codegenResult = codegen.Transform(_astRoot!);
        if (!codegenResult.Success)
        {
            return (false, string.Empty);
        }
        
        return (true, codegenResult.Code);
    }

    public bool TypeCheck(List<ResolvedReliance> reliances)
    {
        var typeChecker = new TypeAnalyzer(_logger, new SourceFile(FilePath, SourceCode), reliances, this);
        var typeCheckResult = typeChecker.Transform(_astRoot!);
        return typeCheckResult.Success;
    }
    
    public string MangleName(string name, Node node)
    {
        if (node is FunctionDefinition funcDef)
        {
            if (funcDef.Attributes.Any(a => a.Name == "symbol"))
            {
                var symbolAttr = funcDef.Attributes.First(a => a.Name == "symbol");
                if (symbolAttr.Arguments.Count == 1 && symbolAttr.Arguments[0] is string symbolName)
                {
                    return symbolName;
                }
            }
        }
        
        var fileTag = SanitizeIdentifier(Path.GetFileNameWithoutExtension(FilePath));
        return $"_oxy_{fileTag}_{name}_{node.Location.Line}_{node.Location.Column}";
    }

    public static string SanitizeIdentifier(string s)
    {
        // Replace anything that isn't alphanumeric with _
        return string.Concat(s.Select(c => char.IsAsciiLetterOrDigit(c) ? c : '_'));
    }
}