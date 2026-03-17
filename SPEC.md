# Oxylang design spec
A C inspired language with less ambiguity!



## Structure:

- Statements end with `;`
- Blocks use `{...}`
- Identifiers `[A-Za-z_][A-Za-z0-9_]*`
- Line comments `//...`
- Block comments `/* ... */`
- All keywords are reserved
- Attributes are declared using the @ symbol before a declaration.

## Keywords:

- `fn` - function declaration
- `let` - variable declaration
- `return`
- `if`
- `else`
- `while`
- `for` - C like `for (expression; expression; expression) { ... }`
- `struct`
- `import` - importing a module
- `as` - importing a module
- `ptr`
- `array`
- `true`
- `false`
- `null`
- `sizeof`
- `alignof`
- `typeof`
- `addr`
- `deref`
- `cast`
- `allocate`
- `free`
- `break`
- `continue`

*Note*: sizeof, alignof and typeof are calculated during compilation. They do not happen during runtime.



## Types:

### value types:

#### integers:
- `i8`
- `u8`
- `i16`
- `u16`
- `i32`
- `u32`
- `i64`
- `u64`
- `int` -> `i32` regardless of architecture
- `long` -> `i64` ^^^

#### floating point numbers:
- `f32`
- `f64`
- `float` -> `f32` regardless of architecture
- `double` -> `f64` ^^^

#### general:
`void` - no type. Only applicable as a function return type, or ptr<void>. Other cases will return an err

#### pointers:
`ptr<T>`, declares a pointer to type `T`.

#### fixed arrays:
`array<T, N>`, where N is a compile time integer.
fixed arrays have bounds checking, which prevents access of indices not available.

#### function types:
`fn (T1, T2) -> RT`

#### casting:
you can cast using `cast<T>(expression)`


## Declarations:

### variables:
- `let x: int = 0;`
- `let y = 0; // i32`
- `let z = 0.0; // f64`
- `let w = 0.0f; // f32`

### structs:
- `struct Name<T, T1> { element: T; element2: T1; }`

### functions:
- `fn add(a: int, b: int) -> int { return a + b; }`
- `fn return_t<T>(x: T) -> T { return x; }`



## Generics & compile time:

generic parameters use <...> on functions, and types after their name.
calculated at compile time.

## compile time expressions:
- `sizeof<T>` returns the size of T in bytes.
- `alignof<T>` returns the alignment of T in bytes.
- `typeof(expression)` returns the type of the expression.

there are no constraints on generics.



## Pointers & arrays:

### types:
- `ptr<T>` is a pointer to `T`.
- `array<T,N>` is a fixed array of `T * N` byte size. Internally turns into the following struct `{ base: ptr<T>; len: i64 = N }`

### operations:
- `addr(x)` gives a pointer to `x`, where `T` of `ptr<T>` is `x: T`.
- `deref(p)` dereferencers the pointer, and returns an instance of `T`.
- `p + x` / `p - x` perform element scaled arithmetic for `ptr<T>`. For `ptr<void>` all arithmetic is in bytes.
- `a[i]` gets compiled into `deref(a + i)` when `a: ptr<T>`, for `array<T, N>` it adds a basic bounds check before deref.

### examples:
```js
let a: ptr<int> = allocate<int>(10);
let b: array<int, 5>;

for (let i: int = 0; i < 10; i++){
    a[i] = i;
    b[i] = i; // will error on i > 4. But not segfault.
}
```



## Function pointers:

- function types are written like `fn(T, T1) -> RT`.
- pointers to functions: `ptr<fn(int) -> int>`.
- arrays to function pointers: `array<ptr<fn(int) -> void>, 10>`

calling a function pointer can be done using `pf(x)` where `pf: ptr<fn(T) -> R>` this internally gets handled as `(deref(pf))(x)`.

### example:
```js
fn square (x: int) -> int { return x * x; }
let pf: ptr<fn(int) -> int> = addr(square);
let y = pf(5);
```



## Attributes:
- placement of attributes must be before a declaration (`fn`, `struct`, `let`). They can be on a new line, but is not required.

### syntax:
- `@export` - Tells the compiler to export this symbol. Must be either a global variable, or a function. Can NOT be a struct, nor can it have generics.
- `@public` - Allows this symbol to be accessed by import statements.
- `@extern` - Declare variable as externally linked. Functions with the @extern attribute must NOT have a body. Variables must also not have a value.
- `@entry` - Declare a function as the entrypoint. (this also adds the @export implicitly)
- `@callconv("callconv")` - Adjust the calling convention "cdecl", "stdcall", "sysv" are valid options.
- `@symbol("external_name")` - If in combination with @export, renames the function to "external_name". If in combination with @extern, it's the symbol to import.

### examples:
```js
@public @export @symbol("multiply") @callconv("cdecl") fn multiply(a: int, b: int) -> int{
    return a * b;
}


@public
struct Window{
    title: ptr<u8>; // null terminated string.
}
```



## Extension methods:

an extension method is a function whose first parameter is named `this` and whose type is `T` becomes a method of `T`, this mangles the name to `TfunctionName`.
`a.printType();` turns into `TprintType(a)` where `T` is typeof `a`. If `TprintType` is from an imported module, it will be the equivalent of `moduleName.TprintType(a);`.
extension methods do not listen to imported module names. if type `T` is imported from module `A` and module `B` extends it, it can still be accessed through `T.bExtension`.



