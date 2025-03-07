# DCC

The parser would not generate a AST which corresponding to the source code, to reduce the complexity of codegen process.

We call this AST `reduced AST`.

the extern declaration in block scope would be ignored in codegen stage

We can delay the static declaration to codegen stage, llvm supports it

we have to use `parser.symtab` because we cannot process this situation:

```c
extern int V;
void F(){V = 1;}
int V;
```

the parser processing flow is top to down, so when processing the body of function `F`, V refers to `extern int V`, but in fact after the full code is parsed, `V` symbol means `int V`. We have to re-locate the symbol that a ast node **refers** by symtab in codegen stage.That's why we need symtab which save the extern or normal declarations.

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
- [ ] replace int constant astn type with size_t in array size declaration
- [ ] expr
  - [ ] struct get member
- [ ] {} initializer
- [ ] sizeof constant expression
- [ ] multi-dim array
- [ ] union
- [ ] attribute
- use llvm c api instead of my llvm scaffold in codegen stage
  - expr
  - ternary expr support
  - return
  - funcion calling
  - struct

## expression parser

- Pratt parser expression

## scope and symbol table
