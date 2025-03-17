# DCC

The parser would not generate a AST which corresponding to the source code, to reduce the complexity of codegen process.

We call this AST `reduced AST`.

the extern declaration in block scope would be ignored in codegen stage

We can delay the static declaration to codegen stage, llvm supports it

We would **not free** the memory in builder stage.

we have to use `parser.symtab` because we cannot process this situation:

```c
extern int V;
void F(){V = 1;}
int V;
```

the parser processing flow is top to down, so when processing the body of function `F`, V refers to `extern int V`, but in fact after the full code is parsed, `V` symbol means `int V`. We have to re-locate the symbol that a ast node **refers** by symtab in codegen stage.That's why we need symtab which save the extern or normal declarations.

Now we just ignore the full functional codegen implementation of global variable initializer. Take more care of runtime codegen (e.g. code in function) until we complete it.

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
- [ ] logic operation
- [ ] if-else statement
- [ ] iteration statement
- [ ] reorganize builder API again
- [ ] expr
  - [x] unary/binop/logic-cmp variable test
  - [x] ternary
  - [x] unary expression array index
  - [x] function call
  - [ ] struct get member
  - [ ] array
- [ ] update switch case
- [ ] string reference table in builder
- [ ] {} initializer
- [ ] sizeof constant expression
- [ ] multi-dim array
- [ ] node meta data
- [ ] union
- [ ] attribute
- [ ] c99

## expression parser

- Pratt parser expression

## API Organization method

```makefile
$(MODULE_NAME)_$(CLASSIFY)_$('is' if return bool or 'get'/'set' for getter/setter or 'new' for create new object or any other verb)_$(TARGET if necessary)
```

the file which has the same name as its parent directory is allowed to use only one time name.
if there is a short name of $(MODULE_NAME)_$(CLASSIFY), e.g. the short name of `parser/grammar.h` is `g`, use this method

```makefile
$(SHORT_NAME)_$(same as above)
```

### Short Name Table

`parser/grammar.h` ： `g`