## Modules and imports:
- each source file is a module.
- a module can be referenced either by file path or by a build-system identifier.
- the source file's location is used as the root for relative paths.
- to register something in the build system, add it to the list of modules as `{"id", "file_path.oxy"}`. There are a few default modules, which rely on oxy installation directories.

### syntax
```js
import "path or build system id" as name
```
- imports the oxy file from the identifier, and adds all @public marked functions/variables/structs under the name.
- the identifier (`name`) is chosen freely by the programmer.
- access is always qualified through that namespace: `name.Symbol`
- there is no implicit import into the local scope.

### example:
math.oxy:
```js
@public
fn add (x: int, y: int) -> int { return x + y; }
```

main.oxy:
```js
import "math.oxy" as m;

@entry
fn start() -> u8{
    let result: u8 = cast<u8>(m.add(10,3));
    return result;
} // returns 13
```



## Control flow:
similar to C.

- `if (cond) {...} else (if) {...}`
- `while (cond) {...}`
- `for (init; cond; step) {...}`
- `return expr;` or `return` for `void` functions.



## Expressions and operators:

### precendence:
all bitwise operators (`&`, `|`, `^`, `<<`, `>>`) have the same precendence. you **must** solve this ambiguity using explicit parenthesis.
for example, `a & b | c` will be a compile-time error unless written as `(a & b) | c` or `a & (b | c)`.
logical operators (`&&`, `||`) have higher precendence than bitwise, but lower than comparison operators.
assignment operators have the lowest precendence.

#### table:
(lower number = higher precedence)

| operator                         | description                                 | associativity | precedence |
|----------------------------------|---------------------------------------------|---------------|------------|
| `++`, `--`                       | post-increment/decrement                    | left          | 0          |
| `()`                             | nested expressions, function calls          | left          | 1          |
| `[]`, `.`, `->`                  | array access, member access, pointer access | left          | 2          |
| `addr`, `deref`, `cast`          | pointer operations                          | right         | 3          |
| `*`, `/`, `%`                    | multiplication, division, modulus           | left          | 4          |
| `+`, `-`                         | addition, subtraction                       | left          | 5          |
| `<`, `<=`, `>`, `>=`, `==`, `!=` | equality operators                          | left          | 6          |
| `&&`, `\|\|`                     | logical AND, OR                             | left          | 7          |
| `<<`, `>>`, `&`, `^`, `\|`       | bitwise operators                           | left          | 8          |
| `=`, `+=`, `-=`, etc.            | assignment operators                        | right         | 9          |



### Memory:

#### built-in:
- `allocate<T>(count: int) -> ptr<T>` - internal call to libc's malloc. If count is specified malloc is called with `T*count`.
- `free<T>(p: ptr<T>) -> void` - internal call to libc's free.
- `array<T, N>` allocates a pointer of size T * N, and stores the length. Passing it does NOT change the pointer. This is stored on the heap.
- `addr(x)` - returns a pointer to `x`. You can not take the address of a literal.
- `deref(p)` - dereferences the pointer `p`. If `p` is null, this will cause a segfault.
- `cast<T>(x)` - casts `x` to type `T`. No runtime checks are performed. You must ensure that the cast is valid.


### Errors:
like in C, no errors will be default. I might consider making try/catch? though i dont know exactly. Maybe a Result<T, E>?



### Name resolution and overloading:

- function overloading is possible.
- no implicit numeric promotions, you must promote/demote by using `cast<target>(source)`.
- all source files have their own symbol table, and namespace. the dot syntax accesses that table.

### Literal expressoins:
- integer literals: `123`, `0x1A`, `0b1101` (all are `i32` by default, but can be suffixed with u8-u64, i8-i64 `123u8`, `0x1Ai16`, `0b1101u64`)
- floating point literals: `123.0`, `0.1f` (all are `f64` by default, unless suffixed with f)
- boolean literals: `true`, `false` (handled as `u8` internally)
- string literals: `"hello"` (of type `ptr<u8>`, null terminated)
- null pointer literal: `null` (of type `ptr<void>` with a pointer to 0x0)


## Example:
```js
import "std/io" as io;

// We are importing raylib here, which is a C library.
@extern @symbol("InitWindow") fn InitWindow(width: i32, height: i32, title: ptr<u8>) -> void;
@extern @symbol("SetTargetFPS") fn SetTargetFPS(fps: i32) -> void;
@extern @symbol("WindowShouldClose") fn WindowShouldClose() -> u8; // bool (u8)
@extern @symbol("CloseWindow") fn CloseWindow() -> void;

@entry
fn main() -> u8 {
    let width: i32 = 800;
    let height: i32 = 450;
    let title: ptr<u8> = "Hello World"; // The null terminator is automatically added.

    InitWindow(width, height, title);
    SetTargetFPS(60);

    io.println("Window initialized!");

    while (WindowShouldClose() == 0) {
        // Game loop would go here
        io.println("Running...");
    }

    io.println("Closing window...");

    CloseWindow();
    return 0;
}
```
