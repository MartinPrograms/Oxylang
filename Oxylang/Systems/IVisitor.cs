using Oxylang.Systems.Parsing;
using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems;

// A visitor is a class that can visit nodes in the AST!
// This is used for type checking, and other static analysis. Use the ITransformer interface for transforming the AST from one form to another (e.g. desugaring, code generation, etc.)
public interface IVisitor
{
    void Visit(Root node);
    void Visit(AlignOfExpression node); // alignof<T>
    void Visit(AssignmentExpression node); // a = b
    void Visit(BinaryExpression node); // a + b, a - b, etc.
    void Visit(BlockStatement node); // { ... }
    void Visit(CastExpression node); // cast<T>(a)
    void Visit(FunctionCallExpression node); // a(b, c, ...)
    void Visit(FunctionDefinition node); // fn foo(a: i32) -> i32 { ... }
    void Visit(ImportStatement node); // import "path" as alias;
    void Visit(LiteralExpression node); // 42, true, false, etc.
    void Visit(PostfixExpression node); // a++, a--.
    void Visit(ReturnStatement node); // return; return a;
    void Visit(SizeOfExpression node); // sizeof<T>
    void Visit(StringExpression node); // "hello", 'a', etc.
    void Visit(StructDefinition node); // struct Foo { a: i32, b: f64 }
    void Visit(StructInitializerExpression node); // Foo { a: 42, b: 3.14 }
    void Visit(VariableDeclaration node); // let x: i32 = 42; var y: f64 = 3.14;
    void Visit(VariableExpression node); // x, y, etc.
    void Visit(AddressOfExpression node); // addr(x)
    void Visit(DereferenceExpression node); // deref(x)
    void Visit(MemberAccessExpression node); // a.b or a->b
    void Visit(UnaryExpression node); // -a, !a, etc.
    
    // Conditionals
    void Visit(IfStatement node); // if (condition) { ... } else { ... }
    void Visit(WhileStatement node); // while (condition) { ... }
    void Visit(ForStatement node); // for (initializer; condition; increment) { ... }
    
    // Loops
    void Visit(BreakStatement node); // break;
    void Visit(ContinueStatement node); // continue;
}

public interface IAstTransformer
{
    Node Visit(Root node);
    Node Visit(AlignOfExpression node); // alignof<T>
    Node Visit(AssignmentExpression node); // a = b
    Node Visit(BinaryExpression node); // a + b, a - b, etc.
    Node Visit(BlockStatement node); // { ... }
    Node Visit(CastExpression node); // cast<T>(a)
    Node Visit(FunctionCallExpression node); // a(b, c, ...)
    Node Visit(FunctionDefinition node); // fn foo(a: i32) -> i32 { ... }
    Node Visit(ImportStatement node); // import "path" as alias;
    Node Visit(LiteralExpression node); // 42, true, false, etc.
    Node Visit(PostfixExpression node); // a++, a--.
    Node Visit(ReturnStatement node); // return; return a;
    Node Visit(SizeOfExpression node); // sizeof<T>
    Node Visit(StringExpression node); // "hello", 'a', etc.
    Node Visit(StructDefinition node); // struct Foo { a: i32, b: f64 }
    Node Visit(StructInitializerExpression node); // Foo { a: 42, b: 3.14 }
    Node Visit(VariableDeclaration node); // let x: i32 = 42; var y: f64 = 3.14;
    Node Visit(VariableExpression node); // x, y, etc.
    Node Visit(AddressOfExpression node); // addr(x)
    Node Visit(DereferenceExpression node); // deref(x)
    Node Visit(MemberAccessExpression node); // a.b or a->b
    Node Visit(UnaryExpression node); // -a, !a, etc.
    
    // Conditionals
    Node Visit(IfStatement node); // if (condition) { ... } else { ... }
    Node Visit(WhileStatement node); // while (condition) { ... }
    Node Visit(ForStatement node); // for (initializer; condition; increment) { ... }
    
    // Loops
    Node Visit(BreakStatement node); // break;
    Node Visit(ContinueStatement node); // continue;
}