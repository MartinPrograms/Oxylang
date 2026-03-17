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
        UserDefined,
        Function,
        TypeOf
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
        {LiteralType::UserDefined, "user_defined"},
        {LiteralType::Function, "function"},
        {LiteralType::TypeOf, "typeof"}
    };

    class Type {
    public:
        Type(LiteralType literalType, long count = 0, Type * nestedType = {}, std::string identifier = "") : literalType(literalType), count(count), nestedType(nestedType), identifier(std::move(identifier)) {}

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

        // Override the C++ is equal operator to compare types based on their literal type, count, nested type and identifier.
        bool operator==(const Type& other) const {
            if (literalType != other.literalType) {
                return false;
            }

            if (count != other.count) {
                return false;
            }

            if (nestedType && other.nestedType) {
                if (!(*nestedType == *other.nestedType)) {
                    return false;
                }
            } else if (nestedType || other.nestedType) {
                return false; // One has a nested type and the other doesn't
            }

            if (literalType == LiteralType::UserDefined) {
                return identifier == other.identifier;
            }

            return true;
        }

    private:
        LiteralType literalType;
        long count; // For array types, this is the number of elements. For other types, it can be ignored or set to 0.
        Type *nestedType; // For pointer and array types, this points to the type of the elements
        std::string identifier; // For user defined types, this is the name of the type
    };

/*
 Operators

    (lower number = higher precedence)

    | operator                         | description                        | associativity | precedence |
    |----------------------------------|------------------------------------|---------------|------------|
    | `++`, `--`                       | post-increment/decrement           | left          | 0          |
    | `()`                             | nested expressions, function calls | left          | 1          |
    | `[]`, `.`                        | array access, member access        | left          | 2          |
    | `addr`, `deref`, `cast`          | pointer operations                 | right         | 3          |
    | `*`, `/`, `%`                    | multiplication, division, modulus  | left          | 4          |
    | `+`, `-`                         | addition, subtraction              | left          | 5          |
    | `<`, `<=`, `>`, `>=`, `==`, `!=` | equality operators                 | left          | 6          |
    | `&&`, `\|\|`                     | logical AND, OR                    | left          | 7          |
    | `<<`, `>>`, `&`, `^`, `\|`       | bitwise operators                  | left          | 8          |
    | `=`, `+=`, `-=`, etc.            | assignment operators               | right         | 9          |
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
        {Operator::Parentheses, 1.0f},
        {Operator::ArraySubscript, 2.0f},
        {Operator::MemberAccess, 2.0f},
        {Operator::Arrow, 2.0f},
        {Operator::Addr, 3.0f},
        {Operator::Deref, 3.0f},
        {Operator::Cast, 3.0f},
        {Operator::Multiply, 4.0f},
        {Operator::Divide, 4.0f},
        {Operator::Modulus, 4.0f},
        {Operator::Add, 5.0f},
        {Operator::Subtract, 5.0f},
        {Operator::LogicalAnd, 7.0f},
        {Operator::LogicalOr, 7.0f},
        {Operator::ShiftLeft, 8.0f},
        {Operator::ShiftRight, 8.0f},
        {Operator::BitwiseAnd, 8.0f},
        {Operator::BitwiseXor, 8.0f},
        {Operator::BitwiseOr, 8.0f},
        {Operator::LessThan, 6.0f},
        {Operator::LessThanOrEqual, 6.0f},
        {Operator::GreaterThan, 6.0f},
        {Operator::GreaterThanOrEqual, 6.0f},
        {Operator::Equal, 6.0f},
        {Operator::NotEqual, 6.0f},
        {Operator::Assignment, 9.0f},
        {Operator::CompoundAssignmentAdd, 9.0f},
        {Operator::CompoundAssignmentSubtract, 9.0f},
        {Operator::CompoundAssignmentMultiply, 9.0f},
        {Operator::CompoundAssignmentDivide, 9.0f},
        {Operator::CompoundAssignmentModulus, 9.0f},
        {Operator::Increment, 0.5f},
        {Operator::Decrement, 0.5f},
        {Operator::Variadic, 11.0f}
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
        LeftBrace, // {
        RightBrace, // }
        Semicolon, // ;
        Colon, // :
        At, // @
        Comma, // ,
        LeftParen, // (
        RightParen, // )
        LeftBracket, // [
        RightBracket // ]
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