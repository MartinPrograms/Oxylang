namespace Oxylang;

public class Language
{
    public const string LineComment = "//";
    public const string BlockCommentStart = "/*";
    public const string BlockCommentEnd = "*/";
    public const string StringDelimiter = "\"";

    public enum Architecture
    {
        Amd64,
        Arm64,
        RiscV64
    }
    
    public enum TokenType
    {
        Identifier, // [A-Za-z_][A-Za-z0-9_]*
        Number,
        String,
        Operator,
        Keyword,
        Syntax,
        EOF
    }

    public enum Keyword
    {
        Fn, // - function declaration
        Let, // - variable declaration
        Return,
        If,
        Else,
        While,
        For, // - C like for (expression; expression; expression) { ... }
        Struct,
        Import, // - importing a module
        As, // - importing a module
        Ptr,
        Array,
        True,
        False,
        Null,
        Sizeof,
        Alignof,
        Typeof,
        Addr,
        Deref,
        Cast,
        Allocate,
        Free,
        Break,
        Continue,
    }

    public static readonly HashSet<string> Keywords = Enum.GetNames(typeof(Keyword)).Select(k => k.ToLower()).ToHashSet();

    public enum Operator
    {
        // Basic arithmetic
        Plus, // +
        Minus, // -
        Asterisk, // *
        Slash, // /
        Percent, // %
        
        // Comparison operators
        DoubleEquals, // ==
        NotEquals, // !=
        LessThan, // <
        GreaterThan, // >
        LessThanOrEqual, // <=
        GreaterThanOrEqual, // >=
        
        // Logical operators
        And, // &&
        Or, // ||
        Not, // !
        
        // Bitwise operators
        BitwiseAnd, // &
        BitwiseOr, // |
        BitwiseXor, // ^
        BitwiseNot, // ~
        LeftShift, // <<
        RightShift, // >>
        
        // Other operators
        Increment, // ++
        Decrement, // --
        Arrow, // ->
        Dot, // .
        NamespaceAccess, // ::
        Variadic, // ...
        
        // Assignment operators
        Equals, // =
        PlusEquals, // +=
        MinusEquals, // -=
        AsteriskEquals, // *=
        SlashEquals, // /=
        PercentEquals, // %=
    }
    
    public static readonly Dictionary<string, Operator> Operators = new()
    {
        { "+", Operator.Plus },
        { "-", Operator.Minus },
        { "*", Operator.Asterisk },
        { "/", Operator.Slash },
        { "%", Operator.Percent },
        { "==", Operator.DoubleEquals },
        { "!=", Operator.NotEquals },
        { "<", Operator.LessThan },
        { ">", Operator.GreaterThan },
        { "<=", Operator.LessThanOrEqual },
        { ">=", Operator.GreaterThanOrEqual },
        { "&&", Operator.And },
        { "||", Operator.Or },
        { "!", Operator.Not },
        { "&", Operator.BitwiseAnd },
        { "|", Operator.BitwiseOr },
        { "^", Operator.BitwiseXor },
        { "~", Operator.BitwiseNot },
        { "<<", Operator.LeftShift },
        { ">>", Operator.RightShift },
        { "++", Operator.Increment },
        { "--", Operator.Decrement },
        { "->", Operator.Arrow },
        { ".", Operator.Dot },
        { "::", Operator.NamespaceAccess },
        { "=", Operator.Equals },
        { "+=", Operator.PlusEquals },
        { "-=", Operator.MinusEquals },
        { "*=", Operator.AsteriskEquals },
        { "/=", Operator.SlashEquals },
        { "%=", Operator.PercentEquals },
        { "...", Operator.Variadic }
    };
    
    // Lower = higher precedence
    public const float LowestPrecedence = float.MaxValue;
    public const float HighestPrecedence = float.MinValue;
    public static readonly Dictionary<Operator, float> OperatorPrecedence = new()
    {
        { Operator.Increment, 0 },
        { Operator.Decrement, 0 },
        { Operator.Arrow, 2 },
        { Operator.Dot, 2 },
        { Operator.NamespaceAccess, 2 },
        { Operator.Plus, 5 },
        { Operator.Minus, 5 },
        { Operator.Asterisk, 4 },
        { Operator.Slash, 4 },
        { Operator.Percent, 4 },
        { Operator.DoubleEquals, 6 },
        { Operator.NotEquals, 6 },
        { Operator.LessThan, 6 },
        { Operator.GreaterThan, 6 },
        { Operator.LessThanOrEqual, 6 },
        { Operator.GreaterThanOrEqual, 6 },
        { Operator.And, 7 },
        { Operator.Or, 7 },
        { Operator.Not, 3 },
        { Operator.BitwiseAnd, 8 },
        { Operator.BitwiseOr, 8 },
        { Operator.BitwiseXor, 8 },
        { Operator.BitwiseNot, 8 },
        { Operator.LeftShift, 8 },
        { Operator.RightShift, 8 },
        { Operator.Equals, 9 },
        { Operator.PlusEquals, 9 },
        { Operator.MinusEquals, 9 },
        { Operator.AsteriskEquals, 9 },
        { Operator.SlashEquals, 9 },
        { Operator.PercentEquals, 9 }
    };
    
    // True if the operator is left associative, false if right associative
    public static readonly Dictionary<Operator, bool> OperatorAssociativity = new()
    {
        { Operator.Increment, false },
        { Operator.Decrement, false },
        { Operator.Arrow, false },
        { Operator.Dot, false },
        { Operator.NamespaceAccess, false },
        { Operator.Plus, true },
        { Operator.Minus, true },
        { Operator.Asterisk, true },
        { Operator.Slash, true },
        { Operator.Percent, true },
        { Operator.DoubleEquals, true },
        { Operator.NotEquals, true },
        { Operator.LessThan, true },
        { Operator.GreaterThan, true },
        { Operator.LessThanOrEqual, true },
        { Operator.GreaterThanOrEqual, true },
        { Operator.And, true },
        { Operator.Or, true },
        { Operator.Not, false },
        { Operator.BitwiseAnd, true },
        { Operator.BitwiseOr, true },
        { Operator.BitwiseXor, true },
        { Operator.BitwiseNot, false },
        { Operator.LeftShift, true },
        { Operator.RightShift, true },
        { Operator.Equals, false },
        { Operator.PlusEquals, false },
        { Operator.MinusEquals, false },
        { Operator.AsteriskEquals, false },
        { Operator.SlashEquals, false },
        { Operator.PercentEquals, false }
    };

    // C like syntax ( { }, ( ), [ ], ;, , : ) and @ for attributes
    public enum Syntax
    {
        LeftBrace, // {
        RightBrace, // }
        LeftParen, // (
        RightParen, // )
        LeftBracket, // [
        RightBracket, // ]
        Semicolon, // ;
        Comma, // ,
        Colon, // :
        AtSign, // @
    }
    
    public static readonly Dictionary<string, Syntax> Syntaxes = new()
    {
        { "{", Syntax.LeftBrace },
        { "}", Syntax.RightBrace },
        { "(", Syntax.LeftParen },
        { ")", Syntax.RightParen },
        { "[", Syntax.LeftBracket },
        { "]", Syntax.RightBracket },
        { ";", Syntax.Semicolon },
        { ",", Syntax.Comma },
        { ":", Syntax.Colon },
        { "@", Syntax.AtSign }
    };
}