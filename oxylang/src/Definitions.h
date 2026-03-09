#ifndef OXYLANG_DEFINITIONS_H
#define OXYLANG_DEFINITIONS_H
#include <map>
#include <string>
#include <utility>
/*

fn - function declaration
let - variable declaration
return
if
else
while
struct
import - importing a module
from - importing a module
ptr
array
true
false
null
sizeof
alignof
typeof
addr
deref
cast
allocate
free

Note: sizeof, alignof and typeof are calculated during compilation. They do not happen during runtime.
*/
namespace Oxy {
    enum class Keyword {
        Fn,
        Let,
        Return,
        If,
        Else,
        While,
        Struct,
        Import,
        As,
        Ptr,
        Array,
        True,
        False,
        Null,
        SizeOf,
        AlignOf,
        TypeOf,
        Addr,
        Deref,
        Cast,
        Allocate,
        Free,
        Break,
        Continue,
        For
    };

    inline std::map<Keyword, std::string> KeywordToString = {
        {Keyword::Fn, "fn"},
        {Keyword::Let, "let"},
        {Keyword::Return, "return"},
        {Keyword::If, "if"},
        {Keyword::Else, "else"},
        {Keyword::While, "while"},
        {Keyword::Struct, "struct"},
        {Keyword::Import, "import"},
        {Keyword::As, "as"},
        {Keyword::Ptr, "ptr"},
        {Keyword::Array, "array"},
        {Keyword::True, "true"},
        {Keyword::False, "false"},
        {Keyword::Null, "null"},
        {Keyword::SizeOf, "sizeof"},
        {Keyword::AlignOf, "alignof"},
        {Keyword::TypeOf, "typeof"},
        {Keyword::Addr, "addr"},
        {Keyword::Deref, "deref"},
        {Keyword::Cast, "cast"},
        {Keyword::Allocate, "allocate"},
        {Keyword::Free, "free"},
        {Keyword::Break, "break"},
        {Keyword::Continue, "continue"},
        {Keyword::For, "for"}
    };

    /* Literal types:
    value types:
    integers:

    i8
    u8
    i16
    u16
    i32
    u32
    i64
    u64
    int -> i32 regardless of architecture
    long -> i64 ^^^

    floating point numbers:

    f32
    f64
    float -> f32 regardless of architecture
    double -> f64 ^^^
    */

    enum class LiteralType {
        Int, // i32
        Long, // i64
        Float, // f32
        Double, // f64
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        F32,
        F64,
        Void,
        Undefined,
        Pointer,
        UserDefined
    };

    inline std::map<LiteralType, std::string> LiteralToString = {
        {LiteralType::I8, "i8"},
        {LiteralType::U8, "u8"},
        {LiteralType::I16, "i16"},
        {LiteralType::U16, "u16"},
        {LiteralType::I32, "i32"},
        {LiteralType::U32, "u32"},
        {LiteralType::I64, "i64"},
        {LiteralType::U64, "u64"},
        {LiteralType::F32, "f32"},
        {LiteralType::F64, "f64"},
        {LiteralType::Void, "void"},
        {LiteralType::Pointer, "ptr"},
        {LiteralType::UserDefined, "user_defined"}
    };

    class Type {
    public:
        Type(LiteralType literalType, long count = 0, Type *nestedType = nullptr, std::string identifier = "") : literalType(literalType), count(count), nestedType(nestedType), identifier(std::move(identifier)) {}

        virtual std::string ToString() const {
            if (literalType == LiteralType::UserDefined) {
                return identifier;
            }

            std::string result = LiteralToString[literalType];
            if (nestedType) {
                if (literalType == LiteralType::Pointer) {
                    result += "<" + nestedType->ToString() + ">";
                } else if (count > 0) {
                    result += nestedType->ToString() + "[" + std::to_string(count) + "]";
                }
            }
            return result;
        }

