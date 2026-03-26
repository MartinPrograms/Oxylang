namespace Oxylang.Systems;

public record TokenValue(string Representation, SourceLocation Location)
{
    public override string ToString() => Representation;
}

public record TokenValueString(string Value, SourceLocation Location) : TokenValue(Value, Location)
{
    public override string ToString() => Representation;
}

public record TokenValueNumber(string Value, string Type, SourceLocation Location) : TokenValue(Value, Location)
{
    public override string ToString() => Representation;
}

public record TokenValueIdentifier(string Value, SourceLocation Location) : TokenValue(Value, Location)
{
    public override string ToString() => Representation;
}

public record TokenValueSyntax(Language.Syntax Value, SourceLocation Location) : TokenValue(Value.ToString(), Location)
{
    public override string ToString() => Representation;
}

public record TokenValueOperator(Language.Operator Value, SourceLocation Location) : TokenValue(Value.ToString(), Location)
{
    public override string ToString() => Representation;
}

public record Token(Language.TokenType Type, TokenValue Lexeme) {
    public override string ToString() => $"{Type, -11}('{Lexeme}') at {Lexeme.Location}";
};

public class Lexer : ICompilerSystem<IReadOnlyList<Token>>
{
    private string _sourceCode;
    private string _fileName;
    private string[] _lines;

    private int _currentLine = 1;
    private int _currentColumn = 1;
    private int _currentIndex = 0;

    private ILogger _logger;
    
    public Lexer(string sourceCode, string fileName = "<unknown>")
    {
        _sourceCode = sourceCode;
        _fileName = fileName;
        _lines = sourceCode.Split('\n');
    }

    public Result<IReadOnlyList<Token>> Run(ILogger logger)
    {
        _logger = logger;
        var tokens = new List<Token>();
        var operatorMap = Language.Operators;
        operatorMap = operatorMap.OrderByDescending(kv => kv.Key.Length).ToDictionary(kv => kv.Key, kv => kv.Value); // Sort by length descending to match longest operators first
        
        while (!IsAtEnd())
        {
            char c = Peek();
            
            if (IsWhitespace(c))
            {
                Advance();
                continue;
            }
            
            if (SkipComments()) continue;

            if (IsDigit(Peek()))
            {
                tokens.Add(ReadNumber());
                continue;
            }
            
            if (IsLetter(Peek()))
            {
                tokens.Add(ReadIdentifierOrKeyword());
                continue;
            }
            
            if (IsSymbol(Peek()))
            {
                tokens.Add(ReadSymbol(operatorMap));
                continue;
            }

            if (Peek() == Language.StringDelimiter[0])
            {
                tokens.Add(ReadString());
                continue;
            }

            logger.Log(new Log(LogLevel.Error, $"Unexpected character '{c}'", new SourceFile(_fileName, _sourceCode), new SourceLocation(_currentLine, _currentColumn)));
            Advance();
        }
        
        if (logger.HasErrors()) return new Result<IReadOnlyList<Token>>(null);
        return new Result<IReadOnlyList<Token>>(tokens);
    }

    private Token ReadString()
    {
        int startLine = _currentLine;
        int startColumn = _currentColumn;
        string lexeme = "";
        
        Advance(); // Skip opening quote

        while (Peek() != Language.StringDelimiter[0] && !IsAtEnd())
        {
            lexeme += Advance();
        }

        // Double every \ in the lexeme to prevent issues with string interpolation later on
        lexeme = lexeme.Replace("\\", "\\\\");
        
        if (IsAtEnd())
        {
            _logger.Log(new Log(LogLevel.Error, "Unterminated string literal", new SourceFile(_fileName, _sourceCode), new SourceLocation(startLine, startColumn)));
            return new Token(Language.TokenType.String, new TokenValueString(lexeme, new SourceLocation(startLine, startColumn)));
        }
        
        Advance(); // Skip closing quote
        
        return new Token(Language.TokenType.String, new TokenValueString(lexeme, new SourceLocation(startLine, startColumn)));
    }

    private Token ReadSymbol(Dictionary<string, Language.Operator> operatorMap)
    {
        int startLine = _currentLine;
        int startColumn = _currentColumn;
        
        // operatorMap is sorted by length descending, so we try to match the longest possible operator first
        foreach (var op in operatorMap.Keys)
        {
            if (Peek() == op[0] && Peek(op.Length - 1) == op[op.Length - 1])
            {
                for (int i = 0; i < op.Length; i++) Advance();
                return new Token(Language.TokenType.Operator, new TokenValueOperator(operatorMap[op], new SourceLocation(startLine, startColumn)));
            }
        }
        
        // If we get here, it's a syntax
        char c = Advance();
        if (Language.Syntaxes.ContainsKey(c.ToString()))
        {
            return new Token(Language.TokenType.Syntax, new TokenValueSyntax(Language.Syntaxes[c.ToString()], new SourceLocation(startLine, startColumn)));
        }
        else
        {
            _logger.Log(new Log(LogLevel.Error, $"Unknown symbol '{c}'", new SourceFile(_fileName, _sourceCode), new SourceLocation(startLine, startColumn)));
            return new Token(Language.TokenType.Syntax, new TokenValueSyntax((Language.Syntax)0, new SourceLocation(startLine, startColumn))); // Return a dummy token to continue lexing
        }
    }

