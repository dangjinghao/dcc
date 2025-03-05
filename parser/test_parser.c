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

x
void tpc_entry(char *code, int (*process_func)(struct lexer *)) {
  struct lexer lexer;
  lexer_from_string(&lexer, code);
  process_func(&lexer);
  lexer_destroy(&lexer);
}

void tpc_entry_expr(char *code) {
  struct lexer lexer;
  lexer_from_string(&lexer, code);
  process_expr(&lexer);
  lexer_destroy(&lexer);
}

void tpc_entry_declaration(char *code) {
  struct lexer lexer;
  lexer_from_string(&lexer, code);
  process_declaration(&lexer);
  lexer_destroy(&lexer);
}

void tpc_entry_trans_unit(char *code) {
  struct lexer lexer;
  lexer_from_string(&lexer, code);
  process_trans_unit(&lexer);
  lexer_destroy(&lexer);
}

void tpc_entry_statement(char *code) {
  struct lexer lexer;
  lexer_from_string(&lexer, code);
  process_statement(&lexer);
  lexer_destroy(&lexer);
}

void tpc_expr() {
  tpc_entry_expr("a1,a2,V1 = V2 = C1?C2?T1:F1:C3?T2:F2");
}

void tpc_paren_expr() {
  tpc_entry_expr("1+(((((1))))) + 2*(3+4)");
}

void tpc_str_combine() {
  tpc_entry_expr(".1f + \"123\" \"456\" + '\\'' ");
}

void tpc_declaration() {
  tpc_entry_trans_unit("int a = 1, b = a, **((*c)) = 1+(((((1))))) + 2*(3+4);");
}

void tpc_statement() {
  tpc_entry_statement("a = b = 1,2,3,4;");
}

void tpc_label() {
  tpc_entry_statement("label: a = 1;");
}

void tpc_typedef_statement() {
  tpc_entry_trans_unit("typedef int i,*ip;i a = 1; ip b = &a;");
}

void tpc_typedef_redef() {
  tpc_entry_trans_unit("typedef int i;i i;");
}

void tpc_typedef2() {
  tpc_entry_trans_unit("typedef int i,*ip;i a = 1; ip b = &a; typedef struct{i i; ip p;} ST; ST sn;");
}

void tpc_typedef3() {
  tpc_entry_trans_unit("typedef const int *I;typedef volatile I* IP;static const IP **v, v1;static volatile const int ****v2;");
}

void tpc_extern_subscope() {
  tpc_entry_trans_unit("int F(){{extern F2(); F2();}} int F2(){return 0;} int F2();");
}

void tpc_func_declaration() {
  tpc_entry_declaration("int func(int a, int (*)(int,char) ,...);");
}

void tpc_typecast() {
  tpc_entry_expr("(const int)1 + (void*)2 + (int(*)(int,char))0");
}

void tpc_unary_expr() {
  tpc_entry_expr("(void*)0 + 1+(((((1))))) + ++!-+~*&a++--++-- * (1 + 2)");
}

void tpc_unary_advance_post_expr() {
  tpc_entry_expr("a[1].x+++&b[2]->member.m(1,2.f,\"hello\",0x1234)");
}

void tpc_var_no_exists() {
  tpc_entry_trans_unit("int a=1,b=a,b=c;");
}

void tpc_var_redef() {
  tpc_entry_trans_unit("int a = 1,a = 2;");
}

void tpc_function_def() {
  tpc_entry_trans_unit("typedef int I;int func(int a, int b){int c = 1; c= a+b + c;}\nI func2(I a,char c){ func;}");
}

void tpc_struct() {
  tpc_entry_trans_unit("struct stu{int id: 4;union {int i;char c;}_t;} STU;struct stu s2;");
}

void tpc_struct_undef() {
  tpc_entry_trans_unit("struct stu{int id: 4;union {int i;char c;}_t;} STU;struct stu2 s2;");
}

void tpc_struct_redef() {
  tpc_entry_trans_unit("struct stu{int id: 4;union {int i;char c;}_t;} STU;struct stu {char* id;};");
}

void tpc_extern() {
  tpc_entry_trans_unit("extern int A;\n int A; extern int A;");
}

void tpc_extern_redef() {
  tpc_entry_trans_unit("extern int A;\n int A; extern int A; int A;");
}

void tpc_decl_def_decl() {
  tpc_entry_trans_unit("int A();extern int A(); int A(); int A(){} int A();");
}

void tpc_decl_def_decl_redef() {
  tpc_entry_trans_unit("int A();extern int A(); int A(); int A(){} int A();int A(){}");
}

void tpc_completed_code() {
  tpc_entry_trans_unit("int ** (*v1)(int,char (*)(void)) = 0; int F(int,char c);void main(int arg){int v2 = 2; {v2 = 20; int v3 = 3; F(1,2);} F(3,4);} int arg = 2;int F(int i,char c){return i + c;}");
}

void tpc_initializer_list() {
  tpc_entry_trans_unit("int A[] = {1,2,3,4,5,6,7,8,9,10}, v1 = 1;struct{char a;char s[10];int is[10];} s = {'c',\"Hello\",{1,2,3,4,5},};");
}

void tpc_jump_statement() {
  tpc_entry_trans_unit("int main(){goto label;{{label: return 0;}}}");
}

void tpc_enum() {
  tpc_entry_trans_unit("enum E{A,B,C};enum E e = A; enum {A0 = A, B0 = B, C0} E1;");
}

void tpc_redef_enum_struct() {
  tpc_entry_trans_unit("enum E{A,B,C}; struct E {int i;};");
}

void tpc_redef_enum_enum() {
  tpc_entry_trans_unit("enum E{A,B,C}; enum E {CC};");
}

[[gnu::constructor]] void init() {
  log_color_enable(true);
  log_set_level(LOG_LEVEL_DEBUG);
  log_debug("test_parser init");
}