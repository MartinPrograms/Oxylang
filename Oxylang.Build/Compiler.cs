using System.Diagnostics;
using Oxylang.Systems.Parsing.Nodes;
using Oxylang.Systems.Transformers;

namespace Oxylang.Build;

public class Compiler(string sourceFile, string outputFile, LogLevel logLevel)
{
    private string _sourceFile = sourceFile;
    private string _outputFile = outputFile;
    private LogLevel _logLevel = logLevel;

    public int Compile()
    {
        var sw = Stopwatch.StartNew();
        var logger = new ConsoleLogger(_logLevel);
        var sourceCode = File.ReadAllText(_sourceFile);
        
        var lexer = new Systems.Lexer(sourceCode, _sourceFile);
        var result = lexer.Run(logger);

        if (!result.IsSuccess)
        {
            return 1; // Failure
        }

        if (result.Value.Count == 0)
        {
            logger.Log(new Log(LogLevel.Warning, "No tokens found in source file.",
                new SourceFile(_sourceFile, sourceCode), new SourceLocation(1, 1)));
        }
        
        // For now debug print every token, but later this will be removed and replaced with the actual compilation process.
        foreach (var token in result.Value)
        {
            logger.Log(new Log(LogLevel.Debug, $"{token}", new SourceFile(_sourceFile, sourceCode),
                token.Lexeme.Location));
        }
        
        // Now we parse it
        var parser = new Systems.Parsing.Parser(new (result.Value), new SourceFile(_sourceFile, sourceCode));
        var parseResult = parser.Run(logger);
        if (!parseResult.IsSuccess)
        {
            return 1; // Failure
        }
        
        // Print the root
        var root = parseResult.Value;
        
        root = new DesugarTransformer().Visit(root) as Root;
        if (root == null)
        {
            logger.Log(new Log(LogLevel.Error, "Failed to desugar the AST.", new SourceFile(_sourceFile, sourceCode), new SourceLocation(1, 1)));
            return 1; // Failure
        }
        
        logger.Log(new Log(LogLevel.Debug, $"Parsed root: {root}", new SourceFile(_sourceFile, sourceCode), new SourceLocation(1, 1)));

        var attributeAnalyzer = new AttributeAnalyzer(logger, new SourceFile(_sourceFile, sourceCode));
        var attributeAnalysisResult = attributeAnalyzer.Transform(root);
        if (!attributeAnalysisResult.IsSuccess)
        {
            return 1; // Failure
        }

        var analyzer = new TypeAnalyzer(logger, new SourceFile(_sourceFile, sourceCode));
        var analysisResult = analyzer.Transform(root);
        if (!analysisResult.Success)
        {
            return 1; // Failure
        }

        var codegen = new CodeGenerator(logger, new SourceFile(_sourceFile, sourceCode), true);
        var codegenResult = codegen.Transform(root);
        
        if (!codegenResult.Success)
        {
            return 1; // Failure
        }
        
        File.WriteAllText(_outputFile, codegenResult.Code);
        logger.Log(new Log(LogLevel.Info, $"Successfully compiled {_sourceFile} to {_outputFile} in {sw.ElapsedMilliseconds}ms", new SourceFile(_sourceFile, sourceCode), new SourceLocation(1, 1)));

        var types = codegen.Module.Types;
        foreach (var t in types)
            logger.Log(new Log(LogLevel.Debug, $"Generated type: {t.GetVisualRepresentation(false)}", new SourceFile(_sourceFile, sourceCode),
                new SourceLocation(1, 1)));
        
        return 0; // Success
    }
}