        [[nodiscard]] LiteralType GetLiteralType() const { return literalType; }
        [[nodiscard]] long GetCount() const { return count; }
        [[nodiscard]] Type* GetNestedType() const { return nestedType; }
        [[nodiscard]] const std::string& GetIdentifier() const { return identifier; }

    private:
        LiteralType literalType;
        long count; // For array types, this is the number of elements. For other types, it can be ignored or set to 0.
        Type *nestedType; // For pointer and array types, this points to the type of the elements
        std::string identifier; // For user defined types, this is the name of the type
    };

/*
 Operators

    Operator 	Description 	Associativity 	Precedence
    () 	Parentheses 	Left to Right 	1
    [] 	Array subscript 	Left to Right 	2
    . 	Member access 	Left to Right 	2
    addr, deref, cast<T> 	Unary operators 	Right to Left 	3
    *, /, % 	Multiplication, Division, Modulus 	Left to Right 	4
    +, - 	Addition, Subtraction 	Left to Right 	5
    && 	Logical AND 	Left to Right 	6
    || 	Logical OR 	Left to Right 	6
    <<, >> 	Bitwise shift left/right 	Left to Right 	7
    & 	Bitwise AND 	Left to Right 	7
    ^ 	Bitwise XOR 	Left to Right 	7
    | 	Bitwise OR 	Left to Right 	7
    <, <=, >, >= 	Comparison operators 	Left to Right 	8
    ==, != 	Equality operators 	Left to Right 	8
    = 	Assignment 	Right to Left 	9
    +=, -=, *=, /=, %= 	Compound assignment operators 	Right to Left 	9
    ++, -- 	Increment, Decrement 	Right to Left 	0
    */

    enum class Operator {
        Parentheses,
        ArraySubscript,
        MemberAccess,
        Addr,
        Deref,
        Cast,
        Multiply,
        Divide,
        Modulus,
        Add,
        Subtract,
        LogicalAnd,
        LogicalOr,
        ShiftLeft,
        ShiftRight,
        BitwiseAnd,
        BitwiseXor,
        BitwiseOr,
        LessThan,
        LessThanOrEqual,
        GreaterThan,
        GreaterThanOrEqual,
        Equal,
        NotEqual,
        Assignment,
        CompoundAssignmentAdd,
        CompoundAssignmentSubtract,
        CompoundAssignmentMultiply,
        CompoundAssignmentDivide,
        CompoundAssignmentModulus,
        Increment,
        Decrement,
        Arrow,
        Variadic
    };

    inline std::map<Operator, std::string> OperatorToString = {
        {Operator::MemberAccess, "."},
        {Operator::Multiply, "*"},
        {Operator::Divide, "/"},
        {Operator::Modulus, "%"},
        {Operator::Add, "+"},
        {Operator::Subtract, "-"},
        {Operator::LogicalAnd, "&&"},
        {Operator::LogicalOr, "||"},
        {Operator::ShiftLeft, "<<"},
        {Operator::ShiftRight, ">>"},
        {Operator::BitwiseAnd, "&"},
        {Operator::BitwiseXor, "^"},
        {Operator::BitwiseOr, "|"},
        {Operator::LessThan, "<"},
        {Operator::LessThanOrEqual, "<="},
        {Operator::GreaterThan, ">"},
        {Operator::GreaterThanOrEqual, ">="},
        {Operator::Equal, "=="},
        {Operator::NotEqual, "!="},
        {Operator::Assignment, "="},
        {Operator::CompoundAssignmentAdd, "+="},
        {Operator::CompoundAssignmentSubtract, "-="},
        {Operator::CompoundAssignmentMultiply, "*="},
        {Operator::CompoundAssignmentDivide, "/="},
        {Operator::CompoundAssignmentModulus, "%="},
        {Operator::Increment, "++"},
        {Operator::Decrement, "--"},
        {Operator::Arrow, "->"},
        {Operator::Variadic, "..."}
    };

