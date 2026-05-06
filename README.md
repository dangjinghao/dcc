# DCC

- No stream in tokenizer
- No memory management(free) in core
- No Preprocessing
- ASCII code only (currently)
- Better type promotion(there are some bugs in type.c from chibicc)
- AST dump and visual representation(from my previous dcc project)
    Use `./build/astrepr [src-path] > astrepr/data.json` to generate the AST tree, then you can preview it on browser.
- more dedicated AST node. E.g. op= and self inc/dec
- automatic testsuit
- dependent tools: 
    - cpp and shell to preprocess
    - LLVM backend

## Reference

- [chibicc](https://github.com/rui314/chibicc)