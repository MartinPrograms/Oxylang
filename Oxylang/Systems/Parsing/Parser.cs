using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems.Parsing;

public class Parser(List<Token> _tokens, SourceFile _sourceFile) : ICompilerSystem<Root>
{
    private ILogger _logger;
    public ILogger Logger => _logger;
    public SourceFile SourceFile => _sourceFile;
    public SourceLocation CurrentLocation => Peek()?.Lexeme.Location ?? new SourceLocation(1, 1);

    private int _currentIndex = 0;


    public Result<Root> Run(ILogger logger)
    {
        _logger = logger;

        var imports = new ImportParser().Parse(this);
        var structs = new StructDefinitionParser().Parse(this);
        var variables = new GlobalVariableDefinitionParser().Parse(this);
        var functions = new FunctionDefinitionParser().Parse(this);

        if (logger.HasErrors()) return new Result<Root>(null);
        return new Result<Root>(new Root(imports, structs, variables, functions));
    }

    public VariableDeclaration? ParseVariableDeclaration()
    {
        var identifier = MatchIdentifier();
        if (identifier == null) return null;
        
        TypeNode? type = null;
        if (MatchSyntax(Language.Syntax.Colon) != null)
        {
            type = ParseType();
            if (type == null) return null;
        }

        var initializer = (Expression?)null;

        if (MatchOperator(Language.Operator.Equals) != null)
        {
            initializer = ParseExpression();
            if (initializer == null) return null;
        }

        return new VariableDeclaration(identifier.Value, type, [], initializer, identifier.Location);
    }

    public Expression? ParseExpression(float minPrecedence = Language.LowestPrecedence)
    {
        var left = ParsePrimary();
        if (left == null) return null;

        while (true)
        {
            var opToken = Peek();
            if (opToken.Lexeme is not TokenValueOperator operatorLexeme)
                break;

            var op = operatorLexeme.Value;

            if (!Language.OperatorPrecedence.TryGetValue(op, out var prec))
                break;

            if (op is Language.Operator.Dot or Language.Operator.Arrow
                or Language.Operator.Increment or Language.Operator.Decrement)
                break;

            if (prec > minPrecedence)
                break;

            Advance();

            var isLeftAssoc = Language.OperatorAssociativity.GetValueOrDefault(op, true);
            var right = ParseExpression(isLeftAssoc ? prec - 1 : prec);
            if (right == null) return null;
            
            left = BuildBinaryExpression(left, op, right);
        }

        return left;
    }

    private Expression BuildBinaryExpression(Expression left, Language.Operator op, Expression right)
    {
        // Region: Compound assignment desugaring
        Language.Operator? desugaredOp = op switch
        {
            Language.Operator.PlusEquals     => Language.Operator.Plus,
            Language.Operator.MinusEquals    => Language.Operator.Minus,
            Language.Operator.AsteriskEquals => Language.Operator.Asterisk,
            Language.Operator.SlashEquals    => Language.Operator.Slash,
            Language.Operator.PercentEquals  => Language.Operator.Percent,
            _                                => null
        };

        if (desugaredOp.HasValue)
        {
            if (left is not LeftValue leftValue)
            {
                Logger.LogError($"Left-hand side of compound assignment must be a left-value", SourceFile, CurrentLocation);
                return new BinaryExpression(CurrentLocation, left, op, right);
            }
            
            return new AssignmentExpression(CurrentLocation, leftValue,
                new BinaryExpression(CurrentLocation, left, desugaredOp.Value, right));
        }

        if (op == Language.Operator.Equals)
        {
            if (left is not LeftValue leftValue)
            {
                Logger.LogError($"Left-hand side of assignment must be a left-value", SourceFile, CurrentLocation);
                return new BinaryExpression(CurrentLocation, left, op, right);
            }
            
            return new AssignmentExpression(CurrentLocation, leftValue, right);
        }

        return new BinaryExpression(CurrentLocation, left, op, right);
    }

