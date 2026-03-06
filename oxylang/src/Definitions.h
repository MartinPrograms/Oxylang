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
        From,
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
        Free
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
        {Keyword::From, "from"},
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
        {Keyword::Free, "free"}
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
        Int,
        Float,
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

        std::string ToString() const {
            std::string result = LiteralToString[literalType];
            if (nestedType) {
                if (literalType == LiteralType::Pointer) {
                    result += "<" + nestedType->ToString() + ">";
                } else if (count > 0) {
                    result += "[" + std::to_string(count) + "]" + "<" + nestedType->ToString() + ">";
                }
            }
            return result;
        }
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
    ++, -- 	Increment, Decrement 	Right to Left 	10
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
        Arrow
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
        {Operator::Arrow, "->"}
    };

    // Some other syntax ( { }, ;, , )
    enum class Syntax {
        LeftBrace,
        RightBrace,
        Semicolon,
        Colon,
        At,
        Comma
    };

    inline std::map<Syntax, std::string> SyntaxToString = {
        {Syntax::LeftBrace, "{"},
        {Syntax::RightBrace, "}"},
        {Syntax::Semicolon, ";"},
        {Syntax::Colon, ":"},
        {Syntax::At, "@"},
        {Syntax::Comma, ","}
    };
}

#endif //OXYLANG_DEFINITIONS_H