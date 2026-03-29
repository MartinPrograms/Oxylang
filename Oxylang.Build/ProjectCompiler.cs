using System.Diagnostics;
using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Build;

public record BuildResult(bool IsSuccess);

public class ProjectCompiler(ILogger logger, Project project, Arguments arguments)
{
    private List<CompilationUnit> GatherCompilationUnits(List<CompilationUnit> units)
    {
        // Check each import, if the import is not already in the list of compilation units, add it to the list and gather its imports as well.
        List<CompilationUnit> allUnits = new();
        HashSet<string> visited = new();
        
        void Visit(CompilationUnit unit)
        {
            if (visited.Contains(unit.Identifier)) return;
            visited.Add(unit.Identifier);
            allUnits.Add(unit);
            var copy = unit.Reliances.ToArray();
            foreach (var reliance in copy)
            {
                var importUnit = units.FirstOrDefault(u => u.Identifier == reliance.Identifier);
                if (importUnit == null)
                {
                    if (File.Exists(reliance.Identifier))
                    {
                        importUnit = CreateCompilationUnit(reliance.Identifier, reliance.Identifier);
                        if (!importUnit.Parse())
                        {
                            logger.LogError($"Failed to parse imported file '{reliance.Identifier}'.", new SourceFile(importUnit.FilePath, importUnit.SourceCode), new SourceLocation(1, 1));
                            return;
                        }
                        if (!importUnit.Process())
                        {
                            logger.LogError($"Failed to process imported file '{reliance.Identifier}'.", new SourceFile(importUnit.FilePath, importUnit.SourceCode), new SourceLocation(1, 1));
                            return;
                        }
                        units.Add(importUnit);
                    }
                    else
                    {
                        logger.LogError($"Failed to find compilation unit for reliance '{reliance.Identifier}'.", new(), new());
                        return;
                    }
                }
                Visit(importUnit);
            }
        }
        
        var copy = units.ToArray();
        foreach (var unit in copy)
        {
            Visit(unit);
        }

        return allUnits;
    }
    
    private CompilationUnit CreateCompilationUnit(string sourceFile, string identifier)
    {
        var sourceCode = File.ReadAllText(sourceFile);
        return new CompilationUnit(identifier, sourceCode, sourceFile, Path.GetDirectoryName(sourceFile)!, logger);
    }
    
    record Edge(CompilationUnit From, CompilationUnit To);
    
    private List<CompilationUnit> SortCompilationUnits(List<CompilationUnit> units)
    {
        var edges = units.SelectMany(unit => unit.Reliances.Select(r =>
        {
            var dependency = units.FirstOrDefault(u => u.Identifier == r.Identifier);
            if (dependency == null)
            {
                logger.LogError($"Failed to find compilation unit for reliance '{r.Identifier}'.", new(), new());
                return null;
            }

            return new Edge(unit, dependency);
        })).Where(e => e != null).ToList()!;

        List<CompilationUnit> sortedUnits = new();
        HashSet<CompilationUnit> visited = new();

        void Visit(CompilationUnit unit)
        {
            if (visited.Contains(unit)) return;
            visited.Add(unit);
            foreach (var edge in edges.Where(e => e!.From == unit))
                Visit(edge!.To);
            sortedUnits.Add(unit);
        }

        foreach (var unit in units)
            Visit(unit);

        return sortedUnits;
    }
    
