# DCC

The parser would not generate a AST which corresponding to the source code, to reduce the complexity of codegen process.

We call this AST `reduced AST`.


## TODO LIST

- [x] use slist instead of dynarray for performance
- [x] refactor repr to generate a visual AST result
- [ ] struct parser
- [ ] parser_check_constant_expr
- use llvm c api instead of my llvm scaffold in codegen stage
  - expr
  - ternary expr support
  - return
  - funcion calling
  - struct

## expression parser

- Pratt parser expression

## scope and symbol table