    private Token ReadIdentifierOrKeyword()
    {
        int startLine = _currentLine;
        int startColumn = _currentColumn;
        string lexeme = "";
        
        while (IsAlphanumeric(Peek()))
        {
            lexeme += Advance();
        }

        var type = Language.Keywords.Contains(lexeme) ? Language.TokenType.Keyword : Language.TokenType.Identifier;
        return new Token(type, new TokenValueIdentifier(lexeme, new SourceLocation(startLine, startColumn)));
    }

    // A number can be 0-9, 0-9.0-9, or 0x0-FF, or 0b0, and they can have a suffix for types (123u32, 0.5f32, 8u8, etc.)
    private Token ReadNumber()
    {
        int startLine = _currentLine;
        int startColumn = _currentColumn;
        string lexeme = "";

        bool isHex = false;
        bool isBinary = false;
        string suffix = "";

        if (Peek() == '0' && (Peek(1) == 'x' || Peek(1) == 'X'))
        {
            isHex = true;
            lexeme += Advance(); // 0
            lexeme += Advance(); // x
            while (IsHexDigit(Peek()))
            {
                lexeme += Advance();
            }
        }
        else if (Peek() == '0' && (Peek(1) == 'b' || Peek(1) == 'B'))
        {
            isBinary = true;
            lexeme += Advance(); // 0
            lexeme += Advance(); // b
            while (IsBinaryDigit(Peek()))
            {
                lexeme += Advance();
            }
        }
        else
        {
            while (IsDigit(Peek()))
            {
                lexeme += Advance();
            }

            if (Peek() == '.' && IsDigit(Peek(1)))
            {
                lexeme += Advance(); // .
                while (IsDigit(Peek()))
                {
                    lexeme += Advance();
                }
            }
        }

        // Read suffix which is either u, i, or f followed by digits (e.g. u32, i64, f32, etc.)
        if (Peek() == 'u' || Peek() == 'i' || Peek() == 'f')
        {
            suffix += Advance(); // u, i, or f
            while (IsDigit(Peek()))
            {
                suffix += Advance();
            }
        }

        if (isBinary)
        {
            // Convert the lexeme to a decimal number for easier parsing later on, but keep the original lexeme for error messages and such
            try
            {
                lexeme = Convert.ToInt64(lexeme.Substring(2), 2).ToString();
            }
            catch (Exception)
            {
                _logger.Log(new Log(LogLevel.Error, $"Invalid binary literal '{lexeme}'",
                    new SourceFile(_fileName, _sourceCode), new SourceLocation(startLine, startColumn)));
                lexeme = "0"; // Set to 0 to continue lexing
            }
        }
        
        if (isHex)
        {
            // Convert the lexeme to a decimal number for easier parsing later on, but keep the original lexeme for error messages and such
            try
            {
                lexeme = Convert.ToInt64(lexeme.Substring(2), 16).ToString();
            }
            catch (Exception)
            {
                _logger.Log(new Log(LogLevel.Error, $"Invalid hexadecimal literal '{lexeme}'",
                    new SourceFile(_fileName, _sourceCode), new SourceLocation(startLine, startColumn)));
                lexeme = "0"; // Set to 0 to continue lexing
            }
        }
        
        return new Token(Language.TokenType.Number, new TokenValueNumber(lexeme, suffix, new SourceLocation(startLine, startColumn)));
    }

    private bool IsBinaryDigit(char peek)
    {
        return peek == '0' || peek == '1';
    }

    private bool IsHexDigit(char peek)
    {
        return IsDigit(peek) || (peek >= 'a' && peek <= 'f') || (peek >= 'A' && peek <= 'F');
    }

    private bool SkipComments()
    {
        if (Peek() == Language.LineComment[0] && Peek(1) == Language.LineComment[1])
        {
            while (Peek() != '\n' && !IsAtEnd()) Advance();
            return true;
        }

        if (Peek() == Language.BlockCommentStart[0] && Peek(1) == Language.BlockCommentStart[1])
        {
            Advance(); // /
            Advance(); // *
            while (!(Peek() == Language.BlockCommentEnd[0] && Peek(1) == Language.BlockCommentEnd[1]) && !IsAtEnd())
            {
                Advance();
            }
            if (!IsAtEnd())
            {
                Advance(); // *
                Advance(); // /
            }
            return true;
        }

        return false;
    }

    private bool IsAtEnd() => _currentIndex >= _sourceCode.Length;
    
    private char Peek(int offset = 0)
    {
        if (_currentIndex + offset >= _sourceCode.Length) return '\0';
        return _sourceCode[_currentIndex + offset];
    }
    
    private char Advance()
    {
        if (IsAtEnd()) return '\0';
        char c = _sourceCode[_currentIndex++];
        if (c == '\n')
        {
            _currentLine++;
            _currentColumn = 0;
        }
        else
        {
            _currentColumn++; 
        }
        
        return c;
    }
    
    private bool IsWhitespace(char c) => char.IsWhiteSpace(c);
    private bool IsLetter(char c) => char.IsLetter(c) || c == '_';
    private bool IsDigit(char c) => char.IsDigit(c);
    private bool IsAlphanumeric(char c) => IsLetter(c) || IsDigit(c);
    private bool IsSymbol(char c) => Language.Operators.ContainsKey(c.ToString()) || Language.Syntaxes.ContainsKey(c.ToString());
}