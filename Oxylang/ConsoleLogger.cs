namespace Oxylang;

public class ConsoleLogger : ILogger
{
    private LogLevel _minLevel;
    private bool _hasErrors = false;
    
    public ConsoleLogger(LogLevel minLevel = LogLevel.Info)
    {
        _minLevel = minLevel;
    }
    
    public void Log(Log log)
    {
        if (log.Level == LogLevel.Error) _hasErrors = true;
        if (log.Level < _minLevel) return;
        Console.WriteLine(log.ToString());
    }
    
    public bool HasErrors()
    {
        return _hasErrors;
    }
}