    private Expression? ParsePrimary()
    {
        var atom = ParseAtom();
        if (atom == null) return null;

        // Handle . and -> and ++ and -- operators for member access and postfix increment/decrement
        while (true)
        {
            var opToken = Peek();
            if (opToken.Lexeme is TokenValueOperator operatorToken &&
                (operatorToken.Value == Language.Operator.Dot || operatorToken.Value == Language.Operator.Arrow))
            {
                Advance();

                var memberToken = MatchIdentifier();
                if (memberToken == null)
                {
                    Logger.LogError("Expected identifier after '.' or '->'", SourceFile, CurrentLocation);
                    return null;
                }

                atom = new MemberAccessExpression(CurrentLocation, atom, memberToken.Value, isPointerAccess: operatorToken.Value == Language.Operator.Arrow);
                continue;
            }
            if (opToken.Lexeme is TokenValueOperator incDecToken &&
                (incDecToken.Value == Language.Operator.Increment || incDecToken.Value == Language.Operator.Decrement))
            {
                Advance();
                atom = new PostfixExpression(CurrentLocation, atom, incDecToken.Value);
                continue;
            }
            else
            {
                break;
            }
        }

        return atom;
    }
    
    private Expression? ParseAtom()
    {
        if (MatchSyntax(Language.Syntax.LeftParen) != null)
        {
            var expression = ParseExpression();
            if (expression == null) return null;
            if (MatchSyntax(Language.Syntax.RightParen) == null)
            {
                Logger.LogError("Expected ')' after expression", SourceFile, CurrentLocation);
                return null;
            }
            return expression;
        }
     
        if (MatchOperator(Language.Operator.Minus) != null)
        {
            var expression = ParseExpression(Language.OperatorPrecedence[Language.Operator.Minus]);
            if (expression == null) return null;
            return new UnaryExpression(CurrentLocation, Language.Operator.Minus, expression);
        }
        
        if (MatchOperator(Language.Operator.Plus) != null)
        {
            var expression = ParseExpression(Language.OperatorPrecedence[Language.Operator.Plus]);
            if (expression == null) return null;
            return new UnaryExpression(CurrentLocation, Language.Operator.Plus, expression);
        }
        
        if (MatchOperator(Language.Operator.BitwiseNot) != null)
        {
            var expression = ParseExpression(Language.OperatorPrecedence[Language.Operator.BitwiseNot]);
            if (expression == null) return null;
            return new UnaryExpression(CurrentLocation, Language.Operator.BitwiseNot, expression);
        }
        
        if (MatchOperator(Language.Operator.Not) != null)
        {
            var expression = ParseExpression(Language.OperatorPrecedence[Language.Operator.Not]);
            if (expression == null) return null;
            return new UnaryExpression(CurrentLocation, Language.Operator.Not, expression);
        }
        
        if (MatchKeyword(Language.Keyword.True) != null)
        {
            return new LiteralExpression(CurrentLocation, 1, new PrimaryType(CurrentLocation, PrimaryType.PrimaryTypeKind.U8));
        }
        
        if (MatchKeyword(Language.Keyword.False) != null)
        {
            return new LiteralExpression(CurrentLocation, 0, new PrimaryType(CurrentLocation, PrimaryType.PrimaryTypeKind.U8));
        }
        
        if (MatchKeyword(Language.Keyword.Null) != null)
        {
            return new LiteralExpression(CurrentLocation, 0, new PointerType(CurrentLocation, new VoidType(CurrentLocation)));
        }
        
        // Match sizeof<type> 
        if (MatchKeyword(Language.Keyword.Sizeof) != null)
        {
            if (MatchOperator(Language.Operator.LessThan) == null) return null;
            var type = ParseType();
            if (type == null) return null;
            if (CloseGeneric() == null) return null;

            return new SizeOfExpression(CurrentLocation, type);
        }
        
        // Match alignof<type>
        if (MatchKeyword(Language.Keyword.Alignof) != null)
        {
            if (MatchOperator(Language.Operator.LessThan) == null) return null;
            var type = ParseType();
            if (type == null) return null;
            if (CloseGeneric() == null) return null;

            return new AlignOfExpression(CurrentLocation, type);
        }
        
        // Match cast<type>(expression)
        if (MatchKeyword(Language.Keyword.Cast) != null)
        {
            if (MatchOperator(Language.Operator.LessThan) == null) return null;
            var type = ParseType();
            if (type == null) return null;
            if (CloseGeneric() == null) return null;

            if (MatchSyntax(Language.Syntax.LeftParen) == null)
            {
                Logger.LogError("Expected '(' after cast type", SourceFile, CurrentLocation);
                return null;
            }

            var expression = ParseExpression();
            if (expression == null) return null;
            if (MatchSyntax(Language.Syntax.RightParen) == null)
            {
                Logger.LogError("Expected ')' after cast expression", SourceFile, CurrentLocation);
                return null;
            }

            return new CastExpression(CurrentLocation, type, expression);
        }
        
        // Match addr(expression)
        if (MatchKeyword(Language.Keyword.Addr) != null)
        {
            if (MatchSyntax(Language.Syntax.LeftParen) == null)
            {
                Logger.LogError("Expected '(' after 'addr'", SourceFile, CurrentLocation);
                return null;
            }

            var expression = ParseExpression();
            if (expression == null) return null;

            if (MatchSyntax(Language.Syntax.RightParen) == null)
            {
                Logger.LogError("Expected ')' after 'addr' expression", SourceFile, CurrentLocation);
                return null;
            }
            
            if (expression is not LeftValue leftValue)
            {
                Logger.LogError("Expression in 'addr' must be a left-value", SourceFile, CurrentLocation);
                return null;
            }
            
            return new AddressOfExpression(CurrentLocation, leftValue);
        }
        
        // Match deref(expression)
        if (MatchKeyword(Language.Keyword.Deref) != null)
        {
            if (MatchSyntax(Language.Syntax.LeftParen) == null)
            {
                Logger.LogError("Expected '(' after 'deref'", SourceFile, CurrentLocation);
                return null;
            }

            var expression = ParseExpression();
            if (expression == null) return null;

            if (MatchSyntax(Language.Syntax.RightParen) == null)
            {
                Logger.LogError("Expected ')' after 'deref' expression", SourceFile, CurrentLocation);
                return null;
            }
            
            return new DereferenceExpression(CurrentLocation, expression);
        }

        var numberToken = MatchNumber();
        if (numberToken != null)
        {
            // Determine the type of the number based on its suffix
            PrimaryType type;
            PrimaryType.PrimaryTypeKind kind;
            if (Enum.TryParse(numberToken.Type, true, out kind))
            {
                type = new PrimaryType(CurrentLocation, kind);
            }
            else
            {
                if (numberToken.Type == "f")
                {
                    type = new PrimaryType(CurrentLocation, PrimaryType.PrimaryTypeKind.F32);
                }
                else
                {
                    if (numberToken.Type != "")
                    {
                        Logger.LogError($"Unknown number literal suffix '{numberToken.Type}'", SourceFile, CurrentLocation);
                    }
                    
                    if (numberToken.Value.Contains('.'))
                        type = new PrimaryType(CurrentLocation, PrimaryType.PrimaryTypeKind.F64);
                    else
                        type = new PrimaryType(CurrentLocation, PrimaryType.PrimaryTypeKind.I32);
                }
            }

            if (type.Kind == PrimaryType.PrimaryTypeKind.F32 || type.Kind == PrimaryType.PrimaryTypeKind.F64)
            {
                if (!float.TryParse(numberToken.Value, out float floatValue))
                {
                    Logger.LogError($"Invalid float literal '{numberToken.Value}'", SourceFile, CurrentLocation);
                    return null;
                }

                return new LiteralExpression(CurrentLocation, floatValue, type);
            }
            
            if (!ulong.TryParse(numberToken.Value, out ulong intValue))
            {
                Logger.LogError($"Invalid integer literal '{numberToken.Value}'", SourceFile, CurrentLocation);
                return null;
            }

            return new LiteralExpression(CurrentLocation, intValue, type);
        }
        
        var stringToken = MatchString();
        if (stringToken != null)
        {
            return new StringExpression(CurrentLocation, stringToken.Value);
        }
        
        var identifierToken = MatchIdentifier();
        if (identifierToken != null)
        {
            var indexBackup = _currentIndex;
            var madeMistake = false;
            List<TypeNode> genericArgs = new();
            if (MatchOperator(Language.Operator.LessThan) != null)
            {
                do
                {
                    var argType = ParseType();
                    if (argType == null)
                    {
                        _currentIndex = indexBackup;
                        madeMistake = true;
                        break;
                    }
                    genericArgs.Add(argType);
                } while (MatchSyntax(Language.Syntax.Comma) != null);

                if (!madeMistake && CloseGeneric() == null)
                {
                    _currentIndex = indexBackup;
                    madeMistake = true;
                    genericArgs.Clear();
                }
            }

            // If the next token is a left parenthesis, this is a function call
            if (MatchSyntax(Language.Syntax.LeftParen) != null)
            {
                List<Expression> arguments = new();
                if (MatchSyntax(Language.Syntax.RightParen) == null)
                {
                    do
                    {
                        var arg = ParseExpression();
                        if (arg == null) return null;
                        arguments.Add(arg);
                    } while (MatchSyntax(Language.Syntax.Comma) != null);

                    if (MatchSyntax(Language.Syntax.RightParen) == null)
                    {
                        Logger.LogError("Expected ')' after function call arguments", SourceFile, CurrentLocation);
                        return null;
                    }
                }

                return new FunctionCallExpression(CurrentLocation, identifierToken.Value, genericArgs, arguments);
            }

            // Peek if the next token is a left brace, if so, parse it as a struct initializer
            if (MatchSyntax(Language.Syntax.LeftBrace) != null)
            {
                List<(string, Expression)> fields = new();
                if (MatchSyntax(Language.Syntax.RightBrace) == null)
                {
                    do
                    {
                        if (Peek().Lexeme is TokenValueSyntax syntax && syntax.Value == Language.Syntax.RightBrace)
                        {
                            break;
                        }
                        
                        var fieldNameToken = MatchIdentifier();
                        if (fieldNameToken == null)
                        {
                            Logger.LogError("Expected identifier for struct field name", SourceFile, CurrentLocation);
                            return null;
                        }

                        if (MatchSyntax(Language.Syntax.Colon) == null)
                        {
                            Logger.LogError("Expected ':' after struct field name", SourceFile, CurrentLocation);
                            return null;
                        }

                        var fieldValue = ParseExpression();
                        if (fieldValue == null) return null;

                        fields.Add((fieldNameToken.Value, fieldValue));
                    } while (MatchSyntax(Language.Syntax.Comma) != null);

                    if (MatchSyntax(Language.Syntax.RightBrace) == null)
                    {
                        Logger.LogError("Expected '}' after struct initializer fields", SourceFile, CurrentLocation);
                        return null;
                    }
                }

                return new StructInitializerExpression(CurrentLocation, identifierToken.Value, genericArgs, fields);
            }
            
            // Otherwise, it's a variable reference
            if (genericArgs.Count > 0)
            {
                _logger.LogError("Generic arguments are not allowed on variable references", SourceFile, CurrentLocation);
                return null;
            }
            
            return new VariableExpression(CurrentLocation, identifierToken.Value);
        }

        return null;
    }

