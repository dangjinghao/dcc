# DCC

- The parser would not generate a AST which 100% corresponding to the source code, to reduce the complexity of codegen/build process.
  - We call this AST `reduced AST`.
  - `int i,j,k = 10;` -> `int i;int j; int k = 10;`
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
- [ ] expr
  - [x] unary/binop/logic-cmp variable test
  - [x] ternary
  - [x] unary expression array index
  - [x] function call
  - [x] struct get member
  - [ ] array
    - [ ] deref
    - [ ] parameter pass
    - [ ] multi-dim test
- [x] basic switch implementation
  - [ ] binary search optimization
- [ ] QA scaffold
- [ ] string reference table in builder
- [x] reorganize builder API again
- [ ] {} initializer
  - [ ] struct/union/array init
- [ ] sizeof constant expression
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

`parser/grammar.h` ： `g`
