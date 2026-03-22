namespace Oxylang;

public enum LogLevel
{
    Debug,
    Info,
    Warning,
    Error
}

public record struct SourceFile(string Name, string Content) {
    public override string ToString() => Name;
};

public record struct SourceLocation(int Line, int Column) {
    public override string ToString() => $"{Line}:{Column}";
};

public record Log(LogLevel Level, string Message, SourceFile File, SourceLocation Location) {
    public override string ToString() => $"[{Level}] {Message} at {File}:{Location}";
};

public interface ILogger
{
    void Log(Log log);
    void Log(LogLevel level, string message, SourceFile file, SourceLocation location) => Log(new Log(level, message, file, location));
    void LogInfo(string message, SourceFile file, SourceLocation location) => Log(LogLevel.Info, message, file, location);
    void LogWarning(string message, SourceFile file, SourceLocation location) => Log(LogLevel.Warning, message, file, location);
    void LogError(string message, SourceFile file, SourceLocation location) => Log(LogLevel.Error, message, file, location);
    bool HasErrors();
}

public record Result<T>(T? Value) {
    public bool IsSuccess => Value != null;
};

public interface ICompilerSystem<T>
{
    Result<T> Run(ILogger logger);
}