    public TypeNode? ParseType()
    {
        if (MatchKeyword(Language.Keyword.Fn) != null)
        {
            // fn (<type>, <type>, ...) -> <type>
            if (MatchSyntax(Language.Syntax.LeftParen) == null) return null;

            List<TypeNode> parameterTypes = new();
            bool isVariadic = false;
            if (MatchSyntax(Language.Syntax.RightParen) == null)
            {
                do
                {
                    if (MatchOperator(Language.Operator.Variadic) != null)
                    {
                        isVariadic = true;
                        break;
                    }

                    var paramType = ParseType();
                    if (paramType == null) return null;
                    parameterTypes.Add(paramType);
                } while (MatchSyntax(Language.Syntax.Comma) != null);
            }

            if (MatchSyntax(Language.Syntax.RightParen) == null) return null;

            // Optional return type
            if (MatchOperator(Language.Operator.Arrow) != null)
            {
                var returnType = ParseType();
                if (returnType == null) return null;
                return new FunctionType(CurrentLocation, parameterTypes, returnType, isVariadic);
            }

            return new FunctionType(CurrentLocation, parameterTypes, new VoidType(CurrentLocation), isVariadic);
        }

        if (MatchIdentifier("void") != null)
        {
            return new VoidType(CurrentLocation);
        }
        
        if (MatchKeyword(Language.Keyword.Ptr) != null)
        {
            if (MatchOperator(Language.Operator.LessThan) == null) return null;

            var pointedType = ParseType();
            if (CloseGeneric() == null) return null;

            if (pointedType == null) return null;
            return new PointerType(CurrentLocation, pointedType);
        }

        if (MatchKeyword(Language.Keyword.Array) != null)
        {
            // array<type, size>
            if (MatchOperator(Language.Operator.LessThan) == null) return null;
            var elementType = ParseType();
            if (elementType == null) return null;
            if (MatchSyntax(Language.Syntax.Comma) == null) return null;
            var sizeToken = MatchNumber();
            if (sizeToken == null)
            {
                Logger.LogError("Expected number literal for array size", SourceFile, CurrentLocation);
                return null;
            }

            if (CloseGeneric() == null) return null;

            if (sizeToken.Type != "" && sizeToken.Type != "i32")
            {
                Logger.LogError("Array size must be i32", SourceFile, CurrentLocation);
                return null;
            }

            return new ArrayType(CurrentLocation, elementType, int.Parse(sizeToken.Value));
        }

        var identifier = MatchIdentifier();
        if (identifier == null) return null;

        // Check if the identifier is u8, i8, u16, i16, u32, i32, u64, i64, f32, or f64 for primitive types
        if (identifier.Value == "u8" || identifier.Value == "i8" ||
            identifier.Value == "u16" || identifier.Value == "i16" ||
            identifier.Value == "u32" || identifier.Value == "i32" ||
            identifier.Value == "u64" || identifier.Value == "i64" ||
            identifier.Value == "f32" || identifier.Value == "f64")
        {
            return new PrimaryType(CurrentLocation,
                Enum.Parse<PrimaryType.PrimaryTypeKind>(identifier.Value.ToUpper()));
        }

        List<TypeNode> genericArgs = new();

        if (MatchOperator(Language.Operator.LessThan) != null)
        {
            // Generic type, e.g. Vec<T>
            do
            {
                var argType = ParseType();
                if (argType == null) return null;
                genericArgs.Add(argType);
            } while (MatchSyntax(Language.Syntax.Comma) != null);

            if (CloseGeneric() == null) return null;
        }

        return new NamedType(CurrentLocation, identifier.Value, genericArgs);
    }

