# DCC

a simple C89 compiler.

## summary

- Compiler only, without preprocessor and assembler.
- Emits to llvm ir.
- LL(1) type parser with **recovery lexer** feature to support those syntax that LL(1) supports poorly.
  - the recovery lexer support lexer snapshot, we can resume the lexer snapshot when parsing failed, then select the other way to parse.
  - e.g. parse `LABEL: result = 1+1;` and `result = 1+1;` they have the same firstset, so we try to parse as `labeled-statement`, for `result = 1+1;` the next token is `=`, not `:` as labeled-statement described. So we can snapshot the lexer(wrapped in parser snapshot function) before process this situation, if parse failed, just resume snapshot and select the `expression-statement` parsing way.
- 2 pass compiler(AST generation + llvm ir generation)
  - to solve the multiple same name declaration problem.
- Achieved `96.7%` test pass rate on `60` critical syntax cases (adapted from TCC),
- Engineered trade-offs:
  - Simplified static initialization
  - Unified `long`/`long long` types with static assertions for cross-platform compatibility.
  - Omitted bitfields and struct/union value passing to function after evaluating alignment complexity.
- Tools
  - AST visiualization web at `analysis/AST.html`
  - a simple CLI tool `dcc.sh` for compile source file.
    - e.g. `dcc.sh src.c -o a.out`

*In fact, lots of caused problems derived from my poor architecture, LOL.*

## Test Coverage

I picked up `60` core syntax test cases derived from `TinyCC` project, and move some static initialization code into function body because of dcc's poor ability of static initialization.

- 58/60 tests passed (`96.7%` success rate)
- 2 edge cases about `sizeof static inferation(55)` and `nameless function parameter(81)`

## Features

- Almost C89 features
- declaration at anywhere is allowed. (C99)
- declaration at `for` statement `init` scope. (C99)
- In fact we doesn't support `long long int` type, it is same as `long int` same as `long`, so we set a static_assert to make sure that the sizeof long long equals to sizeof long.
- statement in expresssion (GNU C)

### Unsupport Features

- bitfield
  - I'm not good at align or something, so looks like I cannot solve this right now.
- struct/union pass to or return from function by value
  - same as before.
- static variable initialization with complex value
  - same as before, but I implement a simple static expression initialization.
- incomplete type
  - I just implement the incomplete enum type feature.

## Hint

- **maybe we can more dependent on LLVM so that we can reduce the complexity of designation.**

- The parser would not generate a AST which 100% corresponding to the source code, to reduce the complexity of codegen/build process.
  - We call this AST `reduced AST`.
  - `int i,j,k = 10;` -> `int i;int j; int k = 10;`
  - `struct/union/enum STU {<xxx>} stu1;` -> `struct/union/enum STU {<xxx>}; struct/union/enum STU stu1;`
- The `extern` declaration in block scope would be ignored in build stage
- We don't have to move the static declaration to global scope, llvm supports add extern symbol to global scope  
- We **WOULD NOT FREE** the memory in builder stage.
- We have to use `parser.symtab` because we cannot process this situation:

```c
extern int V;
void F(){V = 1;}
int V;
```

  the parser processing flow is top to down, so when processing the body of function `F`, V refers to `extern int V`, but in fact after the full code is parsed, `V` symbol means `int V`. We have to re-locate the symbol that a ast node **refers** by symtab in build stage.That's why we need symtab which save the extern or normal declarations.

- Now we just ignore the full functional build implementation of global variable initializer and take more care of runtime build (e.g. code in function) until we complete it.
- We should dependence less LLVM library features expect for instruction generation.
- **DO NOT USE** *i1* directly, cast it to *i8*.
- Postpone to declare struct because some struct declaration is not variable declaration, e.g.:

```c
struct s{int a;char b;};
```

- We have to move builder position to entry(first) block for `alloca` a variable rather create this instruction in other basic block. Because, in LLVM every time one `alloca` instruction was executed, the stack pointer would be pull down.

- We will store the alloca variable in entry block in every function.

- Return struct by value or pass struct by value would lead to wrong ir code situation, so it is banned.
  - I don't think I can resolve it simply.
  - Clang adrress this issue by handling the struct/union encode/decode logic in its frontend.

## TODO LIST

- [x] use slist instead of dynarray for performance
- [x] refactor repr to generate a visual AST result
- [x] struct parser
- [x] va, void test
- [x] expand typedef
- [x] fix `typedef int I;{I I;}`
- [x] struct
- [x] enum
- [x] parser_check_constant_int_expr
- [x] parser_eval_const_int_expr
- [x] re-orginize all api
- [x] build typecast
- [x] build jump statement: goto and return
- [x] return statement
- [x] ptr test
- [x] binop self assign
- [x] logic operation
- [x] iteration statement
- [x] break/continue in iteration statement
- [x] if-else statement
- [x] iteration and switch mixture test
- [x] copy/assign struct to other struct
- [x] union support
- [x] expr
  - [x] unary/binop/logic-cmp variable test
  - [x] ternary
  - [x] unary expression array index
  - [x] function call
  - [x] struct get member
  - [x] array
    - [x] deref
    - [x] multi-dim test
    - [x] ptr assign
    - [x] parameter pass
- [x] basic switch implementation
  - [x] binary search optimization
- [x] string reference table in builder
  - [x] assign string to char ptr
  - [x] char array initialize with string
    - [ ] length inference
- [x] string array, ptr integration in type cast
- [x] struct array test
- [x] ptr to struct/union(any complex type) - ptr
- [x] {} initializer
  - [x] struct/array init
- [x] 72 long long int support
- [ ] ~~remove uid, I don't think it is necessary.(we have to use it to distinguish the static variable)~~
- [x] sizeof constant expression
- [x] 89 gnu `({})` ext
- [ ] type unfold redefine bug
- [ ] QA scaffold
- [ ] refactor
  - [ ] using hierarchical design
  - [ ] comment functions
  - [ ] remove uesless functions
- [ ] incomplete type (void/struct/union/enum dlclaration only/array declaration without array size)
- [ ] node meta data
- [ ] union
- [ ] attribute
- [ ] c99

## expression parser

- Pratt parser expression

## API Organization method

```makefile
$(MODULE_NAME)_$(CLASSIFY)_$('is' if return bool or 'get'/'set' for getter/setter or 'new' for create new object or any other verb)_$(TARGET if it exists in the object)
```

the file which has the same name as its parent directory is allowed to use only one time name.
if there is a short name of $(MODULE_NAME)_$(CLASSIFY), e.g. the short name of `parser/grammar.h` is `g`, use this method

```makefile
$(SHORT_NAME)_$(same as above)
```

### Short Name Table

`parser/grammar.h` :  `g`

## BUGS

- assign array to array would not leads to panic
  - fix by using type checker?
- assign typedef advanced type (struct/union) may leads to redefined struct declaration
