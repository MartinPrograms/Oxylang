// This is the build system and compiler for Oxylang.
// By default it runs in build system mode, which means it looks for a toml project file, and builds that project, but it can be overridden to compile a single file.

// For now we run in compile single source file mode, and the build system will be implemented later.
using CommandLine;
using Oxylang;
using Oxylang.Build;

var arguments = Parser.Default.ParseArguments<Arguments>(args).Value ?? throw new Exception("Failed to parse arguments");
if (!File.Exists(arguments.SourceFile))
{
    Console.WriteLine($"Source file '{arguments.SourceFile}' does not exist.");
    return 1;
}

var compiler = new Compiler(arguments.SourceFile, arguments.OutputFile, arguments.LogLevel);
return compiler.Compile();