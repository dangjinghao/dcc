# DCC

- No stream in tokenizer.
- No memory management(free) in core.
- No Preprocessing.
- Better type promotion(there are some bugs in type.c from chibicc).
- AST dump and visual representation(from my previous dcc project)
    Use `./build/astrepr [src-path] > astrepr/data.json` to generate the AST tree, then you can preview it on browser.
- more dedicated AST node. E.g. `op=`.
- automatic testsuit.
- dependent tools: 
    - `cpp` and shell to preprocess
    - LLVM backend
- small struct passed-as-value is simply implemented by LLVM struct, so maybe sometimes it's not correct.

## TODO

- va function
- alignof
- alignas

## Reference

- [chibicc](https://github.com/rui314/chibicc)