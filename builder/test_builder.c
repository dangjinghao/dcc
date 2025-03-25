#include "ast.h"
#include "builder.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include "slist/slist.h"
#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>

[[gnu::constructor]] void init() {
  log_color_enable(true);
  log_set_level(LOG_LEVEL_TRACE);
  log_trace("test_builder init");
}

int process_trans_unit(struct lexer *lexer) {
  struct parser parser;
  parser_new_from_lexer(&parser, lexer);
  astn u = parse_translation_unit(&parser);
  struct builder b;
  builder_new(&b, "test", &parser.symtab);
  slist symtab = parser_gen_symtab(&parser);
  build_trans_unit(&b, symtab);

  char *ir = LLVMPrintModuleToString(b.module);
  puts(ir);
  LLVMDisposeMessage(ir);

  char *msg = NULL;
  if (LLVMVerifyModule(b.module, LLVMReturnStatusAction, &msg)) {
    log_error("ir code verification failed: %s", msg);
  }
  LLVMDisposeMessage(msg);
  builder_destroy(&b);
  parser_destory(&parser);
  ast_free(u);
  return 0;
}

void tbc_entry(char *code) {
  struct lexer lexer;
  lexer_new_from_string(&lexer, code);
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
}

void tbc_extern_multi_type() { tbc_entry("int v;double dv;float fv;char cv;"); }

void tbc_storages() {
  tbc_entry("extern int xv;static int v;float x2;int xv;");
}

void tbc_func() { tbc_entry("void f();void f2(int a,char b);void f3(void);"); }

void tbc_vafunc() { tbc_entry("void f(int a,char b,short c,...);"); }

void tbc_ptr() { tbc_entry("void* ptr;"); }

void tbc_struct() {
  tbc_entry("struct s {int a;char b; struct{short s; int d;}s;} s; struct s "
            "refs; struct{char c; "
            "char*s; struct{short s;}s;} abss;");
}

void tbc_struct_decl_only() {
  tbc_entry("struct s{int a;char b;}; struct s s1; int F(){struct s s2;}");
}

void tbc_multi_subscope_extern() {
  tbc_entry("int F1(){extern Fe();{extern Fe();}} int F2(){extern Fe(); "
            "{extern Fe();}}");
}

void tbc_func_def() { tbc_entry("char* f();char* f(){int a = 1;}"); }

void tbc_add() { tbc_entry("void F(){int a = 6.9f + 2;}"); }

void tbc_arithmetic() { tbc_entry("void F(){int v = 2/1u;}"); }

void tbc_not() { tbc_entry("void F(){int v = !!0;}"); }

void tbc_load_var() { tbc_entry("void F(){int v = 1;int p = 3 * v + 2/1.5;}"); }

void tbc_rem() { tbc_entry("void F(){int v = 1;int p = v % 2;}"); }

void tbc_urem() { tbc_entry("void F(){char v = 1;float p = 3ul % v;}"); }

void tbc_fp_rem() { tbc_entry("void F(){float v = 1.0;float p = v % 2.0;}"); }

void tbc_char_pos_promote() { tbc_entry("void F(){char v = 1;long p = +v;}"); }

void tbc_extern_in_func() { tbc_entry("extern int v;void F2(){v = 1;}"); }

void tbc_assign() { tbc_entry("void F(){int v = 1;int v2 = v = 2;v = ~v2;}"); }

void tbc_multi_extern() {
  tbc_entry("extern int A;int A; extern int A; int F(){int B = A; A = 2;}");
}

void tbc_ptr_assign() {
  tbc_entry("void F(){int v = 1;int* p = &v;int** p2 = &p;int nv = **p2;}");
}

void tbc_self_inc() {
  tbc_entry("void F(){int v = 1;v++;int v2 = v--;int v0 = --v;}");
}

void tbc_deref_ptr_assign() {
  tbc_entry("void F(){int v = 1;int* p = &v;*(p + 1) = 2;}");
}

void tbc_combine_assign() {
  tbc_entry("void F(){int v = 1;char v2 = 3; v2 = v = v2+v;}");
}

void tbc_ptr_cmp() {
  tbc_entry("void F(){int* p,* p2; p >= p2; p <= p2; p == p2; p != p2; p > p2; "
            "p < p2;}");
}

void tbc_ptr_int() {
  tbc_entry("void F(){char* p,*p2;char v = 1;v + p; p - v; long v2 = 2;p2 = p "
            "+ v2; p2 - p;}");
}