    public BuildResult Build()
    {
        logger.LogInfo("Starting build process...", new(), new());

        var modules = project.Modules
            .Select(x => CreateCompilationUnit(x.Value, x.Key))
            .ToList();

        modules.ForEach(x =>
        {
            if (!x.Parse())
            {
                logger.LogError($"Failed to parse module '{x.Identifier}'.", new SourceFile(x.FilePath, x.SourceCode),
                    new SourceLocation(1, 1));
            }
        });
        
        if (logger.HasErrors())
        {
            logger.LogError("Build failed due to errors in parsing modules.", new(), new());
            return new BuildResult(false);
        }
        
        var sources = project.Sources
            .Select(x => CreateCompilationUnit(x, x)) // Unlike modules, source files require the .oxy extension to be included so they are differentiated from modules.
            .ToList();
        
        sources.ForEach(x =>
        {
            if (!x.Parse())
            {
                logger.LogError($"Failed to parse source file '{x.Identifier}'.",
                    new SourceFile(x.FilePath, x.SourceCode),
                    new SourceLocation(1, 1));
            }
        });
        
        if (logger.HasErrors())
        {
            logger.LogError("Build failed due to errors in parsing source files.", new(), new());
            return new BuildResult(false);
        }
        
        List<CompilationUnit> units = modules.Concat(sources).ToList();
        units.ForEach(x =>
        {
            if (!x.Process())
            {
                logger.LogError($"Failed to process compilation unit '{x.Identifier}'.",
                    new SourceFile(x.FilePath, x.SourceCode),
                    new SourceLocation(1, 1));
            }
        });
        var allUnits = GatherCompilationUnits(units);
        allUnits = SortCompilationUnits(allUnits);

        for (int i = 0; i < allUnits.Count; i++)
        {
            for (int j = 0; j < allUnits[i].Exports.Count; j++)
            {
                var export = allUnits[i].Exports[j];
                if (export.Type is ModuleType moduleType && moduleType.Unit == null && export.DefinitionNode is ImportStatement importStatement)
                {
                    allUnits[i].Exports[j] = export with
                    {
                        Type = new ModuleType(export.Type.Location,
                            allUnits.FirstOrDefault(u => u.Identifier == importStatement.Path)!)
                    };
                }
            }
        }
        
        logger.LogDebug("Sorted: \n" + string.Join(", \n", allUnits.Select(x => x.FilePath)) + "\n", new(), new());

        var tempDirectory = Path.Combine(Path.GetTempPath(), "Oxylang", Guid.NewGuid().ToString());
        Directory.CreateDirectory(tempDirectory);
        logger.LogInfo($"Created temporary directory for build artifacts at '{tempDirectory}'.", new(), new());
        
        Dictionary<CompilationUnit, string> compiledUnits = new();
        
        for (int i = 0; i < allUnits.Count; i++)
        {
            var unit = allUnits[i];
            logger.LogDebug($"Building compilation unit '{unit.Identifier}' ({i + 1}/{allUnits.Count})...", new SourceFile(unit.FilePath, unit.SourceCode), new SourceLocation(1, 1));
            var compiled =
                unit.Compile(allUnits.GetRange(0, i).Select(x => new ResolvedReliance(x.Identifier, x)).ToList());
            if (compiled.IsSuccess)
            {
                logger.LogInfo($"Successfully built compilation unit '{unit.Identifier}'.", new SourceFile(unit.FilePath, unit.SourceCode), new SourceLocation(1, 1));
                compiledUnits[unit] = compiled.Result!;
            }
            else
            {
                logger.LogError($"Failed to build compilation unit '{unit.Identifier}'.", new SourceFile(unit.FilePath, unit.SourceCode), new SourceLocation(1, 1));
                return new BuildResult(false);
            }
        }
        
        if (logger.HasErrors())
        {
            logger.LogError("Build failed due to errors in building compilation units.", new(), new());
            return new BuildResult(false);
        }
        
        Dictionary<CompilationUnit, string> outputPaths = new();
        
        foreach (var kvp in compiledUnits)
        {
            var outputPath = Path.Combine(tempDirectory, Guid.NewGuid().ToString() + ".qbe");
            File.WriteAllText(outputPath, kvp.Value);
            outputPaths[kvp.Key] = outputPath;

            logger.LogDebug($"Wrote {kvp.Key.Identifier}'s compiled output to '{outputPath}'.",
                new SourceFile(kvp.Key.FilePath, kvp.Key.SourceCode), new SourceLocation(1, 1));
            
            logger.LogDebug($"\n{kvp.Value}\n", new SourceFile(kvp.Key.FilePath, kvp.Key.SourceCode), new SourceLocation(1, 1));
        }

        // The last few remaining steps are as follows: 
        // 1: call `qbe ${filename}` and ensure the output is not an error
        // 2: compile the output binaries using clang together
        // 3: done!
        
        foreach (var kvp in outputPaths)
        {
            if (!RunProcess("qbe", $"{kvp.Value}", out var qbeOutput))
            {
                logger.LogError($"Failed to compile '{kvp.Key.Identifier}' with qbe. Output: {qbeOutput}", new SourceFile(kvp.Key.FilePath, kvp.Key.SourceCode), new SourceLocation(1, 1));
                return new BuildResult(false);
            }
            
            // qbe outputs assembly code, so we need to write that to a file and compile it with clang.
            File.WriteAllText(kvp.Value + ".S", qbeOutput);
        }
        
        string flags = string.Join(" ", project.LinkedLibraries.Select(lib => $"-l{lib}"));
        // We must tell clang to compile to .o files, and to not link them just yet, since we need to link them together at the end.
        
        foreach (var kvp in outputPaths)
        {
            var outputBinary = Path.Combine(tempDirectory, Guid.NewGuid().ToString()) + ".o";
            if (!RunProcess("clang", $"{kvp.Value}.S -o {outputBinary} {flags} -c", out var clangOutput))
            {
                logger.LogError($"Failed to compile '{kvp.Key.Identifier}' with clang. Output: {clangOutput}", new SourceFile(kvp.Key.FilePath, kvp.Key.SourceCode), new SourceLocation(1, 1));
                return new BuildResult(false);
            }
        }
        
        foreach (var kvp in outputPaths)
        {
            File.Delete(kvp.Value);
            File.Delete(kvp.Value + ".S");
        }
        
        // Now we have a lot of object files! And it is time to link them together. We can just call clang again and pass all the object files together, and it will link them together.
        var objectFiles = Directory.GetFiles(tempDirectory, "*.o");
        var outputExecutable = Path.Combine(Directory.GetCurrentDirectory(), project.Output);
        
        if (!Directory.Exists(Path.GetDirectoryName(outputExecutable)))
        {
            Directory.CreateDirectory(Path.GetDirectoryName(outputExecutable)!);
        }
        
        if (OperatingSystem.IsWindows())
            outputExecutable += ".exe";
        
        var linkFlags = string.Join(" ", objectFiles) + " -o " + outputExecutable + " " + flags;
        if (!RunProcess("clang", linkFlags, out var linkOutput))
        {
            logger.LogError($"Failed to link object files with clang. Output: {linkOutput}", new(), new());
            return new BuildResult(false);
        }
        
        logger.LogDebug("Wrote final executable to '" + outputExecutable + "'.", new(), new());

        return new(true);
    }

    private bool RunProcess(string process, string args, out string output)
    {
        if (OperatingSystem.IsWindows())
        {
            process += ".exe";
        }
        
        var startInfo = new ProcessStartInfo(process, args)
        {
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };
        
        try
        {
            using var proc = Process.Start(startInfo);
            if (proc == null)
            {
                output = $"Failed to start process '{process}'.";
                return false;
            }

            output = proc.StandardOutput.ReadToEnd() + proc.StandardError.ReadToEnd();
            proc.WaitForExit();
            return proc.ExitCode == 0;
        }
        catch (Exception ex)
        {
            output = $"Exception while running process '{process}': {ex.Message}";
            return false;
        }
    }
}

public static class ProjectExtensions
{
    extension(Project project)
    {
        public BuildResult Build(ILogger logger, Arguments arguments)
        {
            return new ProjectCompiler(logger, project, arguments).Build();
        }
    }
}