using CommandLine;

namespace Oxylang.Build;

public class Arguments
{
    [Option('c', "compile", Required = true, HelpText = "Source file to compile.")]
    public string SourceFile { get; set; }
    
    [Option('o', "output", Required = false, HelpText = "Output file name. Defaults to 'a.qbe'.")]
    public string OutputFile { get; set; } = "a.qbe";
    
    [Option('l', "log-level", Required = false, HelpText = "Log level (Debug, Info, Warning, Error). Defaults to Info.")]
    public LogLevel LogLevel { get; set; } = LogLevel.Info;
}