    public TokenValueOperator? CloseGeneric()
    {
        if (MatchOperator(Language.Operator.GreaterThan) == null)
        {
            if (MatchOperator(Language.Operator.RightShift) != null)
            {
                _currentIndex -= 1;
                // Handle >> as two > operators for closing nested generics
                _tokens[_currentIndex] = new Token(Language.TokenType.Operator,
                    new TokenValueOperator(Language.Operator.GreaterThan, CurrentLocation));
                return new TokenValueOperator(Language.Operator.GreaterThan, CurrentLocation);
            }

            return null;
        }

        return new TokenValueOperator(Language.Operator.GreaterThan, CurrentLocation);
    }

    public List<Attribute> ParseAttributes()
    {
        if (MatchSyntax(Language.Syntax.AtSign) == null) return new List<Attribute>();
        List<Attribute> attributes = new();
        do
        {
            var nameToken = MatchIdentifier();
            if (nameToken == null)
            {
                Logger.LogError("Expected identifier for attribute name", SourceFile, CurrentLocation);
                return attributes;
            }

            List<string> args = new();
            if (MatchSyntax(Language.Syntax.LeftParen) != null)
            {
                do
                {
                    var argToken = MatchString();
                    if (argToken == null)
                    {
                        Logger.LogError("Expected string literal for attribute argument", SourceFile, CurrentLocation);
                        return attributes;
                    }

                    args.Add(argToken.Value);
                } while (MatchSyntax(Language.Syntax.Comma) != null);

                if (MatchSyntax(Language.Syntax.RightParen) == null)
                {
                    Logger.LogError("Expected ')' to close attribute arguments", SourceFile, CurrentLocation);
                    return attributes;
                }
            }

            attributes.Add(new Attribute(nameToken.Value, args));
        } while (MatchSyntax(Language.Syntax.AtSign) != null);

        return attributes;
    }

