# DCC

- No real stream in tokenizer, just array.
- No more memory management(free) in `dcc_core`.
- No Preprocessing.
- Better type promotion(there are some bugs in type.c from chibicc).
- AST dump and visual representation(from my previous dcc project)
    Use `./build/astrepr <src> > astrepr/data.json` to generate the AST tree, then you can preview it on browser.
- More dedicated AST node. E.g. `op=`.
- Automatic testsuit.
- Dependent tools:
    - `cpp` the C Preprocessor
    - LLVM backend
- Small struct passed-as-value is simply implemented by LLVM struct, so maybe sometimes it's not correct.
- Bootstrap support (stage3)

## tested program

- [dangjinghao/dcc](https://github.com/dangjinghao/dcc)
- [antirez/kilo](https://github.com/antirez/kilo)

## Reference

- [chibicc](https://github.com/rui314/chibicc)