// This is the build system and compiler for Oxylang.
// By default it runs in build system mode, which means it looks for a toml project file, and builds that project, but it can be overridden to compile a single file.

// For now we run in compile single source file mode, and the build system will be implemented later.

using System.Diagnostics;
using CommandLine;
using Oxylang;
using Oxylang.Build;

Stopwatch stopwatch = Stopwatch.StartNew();

var arguments = Parser.Default.ParseArguments<Arguments>(args).Value ?? throw new Exception("Failed to parse arguments");

if (arguments.StandardLibraryPath != "")
{
    Environment.SetEnvironmentVariable("OXYLIB", arguments.StandardLibraryPath);
}

var logger = new ConsoleLogger(arguments.LogLevel);

if (arguments.ProjectFilePath == "")
{
    // Check if any .toml file exists in the current directory
    var tomlFiles = Directory.GetFiles(Directory.GetCurrentDirectory(), "*.toml");
    if (tomlFiles.Length == 0)
    {
        logger.Log(new Log(LogLevel.Warning,
            "No project file specified and no .toml file found in the current directory. Nothing to build.", new(),
            new ()));
        return 1;
    }
    
    arguments.ProjectFilePath = tomlFiles[0];
}

Environment.CurrentDirectory = Path.GetDirectoryName(arguments.ProjectFilePath)!; // Change the current directory to the project file's directory, so that relative paths in the project file work as expected.
var project = Project.LoadFromToml(File.ReadAllText(arguments.ProjectFilePath), logger);

var buildResult = project.Build(logger, arguments);
stopwatch.Stop();

if (!buildResult.IsSuccess)
{
    logger.Log(new Log(LogLevel.Error, "Build failed in " + stopwatch.Elapsed.TotalSeconds + " seconds.", new(), new()));
    return 1;
}

logger.Log(new Log(LogLevel.Info, "Build succeeded in " + stopwatch.Elapsed.TotalSeconds + " seconds.", new(), new()));

return 0;