    public TokenValue? Match(Language.TokenType expectedType, string? expectedValue = null)
    {
        if (ErrorOnEOF(expectedType, expectedValue)) return null;

        var token = _tokens[_currentIndex];
        if (token.Type != expectedType || (expectedValue != null && token.Lexeme.ToString() != expectedValue))
        {
            return null;
        }

        _currentIndex++;
        return token.Lexeme;
    }

    public TokenValueIdentifier? MatchIdentifier(string? expectedValue = null) =>
        Match(Language.TokenType.Identifier, expectedValue) as TokenValueIdentifier;

    public TokenValueNumber? MatchNumber(string? expectedValue = null) =>
        Match(Language.TokenType.Number, expectedValue) as TokenValueNumber;

    public TokenValueString? MatchString(string? expectedValue = null) =>
        Match(Language.TokenType.String, expectedValue) as TokenValueString;

    public TokenValueOperator? MatchOperator(Language.Operator op) =>
        Match(Language.TokenType.Operator, op.ToString()) as TokenValueOperator;

    public TokenValueSyntax? MatchSyntax(Language.Syntax syntax) =>
        Match(Language.TokenType.Syntax, syntax.ToString()) as TokenValueSyntax;

    public TokenValue? MatchKeyword(Language.Keyword keyword) =>
        Match(Language.TokenType.Keyword, keyword.ToString().ToLower()) as TokenValue;