void tbc_signed() {
  tbc_entry("void F(){int v = 1;unsigned int v2 = 2;int v3 = v + v2; v2 /= "
            "4.;v2 /= 2; "
            "v2 %=3;}");
}

void tbc_func_arg() { tbc_entry("void F(int a,char b,short* c){*c = a + b;}"); }

void tbc_func_void() { tbc_entry("void F(){}"); }

void tbc_func_va() { tbc_entry("void F(int a,...){a = 1;}"); }

void tbc_func_typecast() {
  tbc_entry("void F(int a,char b,short c){c  = (char)a;}");
}

void tbc_ternary_type_cast() {
  tbc_entry("void F(int v,char v2,long v3){   v3 = v ? v>v2 ? v3 - 2? v3 : v : "
            "v2 >= 2 : 2;}");
}

void tbc_return() { tbc_entry("long F(char c, short s){return c + s;}"); }

void tbc_void_return() { tbc_entry("void F(){return;}"); }

void tbc_void_wrong_return() { tbc_entry("void F(){return 1;}"); }

void tbc_cast_return() { tbc_entry("char F(unsigned long x){return 1 + x;}"); }

void tbc_goto_label() {
  tbc_entry("int F(int v){L1: v = 1;v = 2;L2: v = 3; L3: v = 4; return v; L4: "
            "v = v + 1;}");
}

void tbc_goto() {
  tbc_entry("int F(int v){goto L2;v = v * 2;v = 1;L1: v = 2;return v;L2: v "
            "= 3;goto L3;v = 4;L3: v = 4;v = v + 1;goto L1;}");
}

void tbc_goto_wrong() {
  tbc_entry("int F(int v){v = v * 2;goto L2; L3: return v;}");
}

void tbc_multi_func() {
  tbc_entry(
      "int F1(int v){return v;}int F2(int v){return v + 2 > v ? v : 2;} int "
      "F3(int v){return v * 3;}");
}

void tbc_arr_deref_assign() {
  tbc_entry(
      "int F(int *arr,int idx) {arr[idx] = idx;*(arr + 1 + idx) = idx + 1;}");
}
void tbc_arr_deref() {
  tbc_entry(
      "int F(int *arr,int idx) {return arr[idx + 1] + *(arr + idx + 2);}");
}

void tbc_arr_sc() { tbc_entry("int F(int *arr,int idx) {return idx[arr];}"); }

void tbc_ref_func() { tbc_entry("void F(int a, void*p){ p = F;}"); }

void tbc_func_call() {
  tbc_entry("int add(int a, short b, double c){ return a + b + c;}"
            "int F(){int (*call)(int a, short b, double c) = add;char a = 1,b "
            "= 2; unsigned int c = 3; return call(a,b,c) * add(1,2,3);}");
}

void tbc_func_uncallable() { tbc_entry("int F(){int a = 1; return a();}"); }

void tbc_ptr_unary_wrong() { tbc_entry("void F(){char* p; +p; -p; ~p;}"); }
void tbc_ptr_unary_right() {
  tbc_entry("void F(){char* p; char*p2 = p++;char*p3 = p--;char*p4 = "
            "++p;char*p5 = --p;void*pp = &p;int c =*p; c = !p;}");
}

void tbc_ptr_self_inc() {
  tbc_entry("void F(){int* p = 0; char*x = p++;char c =  *--p;}");
}

void tbc_ptr_null() {
  tbc_entry("int F(){char*p = 0x10; return (long)p ? (long)p : (char) p; }");
}
void tbc_ptr_logic() {
  // test assign
  tbc_entry("void F(){short* p; p > 1; p < p + 1; p == p; p <= p + 1;}");
}

void tbc_ptr_assign2() {
  tbc_entry("void F(){int**p;p[1] = 1; *(p+2) = 2; p = 3;}");
}

void tbc_self_assign() {
  tbc_entry("void F(){int v = 1;v = v + 2;v += 3;v -= 4;v *= 5;v /= 6;v %= "
            "7;v &= 8;v |= 9;v ^= 10;v <<= 11;v >>= 12;}");
}

void tbc_self_assign_side_effect() {
  tbc_entry("int idx(); \
            void F(){\
            int *p;\
            p[idx()] += 42;\
            p[idx()] -= 42;\
            p[idx()] *= 42;\
            p[idx()] /= 42;\
            p[idx()] %= 42;\
            p[idx()] &= 42;\
            p[idx()] |= 42;\
            p[idx()] ^= 42;\
            p[idx()] <<= 42;\
            p[idx()] >>= 42;\
            }");
}