    inline std::map<Operator, float> OperatorPrecedence = {
        {Operator::Parentheses, 1},
        {Operator::ArraySubscript, 2},
        {Operator::MemberAccess, 2},
        {Operator::Addr, 3},
        {Operator::Deref, 3},
        {Operator::Cast, 3},
        {Operator::Multiply, 4},
        {Operator::Divide, 4},
        {Operator::Modulus, 4},
        {Operator::Add, 5},
        {Operator::Subtract, 5},
        {Operator::LogicalAnd, 6},
        {Operator::LogicalOr, 6},
        {Operator::ShiftLeft, 7},
        {Operator::ShiftRight, 7},
        {Operator::BitwiseAnd, 7},
        {Operator::BitwiseXor, 7},
        {Operator::BitwiseOr, 7},
        {Operator::LessThan, 8},
        {Operator::LessThanOrEqual, 8},
        {Operator::GreaterThan, 8},
        {Operator::GreaterThanOrEqual, 8},
        {Operator::Equal, 8},
        {Operator::NotEqual, 8},
        {Operator::Assignment, 9},
        {Operator::CompoundAssignmentAdd, 9},
        {Operator::CompoundAssignmentSubtract, 9},
        {Operator::CompoundAssignmentMultiply, 9},
        {Operator::CompoundAssignmentDivide, 9},
        {Operator::CompoundAssignmentModulus, 9},
        {Operator::Increment, 0},
        {Operator::Decrement, 0},
        {Operator::Arrow, 11}
    };

    inline std::map<Operator, bool> OperatorRightAssociative = {
        {Operator::Parentheses, false},
        {Operator::ArraySubscript, false},
        {Operator::MemberAccess, false},
        {Operator::Addr, true},
        {Operator::Deref, true},
        {Operator::Cast, true},
        {Operator::Multiply, false},
        {Operator::Divide, false},
        {Operator::Modulus, false},
        {Operator::Add, false},
        {Operator::Subtract, false},
        {Operator::LogicalAnd, false},
        {Operator::LogicalOr, false},
        {Operator::ShiftLeft, false},
        {Operator::ShiftRight, false},
        {Operator::BitwiseAnd, false},
        {Operator::BitwiseXor, false},
        {Operator::BitwiseOr, false},
        {Operator::LessThan, false},
        {Operator::LessThanOrEqual, false},
        {Operator::GreaterThan, false},
        {Operator::GreaterThanOrEqual, false},
        {Operator::Equal, false},
        {Operator::NotEqual, false},
        {Operator::Assignment, true},
        {Operator::CompoundAssignmentAdd, true},
        {Operator::CompoundAssignmentSubtract, true},
        {Operator::CompoundAssignmentMultiply, true},
        {Operator::CompoundAssignmentDivide, true},
        {Operator::CompoundAssignmentModulus, true},
        {Operator::Increment, true},
        {Operator::Decrement, true},
        {Operator::Arrow, true}
    };

    inline float LOWEST_PRECEDENCE = 12.0f;
    inline float HIGHEST_PRECEDENCE = 0.0f;

    // Some other syntax ( { }, ;, , )
    enum class Syntax {
        LeftBrace,
        RightBrace,
        Semicolon,
        Colon,
        At,
        Comma,
        LeftParen,
        RightParen,
        LeftBracket,
        RightBracket
    };

    inline std::map<Syntax, std::string> SyntaxToString = {
        {Syntax::LeftBrace, "{"},
        {Syntax::RightBrace, "}"},
        {Syntax::Semicolon, ";"},
        {Syntax::Colon, ":"},
        {Syntax::At, "@"},
        {Syntax::Comma, ","},
        {Syntax::LeftParen, "("},
        {Syntax::RightParen, ")"},
        {Syntax::LeftBracket, "["},
        {Syntax::RightBracket, "]"}
    };
}

#endif //OXYLANG_DEFINITIONS_H