    public bool ErrorOnEOF(Language.TokenType expectedType, string? expectedValue)
    {
        if (_currentIndex >= _tokens.Count)
        {
            _logger.LogError($"Unexpected end of input, expected {expectedType} '{expectedValue}'", _sourceFile,
                new SourceLocation(_tokens.Last().Lexeme.Location.Line,
                    _tokens.Last().Lexeme.Location.Column + _tokens.Last().Lexeme.ToString().Length));
            return true;
        }

        return false;
    }

    public void Reset()
    {
        _currentIndex = 0;
    }

    public Token? Peek(int offset = 0)
    {
        if (_currentIndex + offset >= _tokens.Count)
        {
            return null;
        }

        return _tokens[_currentIndex + offset];
    }

    public void Advance()
    {
        if (_currentIndex < _tokens.Count)
        {
            _currentIndex++;
        }
    }

    public bool IsAtEnd()
    {
        return _currentIndex >= _tokens.Count;
    }

    public bool IsGlobalScope()
    {
        // Count the amount of unmatched opening and closing braces before the current token
        int braceCount = 0;
        for (int i = 0; i < _currentIndex; i++)
        {
            var token = _tokens[i];
            if (token.Type == Language.TokenType.Syntax)
            {
                if (token.Lexeme is TokenValueSyntax syntax)
                {
                    if (syntax.Value == Language.Syntax.LeftBrace)
                    {
                        braceCount++;
                    }
                    else if (syntax.Value == Language.Syntax.RightBrace)
                    {
                        braceCount--;
                    }
                }
            }
        }

        return braceCount == 0;
    }

    public BlockStatement? ParseBlock()
    {
        List<Node> statements = new();
        int braceCount = 1; // We start after consuming the opening brace
        
        while (!IsAtEnd() && braceCount > 0)
        {
            var token = Peek();
            if (token.Type == Language.TokenType.Syntax)
            {
                if (token.Lexeme is TokenValueSyntax syntax)
                {
                    if (syntax.Value == Language.Syntax.LeftBrace)
                    {
                        braceCount++;
                    }
                    else if (syntax.Value == Language.Syntax.RightBrace)
                    {
                        braceCount--;
                        if (braceCount == 0)
                        {
                            Advance(); // Consume the closing brace
                            break;
                        }
                    }
                }
            }

            var statement = ParseStatement();
            if (statement != null)
            {
                statements.Add(statement);
            }
            else
            {
                Advance(); // Skip invalid tokens to avoid infinite loop
            }
        }
        
        if (braceCount != 0)
        {
            Logger.LogError("Expected '}' to close block", SourceFile, CurrentLocation);
            return null;
        }

        return new BlockStatement(CurrentLocation, statements);
    }

    private Node? ParseStatement()
    {
        return new StatementParser().Parse(this);
    }
}