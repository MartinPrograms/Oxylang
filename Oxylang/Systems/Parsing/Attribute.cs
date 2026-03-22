namespace Oxylang.Systems.Parsing;

/// <summary>
/// @extern, or @thing(arg1, "arg2", 123), etc.
/// Attributes can NOT have dynamic expressions, they must all be literals.
/// </summary>
public class Attribute
{
    public string Name { get; }
    public IReadOnlyList<string> Arguments { get; }
    
    public Attribute(string name, IReadOnlyList<string> arguments)
    {
        Name = name;
        Arguments = arguments;
    }
}