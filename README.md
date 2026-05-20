# DCC

- No real stream in tokenizer, just array.
- No more memory management(free) in `dcc_core`.
- No Preprocessing.
- Better type promotion(there are some bugs in type.c from chibicc).
- AST dump and visual representation(from my previous dcc project)
    Use `./build/astrepr <src> > astrepr/data.json` to generate the AST tree, then you can preview it on browser.
- more dedicated AST node. E.g. `op=`.
- automatic testsuit.
- dependent tools:
    - `cpp` the C Preprocessor
    - LLVM backend
- small struct passed-as-value is simply implemented by LLVM struct, so maybe sometimes it's not correct.

## TODO

- better eval2
- mark living function before codegen
  - defination
  - ref
- alignof
- alignas

## Reference

- [chibicc](https://github.com/rui314/chibicc)