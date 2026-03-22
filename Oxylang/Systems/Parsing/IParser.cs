namespace Oxylang.Systems.Parsing;

public interface IParser<T>
{
    T Parse(Parser parser);
}