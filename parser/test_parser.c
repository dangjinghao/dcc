#include "ast.h"
#include "convert/convert.h"
#include "lexer.h"
#include "log/log.h"
#include "parser.h"
#include <stdbool.h>

int process_expr(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = parse_expression(&parser);
  sds buf = sdsempty();
  buf = convert_ast_to_json(n, buf, false);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);

  return 0;
}

int process_declaration(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = ast_new(ast_block);
  parse_external_declaration(&parser, &n->block.list);
  sds buf = sdsempty();
  buf = convert_ast_to_json(n, buf, false);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);
  return 0;
}

int process_trans_unit(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = parse_translation_unit(&parser);
  sds buf = sdsempty();
  buf = convert_ast_to_json(n, buf, false);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);
  return 0;
}

int process_statement(struct lexer *lexer) {
  struct parser parser;
  parser_from_lexer(&parser, lexer);
  astn n = parse_statement(&parser);
  sds buf = sdsempty();
  buf = convert_ast_to_json(n, buf, false);
  ast_free(n);
  printf("%s\n", buf);
  sdsfree(buf);
  parser_destory(&parser);
  return 0;
}

int ptc_ternary() {
  struct lexer lexer;
  lexer_from_string(&lexer, "a1,a2,V1 = V2 = C1?C2?T1:F1:C3?T2:F2");
  process_expr(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int tesecase_paren_expr() {
  struct lexer lexer;
  lexer_from_string(&lexer, "1+(((((1))))) + 2*(3+4)");
  process_expr(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int ptc_str_combine() {
  struct lexer lexer;
  lexer_from_string(&lexer, ".1f + \"123\" \"456\" + '\\'' ");
  process_expr(&lexer);
  lexer_destroy(&lexer);

  return 0;
}

int ptc_declaration() {
  struct lexer lexer;
  lexer_from_string(&lexer,
                    "int a = 1, b = a, **((*c)) = 1+(((((1))))) + 2*(3+4);");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_statement() {
  struct lexer lexer;
  lexer_from_string(&lexer, "a = b = 1,2,3,4;");
  process_statement(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_label() {
  struct lexer lexer;
  lexer_from_string(&lexer, "label: a = 1;");
  process_statement(&lexer);
  lexer_destroy(&lexer);
  return 0;
}
int ptc_typedef_statement() {
  struct lexer lexer;
  lexer_from_string(&lexer, "typedef int i,*ip;i a = 1; ip b = &a;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_typedf_redef() {
  struct lexer lexer;
  lexer_from_string(&lexer, "typedef int i;i i;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_typedef2() {
  struct lexer lexer;
  lexer_from_string(&lexer, "typedef int i,*ip;i a = 1; ip b = &a; typedef "
                            "struct{i I; ip p;} ST; ST sn;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_typedef3(){
  struct lexer lexer;
  lexer_from_string(&lexer, "typedef const int *I;typedef volatile I* IP;const IP **v, v1;volatile const int ****v2;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_func_declaration() {
  struct lexer lexer;
  lexer_from_string(&lexer, "int func(int a, int (*)(int,char) ,...);");
  process_declaration(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_typecast() {
  struct lexer lexer;
  lexer_from_string(&lexer, "(const int)1 + (void*)2 + (int(*)(int,char))0");
  process_expr(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_unary_expr() {
  struct lexer lexer;
  lexer_from_string(&lexer,
                    "(void*)0 + 1+(((((1))))) + ++!-+~*&a++--++-- * (1 + 2)");
  process_expr(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_unary_advance_post_expr() {
  struct lexer lexer;
  lexer_from_string(&lexer, "a[1].x+++&b[2]->member.m(1,2.f,\"hello\",0x1234)");
  process_expr(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_var_no_exists() {
  struct lexer lexer;
  lexer_from_string(&lexer, "int a=1,b=a,b=c;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_var_redef() {
  struct lexer lexer;
  lexer_from_string(&lexer, "int a = 1,a = 2;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_function_def() {
  struct lexer lexer;
  lexer_from_string(&lexer, "typedef int I;int func(int a, int b){int c = 1; "
                            "c= a+b + c;}\nI func2(I a,char c){ func;}");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_struct() {
  struct lexer lexer;
  lexer_from_string(
      &lexer,
      "struct stu{int id: 4;union {int i;char c;}_t;} STU;struct stu s2;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}
int ptc_struct_undef() {
  struct lexer lexer;
  lexer_from_string(
      &lexer,
      "struct stu{int id: 4;union {int i;char c;}_t;} STU;struct stu2 s2;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_struct_redef() {
  struct lexer lexer;
  lexer_from_string(&lexer, "struct stu{int id: 4;union {int i;char c;}_t;} "
                            "STU;struct stu {char* id;};");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_extern() {
  struct lexer lexer;
  lexer_from_string(&lexer, "extern int A;\n int A; extern int A;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}
int ptc_extern_redef() {
  struct lexer lexer;
  lexer_from_string(&lexer, "extern int A;\n int A; extern int A; int A;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_decl_def_decl() {
  struct lexer lexer;
  lexer_from_string(&lexer,
                    "int A();extern int A(); int A(); int A(){} int A();");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_decl_def_decl_redef() {
  struct lexer lexer;
  lexer_from_string(
      &lexer, "int A();extern int A(); int A(); int A(){} int A();int A(){}");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_completed_code() {
  struct lexer lexer;
  lexer_from_string(&lexer,
                    "int ** (*v1)(int,char (*)(void)) = 0; int F(int,char c);"
                    "void main(int arg){int v2 = 2; {v2 = 20; int v3 = 3; "
                    "F(1,2);} F(3,4);} int arg = 2;");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_initializer_list() {
  struct lexer lexer;
  lexer_from_string(
      &lexer, "int A[] = {1,2,3,4,5,6,7,8,9,10}, v1 = 1;struct{char a;char "
              "s[10];int is[10];} s = {'c',\"Hello\",{1,2,3,4,5},};");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}

int ptc_jump_statement() {
  struct lexer lexer;
  lexer_from_string(&lexer, "int main(){goto label;{{label: return 0;}}}");
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
  return 0;
}
[[gnu::constructor]] void init() {
  log_color_enable(true);
  log_set_level(LOG_LEVEL_DEBUG);
  log_debug("test_parser init");
}