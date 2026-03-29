using CsToml;
using GlobExpressions;

namespace Oxylang.Build;

/*
   [project]
   name = "Test02"
   version = "0.1.0"
   output = "bin/test02"
   type = "executable" # static(library), shared(library) or executable. (without the content in parentheses)
   sources = ["src/** /*.oxy"] # glob pattern for source files. You can also specify individual files like ["src/main.oxy", "src/utils.oxy"]
   
   [modules]
   std = "${OXYLIB}/std/std.oxy"
   
   [links]
   libs = ["m", "raylib"]
*/

public record Version(int Major, int Minor, int Patch);

public enum ProjectType
{
    Executable,
    StaticLibrary,
    SharedLibrary
}

public class Project
{
    public string Name { get; }
    public Version Version { get; }
    public string Output { get; }
    public ProjectType Type { get; }
    public List<string> Sources { get; }
    public Dictionary<string, string> Modules { get; }
    public List<string> LinkedLibraries { get; }
    
    public Project(string name, Version version, string output, ProjectType type, List<string> sources, Dictionary<string, string> modules, List<string> linkedLibraries)
    {
        Name = name;
        Version = version;
        Output = output;
        Type = type;
        Sources = sources;
        Modules = modules;
        LinkedLibraries = linkedLibraries;
    }
    
    private static Version ParseVersion(string versionStr)
    {
        var parts = versionStr.Split('.');
        if (parts.Length != 3)
            throw new FormatException($"Invalid version format: '{versionStr}'. Expected format is 'major.minor.patch'.");
        
        return new Version(int.Parse(parts[0]), int.Parse(parts[1]), int.Parse(parts[2]));
    }
    
    private static ProjectType ParseProjectType(string typeStr)
    {
        return typeStr.ToLower() switch
        {
            "executable" => ProjectType.Executable,
            "static" => ProjectType.StaticLibrary,
            "shared" => ProjectType.SharedLibrary,
            _ => throw new FormatException($"Invalid project type: '{typeStr}'. Expected 'executable', 'static' or 'shared'.")
        };
    }

    public static Project LoadFromToml(string tomlContent, ILogger logger)
    {
        var document =
            CsTomlSerializer.Deserialize<TomlDocument>(
                new ReadOnlySpan<byte>(System.Text.Encoding.UTF8.GetBytes(tomlContent)));

        var projectSection = document.RootNode["project"];
        if (!projectSection.IsTableHeader)
            throw new FormatException("Missing [project] section in the TOML file.");

        var modulesSection = document.RootNode["modules"]; // optional
        var linksSection = document.RootNode["links"]; // optional

        string name = projectSection["name"].GetValue<string>();
        Version version = ParseVersion(projectSection["version"].GetValue<string>());
        string output = projectSection["output"].GetValue<string>();
        ProjectType type = ParseProjectType(projectSection["type"].GetValue<string>());
        List<string> sources = projectSection["sources"].GetValue<List<string>>();

        Dictionary<string, string> modules = new();
        if (modulesSection.IsTableHeader)
        {
            var enumerator = modulesSection.GetEnumerator();
            while (enumerator.MoveNext())
            {
                var kvp = enumerator.Current;
                modules[kvp.Key.GetString()] = kvp.Value.GetValue<string>();
            }
        }

        List<string> linkedLibraries = new();
        if (linksSection.IsTableHeader)
        {
            linkedLibraries = linksSection["libs"].GetValue<List<string>>();
        }
        
        // Replace any ${} variables in module paths with environment variables
        foreach (var key in modules.Keys.ToList())
        {
            string path = modules[key];
            int startIndex = path.IndexOf("${", StringComparison.InvariantCulture);
            while (startIndex != -1)
            {
                int endIndex = path.IndexOf("}", startIndex, StringComparison.InvariantCulture);
                if (endIndex == -1) break; // No closing brace, stop processing
            
                string varName = path.Substring(startIndex + 2, endIndex - startIndex - 2);
                string? varValue = Environment.GetEnvironmentVariable(varName);
                if (varValue == null)
                    throw new FormatException($"Environment variable '{varName}' not found for module '{key}'.");
                path = path.Substring(0, startIndex) + varValue + path.Substring(endIndex + 1);
                startIndex = path.IndexOf("${", startIndex + varValue.Length);
            }
            modules[key] = path;
        }
        
        // Glob the source files
        List<string> expandedSources = new();
        foreach (var source in sources)
        {
            var matches = Glob.Files(Directory.GetCurrentDirectory(), source).ToList();
            if (matches.Count == 0)
                logger.Log(new Log(LogLevel.Warning, $"No source files found for pattern '{source}'.", new(), new()));

            matches = matches.Select(Path.GetFullPath).ToList();
            
            expandedSources.AddRange(matches);
        }
        sources = expandedSources;

        return new Project(name, version, output, type, sources, modules, linkedLibraries);
    }
}