void tbc_self_assign_ptr_wrong() {
  tbc_entry("void F(){int *p; p += 1; p -= 1; p *= 1; p /= 1; p %= 1; p &= 1; "
            "p |= 1; p ^= 1; p <<= 1; p >>= 1;}");
}

void tbc_ptr_self_assign() {
  tbc_entry("void F(){int* v = 0;v = v + 2;v += 3;v -= 4;}");
}

void tbc_logic() {
  tbc_entry("void F(){int v = 1,v2 = 2,v3 = 3,v4=4,v5=5;int r = ((v && v2) || "
            "v3) && (v4 || v5);}");
}

void tbc_void_func_call() { tbc_entry("void F(){{{void f();f();}}}"); }

void tbc_sum5050() {
  tbc_entry("int F(){int sum = 0;for(int i = 0,j = 1,k;i <= 100.; i++){sum += "
            "i;}return "
            "sum;}");
}

void tbc_for_noinc_init() {
  tbc_entry("int F(){int sum = 0,i = 0;for(;i <= 100;){sum += i;i++;}return "
            "sum;}");
}

void tbc_do() {
  tbc_entry("int F(){int sum = 0,i = 0;do{sum += i;i++;}while(i <= 100);return "
            "sum;}");
}

void tbc_do_done_now() {
  tbc_entry("int F(){int sum = 0,i = 1;do{sum += i;i++;}while(i <= 0);return "
            "sum;}");
}

void tbc_while() {
  tbc_entry("int F(){int sum = 0,i = 0;while(i <= 100){sum += i;i++;}return "
            "sum;}");
}

void tbc_while_done_now() {
  tbc_entry("int F(){int sum = 4,i = 1;while(i <= 0){sum += i;i++;}return "
            "sum;}");
}

void tbc_for_done_now() {
  tbc_entry(
      "int F(){int sum = 0;for(int i = 0;i <= -1;i++){sum += i;}return sum;}");
}

void tbc_always() {
  tbc_entry("void F(){void p(int);int i,j;for(i = 0,j = 1;;i+=j)p(i);}");
}

void tbc_continue_break() {
  tbc_entry("int F(){int sum = 0;for(int i = 0;i <= 100;i++){sum "
            "+=i;continue;break;sum +=i;}return sum;}");
}

void tbc_empty_label() { tbc_entry("int F(){int a; L1:a = 1; L2: }"); }

void tbc_if() { tbc_entry("int F(){int a = 1;if(a > 1){a = 2;}return a;}"); }

void tbc_if_else() {
  tbc_entry("int F(){int a = 1;if(a > 1){a = 2;}else{a = 3;}return a;}");
}

void tbc_if_else_chain() {
  tbc_entry(
      "int fib(int n){if(n <= 1){return n;}else{return fib(n-1) + fib(n-2);}}");
}

void tbc_enum() {
  tbc_entry("enum E{A = 1,B = 2,C = 4};int F(){enum E e = A|B|C;return e;}");
}

void tbc_switch_default() {
  tbc_entry("int F(){int a = 1;switch(a){int a = 0;default: a = 4;case 1: a = "
            "1;case 3: a = 3;case 2: a = "
            "2;break;case 5: a = 5; case 4: a = 4; }return a;}");
}

void tbc_switch_no_default() {
  tbc_entry("int F(){int a = 1;switch(a){int a = 0;case 1: a = 1;case 2: a = "
            "2;break;case 3: a = 3;}return a;}");
}

void tbc_switch_return() {
  tbc_entry("int F(int a){switch(a){case 1: return -1;case 2: return "
            "-2;case 3:case 4:  return -5; default: return a;}}");
}

void tbc_switch_dup() {
  tbc_entry("int F(int a){switch(a){case 1: return -1;case 1: return 2;}}");
}

void tbc_switch_dup_default() {
  tbc_entry(
      "int F(int a){switch(a){case 1: return -1;default: return 2;default: "
      "return 3;}}");
}
void tbc_switch_iter_mix() {
  tbc_entry("int F(int a) {\
  while (a < 10) {\
    a++;\
    if (a == 5)\
      break;\
    else if (a == 3)\
      continue;\
    else a++;\
    switch (a) {\
    case 1:\
      a = 1;\
      break;\
    case 2: {\
      int b = 1;\
      while (b < 10) {\
        b++;\
        if (b == 5)\
          break;\
        else if (b == 3)\
          continue;\
      }\
      continue;\
      return b;\
    }\
    case 3:\
      break;\
    default:\
      continue;\
    }\
  }\
  return a;\
}");
}

