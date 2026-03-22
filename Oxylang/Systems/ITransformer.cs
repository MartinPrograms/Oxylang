using Oxylang.Systems.Parsing.Nodes;

namespace Oxylang.Systems;

// A transformer is a visitor that transforms the AST! So it can be used for optimizations, extra features, code generation, etc. It can also be used to transform the AST into a different representation, like an intermediate representation (IR) for code generation.
public interface ITransformer<T> : IVisitor
{
    T Transform(Root node);
}

