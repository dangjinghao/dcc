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
- Bootstrap support (stage3)

## tested program

- [dangjinghao/dcc](https://github.com/dangjinghao/dcc/tree/v2)
- [antirez/kilo](https://github.com/antirez/kilo/tree/323d93b29bd89a2cb446de90c4ed4fea1764176e)
- [c-testsuite/c-testsuite](https://github.com/c-testsuite/c-testsuite/tree/5c7275656d751de0e68b2d340a95b5681858ed07)

    Failed tests:

    - 00170.c: anonymous enumeration
    - 00209.c: anonymous enumeration
    - 00210.c: `stdcall` attribute
    - 00214.c: `__builtin_expect`
    - 00216.c: unnamed struct/union members

- [rui314/chibicc](https://github.com/rui314/chibicc/tree/90d1f7f199cc55b13c7fdb5839d1409806633fdb)
- [rui314/libpng](https://github.com/rui314/libpng/tree/dbe3e0c43e549a1602286144d94b0666549b18e6)
- [sqlite/sqlite](https://github.com/sqlite/sqlite/tree/86f477edaa17767b39c7bae5b67cac8580f7a8c1)
- [git/git](https://github.com/git/git/tree/54e85e7af1ac9e9a92888060d6811ae767fea1bc)
    
    There are 3 failed objectsize:disk tests exist(without `TODO` mark) and I think they are related to my `zlib-ng` library.

- [python/cpython](https://github.com/python/cpython/tree/c75330605d4795850ec74fdc4d69aa5d92f76c00)

    Tested without network-related testcases. Then `test_peg_generator` and `test_zlib` failed.

- [lua/lua](https://github.com/lua/lua/tree/v5.5.0)


## Reference

- [chibicc](https://github.com/rui314/chibicc)