void tbc_return_void() { tbc_entry("void F(){return F();}"); }

void tbc_sc_memleak() {
  tbc_entry("\
    void F(){\
    int v1,v2=2,*vp;\
    v1 += v2;\
    v1 += vp[v1];\
    vp[v2] = v1;\
    }");
}

void tbc_and_not() {
  tbc_entry("int F(){int a = 1;int b = 2;return !(a && b);}");
}

void tbc_struct_get_member() {
  tbc_entry("struct S{int a;char b;};\
    int F(){\
      struct S s;\
      char c = s.b + s.a;\
  }");
}

void tbc_struct_deref_member() {
  tbc_entry("struct S{int a;char b;};\
    int F(){\
      struct S *sp,s1;\
      long c = sp->b + s1.a;\
  }");
}

void tbc_struct_lvalue_member() {
  tbc_entry("struct S{int a;char b;};\
    int F(){\
      struct S *sp,s1;\
      sp->a = s1.b;\
      s1.a = sp->a;\
  }");
}

void tbc_struct_typedef() {
  tbc_entry("typedef volatile long l_t;typedef struct S{int a;const l_t b;}S_t;\
    int F(){\
      S_t s;\
      s.a = 1;\
      s.b = 2;\
  }");
}

void tbc_struct_in_struct() {
  tbc_entry("struct s1{int a;struct s2 {int b; float c;} s;};\
    int F(){\
      struct s1 s;\
      s.a = 1;\
      s.s.c = 2;\
  }");
}

void tbc_struct_self() {
  tbc_entry("struct S{int a;char b;};\
    int F(struct S *sp){\
      ;\
      (sp+1)->a++;\
      (sp+1)->b-=1;\
  }");
}

void tbc_struct_return() {
  tbc_entry("struct S{int a;char b; short*fp;};\
    struct S F(){\
      struct S s;\
      s.a = 1;\
      s.b = 2;\
      s.fp = (void*)0;\
      return s;\
  }");
}

void tbc_struct_copy() {
  tbc_entry("struct S{int a;char b;double fpd; short*fp;};\
    double F(){\
      struct S s1,s2;\
      s2.a = 1;\
      s2.b = 2;\
      s2.fpd = 3;\
      s2.fp = (void*)0;\
      s1 = s2;\
      return s1.fpd + s1.a + s1.b;\
  }");
}

void tbc_struct_copy_from_outer() {
  tbc_entry("struct S{int a;char b;double fpd; short*fp;};\
    double F(struct S*src){\
      struct S s1,s2,s3,s4 = *src;\
      s3 = s2 = s1 = *&src[1];\
      *src = s4;\
      return src->fpd + s1.a + s2.b;\
  }");
}

void tbc_struct_wrong_self_inc() {
  tbc_entry("struct S{int a;char b;};\
    int F(struct S *sp){\
      (*sp)++;\
  }");
}

void tbc_union_copy() {
  tbc_entry(
      "union U{int a;char b;struct S{int a;char b;double fpd; short*fp;} s;};\
    int F(){\
      union U u1,u2 = u1;\
      u2 = u1 = u2;\
      struct S s;\
  }");
}

void tbc_union_member() {
  tbc_entry(
      "union U{int a;char b;struct S{int a;char b;double fpd; short*fp;} s;};\
    int F(){\
      union U u;\
      u.a = 256;\
      u.b = 2;\
      return u.a;\
  }");
}

void tbc_union_deref_member() {
  tbc_entry(
      "union U{int a;char b;struct S{int a;char b;double fpd; short*fp;} s;};\
    void F(union U *up){\
    up->a = 256;\
    up->b = 2;\
  }");
}

void tbc_union_struct_in_union() {
  tbc_entry(
      "union U{int a;char b;struct S{int a;char b;double fpd; short*fp;} s;};\
    void F(){\
      union U u;\
      u.s.a = 1;\
      u.s.b = 2;\
      u.s.fpd = 3;\
      u.s.fp = (void*)0;\
  }");
}

void tbc_union_struct_ptr_in_union() {
  tbc_entry(
      "union U{int a;char b;struct S{int a;char b;double fpd; short*fp;} *sp;};\
    void F(union U *u){\
      u->sp->a = 1;\
      u->sp->b = 2;\
      u->sp->fpd = 3;\
      u->sp->fp = (void*)0;\
  }");
}
