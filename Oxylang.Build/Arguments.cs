using CommandLine;

namespace Oxylang.Build;

public class Arguments
{
    [Option('p', "project", Required = false,
        HelpText = "Path to the project file (.toml) to build. By default it chooses the first .toml file in the current directory, if any.")]
    public string ProjectFilePath { get; set; } = "";

    [Option('s', "stdlib", Required = false,
        HelpText = "Path to the standard library directory. If not provided there should be a OXYLIB environment variable pointing to the standard library directory, otherwise it will not be available.")]
    public string StandardLibraryPath { get; set; } = "";

    [Option('a', "arch", Required = false,
        HelpText = "Target architecture (Amd64, Arm64, RiscV64). Defaults to system arch.")]
    public Language.Architecture TargetArchitecture { get; set; } =
        System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture switch
        {
            System.Runtime.InteropServices.Architecture.X64 => Language.Architecture.Amd64,
            System.Runtime.InteropServices.Architecture.Arm64 => Language.Architecture.Arm64,
            System.Runtime.InteropServices.Architecture.RiscV64 => Language.Architecture.RiscV64,
            _ => throw new NotSupportedException("Unsupported system architecture.")
        };
    
    [Option('l', "log-level", Required = false,
        HelpText = "Log level (Debug, Info, Warning, Error). Defaults to Info.")]
    public LogLevel LogLevel { get; set; } = LogLevel.Info;
}