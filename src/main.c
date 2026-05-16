#include "dcc.h"
#include <errno.h>
#include <libgen.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void run_subprocess(char **argv) {
  if (argv == NULL || argv[0] == NULL) {
    fprintf(stderr,
            "run_subprocess: invalid arguments (NULL or empty program name)\n");
    return;
  }
  if (opt_hash_hash_hash) {
    fprintf(stderr, "%s", argv[0]);
    for (int i = 1; argv[i]; i++)
      fprintf(stderr, " %s", argv[i]);
    fprintf(stderr, "\n");
  }
  pid_t pid;
  extern char **environ;
  int ret = posix_spawnp(&pid, argv[0], NULL, NULL, argv, environ);
  if (ret != 0) {
    error("posix_spawnp failed: %s (errno=%d)\n", strerror(ret), ret);
  }
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    error("waitpid failed: %s (errno=%d)\n", strerror(errno), errno);
  }

  if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
    error("run_subprocess: %s exited with error code: %d", argv[0],
          WEXITSTATUS(status));
  }
}

static void run_cc1(char *input, char *output, StringArray *args) {
  strarray_push(args, "-cc1");
  strarray_push(args, "-cc1-input");
  strarray_push(args, input);
  strarray_push(args, "-cc1-output");
  strarray_push(args, output);
  strarray_push(args, NULL);

  run_subprocess(args->data);
}

noreturn void cc1() {
  Token *tokens = tokenize_file(opt_cc1_input);
  Obj *ast = parse(tokens);
  char *ir_tmp_path = path_new_tmpfile();
  FILE *ir_file = fopen(ir_tmp_path, "wb");

  codegen(ast, ir_file);

  fclose(ir_file);
  ir_file = NULL;

  if (opt_ir) {
    path_cp(opt_cc1_output, ir_tmp_path);
    exit(0);
  }

  char *args[] = {"llc", "-o", opt_cc1_output, ir_tmp_path, NULL};
  run_subprocess(args);

  exit(0);
}

static void pack_args(int argc, char *argv[], StringArray *arr) {
  for (int i = 0; i < argc; i++) {
    strarray_push(arr, argv[i]);
  }
}

static void expand_macro(char *input, char *output, char *argv0) {
  char *compiler_include = format("-I%s/../include", dirname(strdup(argv0)));
  char *argv[] = {"cpp",
                  "-P",
                  "-E",
                  "-w",
                  "-undef",
                  "-D_LP64=1",
                  "-D__C99_MACRO_WITH_VA_ARGS=1",
                  "-D__ELF__=1",
                  "-D__LP64__=1",
                  "-D__SIZEOF_DOUBLE__=8",
                  "-D__SIZEOF_FLOAT__=4",
                  "-D__SIZEOF_INT__=4",
                  "-D__SIZEOF_LONG_DOUBLE__=8",
                  "-D__SIZEOF_LONG_LONG__=8",
                  "-D__SIZEOF_LONG__=8",
                  "-D__SIZEOF_POINTER__=8",
                  "-D__SIZEOF_PTRDIFF_T__=8",
                  "-D__SIZEOF_SHORT__=2",
                  "-D__SIZEOF_SIZE_T__=8",
                  "-D__SIZE_TYPE__=unsigned long",
                  "-D__STDC_HOSTED__=1",
                  "-D__STDC_NO_COMPLEX__=1",
                  "-D__STDC_UTF_16__=1",
                  "-D__STDC_UTF_32__=1",
                  "-DSTDC_VERSION=201112L",
                  "-D__STDC__=1",
                  "-D__USER_LABEL_PREFIX__=",
                  "-D__alignof__(x)=_Alignof(x)",
                  "-D__amd64=1",
                  "-D__amd64__=1",
                  "-D__const__=const",
                  "-D__gnu_linux__=1",
                  "-D__inline__=inline",
                  "-D__linux=1",
                  "-D__linux__=1",
                  "-D__signed__=signed",
                  "-D__typeof__(x)=typeof(x)",
                  "-D__unix=1",
                  "-D__unix__=1",
                  "-D__volatile__=volatile",
                  "-D__x86_64=1",
                  "-D__x86_64__=1",
                  "-Dlinux=1",
                  "-Dunix=1",
                  "-D__dcc__=1",
                  compiler_include,
                  "-I/usr/local/include",
                  "-I/usr/include/x86_64-linux-gnu",
                  "-I/usr/include",
                  "-o",
                  output,
                  input,
                  NULL};

  StringArray args_full = {};

  strarray_push_batch(&args_full, argv);
  strarray_push_batch2(&args_full, &opt_cpp_extra_args);

  run_subprocess(args_full.data);
}

static void assemble(char *input, char *output) {
  char *cmd[] = {"as", "-c", input, "-o", output, NULL};
  run_subprocess(cmd);
}

static char *find_libpath(void) {
  if (path_exists("/usr/lib/x86_64-linux-gnu/crti.o"))
    return "/usr/lib/x86_64-linux-gnu";
  if (path_exists("/usr/lib64/crti.o"))
    return "/usr/lib64";
  error("library path is not found");
}

static char *find_gcc_libpath(void) {
  char *paths[] = {
      "/usr/lib/gcc/x86_64-linux-gnu/*/crtbegin.o",
      "/usr/lib/gcc/x86_64-pc-linux-gnu/*/crtbegin.o", // For Gentoo
      "/usr/lib/gcc/x86_64-redhat-linux/*/crtbegin.o", // For Fedora
  };

  for (int i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
    char *path = path_find_file(paths[i]);
    if (path)
      return dirname(path);
  }

  error("gcc library path is not found");
}

static void run_linker(StringArray *inputs, char *output) {
  StringArray arr = {};

  strarray_push(&arr, "ld");
  strarray_push(&arr, "-o");
  strarray_push(&arr, output);
  strarray_push(&arr, "-m");
  strarray_push(&arr, "elf_x86_64");

  char *libpath = find_libpath();
  char *gcc_libpath = find_gcc_libpath();

  if (opt_shared) {
    strarray_push(&arr, format("%s/crti.o", libpath));
    strarray_push(&arr, format("%s/crtbeginS.o", gcc_libpath));
  } else {
    strarray_push(&arr, format("%s/crt1.o", libpath));
    strarray_push(&arr, format("%s/crti.o", libpath));
    strarray_push(&arr, format("%s/crtbegin.o", gcc_libpath));
  }

  strarray_push(&arr, format("-L%s", gcc_libpath));
  strarray_push(&arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(&arr, "-L/usr/lib64");
  strarray_push(&arr, "-L/lib64");
  strarray_push(&arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-pc-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-redhat-linux");
  strarray_push(&arr, "-L/usr/lib");
  strarray_push(&arr, "-L/lib");

  if (!opt_static) {
    strarray_push(&arr, "-dynamic-linker");
    strarray_push(&arr, "/lib64/ld-linux-x86-64.so.2");
  }

  for (int i = 0; i < opt_ld_extra_args.len; i++)
    strarray_push(&arr, opt_ld_extra_args.data[i]);

  for (int i = 0; i < inputs->len; i++)
    strarray_push(&arr, inputs->data[i]);

  if (opt_static) {
    strarray_push(&arr, "--start-group");
    strarray_push(&arr, "-lgcc");
    strarray_push(&arr, "-lgcc_eh");
    strarray_push(&arr, "-lc");
    strarray_push(&arr, "--end-group");
  } else {
    strarray_push(&arr, "-lc");
    strarray_push(&arr, "-lgcc");
    strarray_push(&arr, "--as-needed");
    strarray_push(&arr, "-lgcc_s");
    strarray_push(&arr, "--no-as-needed");
  }

  if (opt_shared)
    strarray_push(&arr, format("%s/crtendS.o", gcc_libpath));
  else
    strarray_push(&arr, format("%s/crtend.o", gcc_libpath));

  strarray_push(&arr, format("%s/crtn.o", libpath));
  strarray_push(&arr, NULL);

  run_subprocess(arr.data);
}

int main(int argc, char *argv[]) {
  parse_args(argc, argv);

  if (opt_cc1) {
    cc1();
    unreachable();
  }

  if (opt_input_paths.len > 1 && opt_o && (opt_c || opt_S | opt_E))
    error("cannot specify '-o' with '-c,' '-S' or '-E' with multiple files");

  StringArray ld_objs = {0};
  for (int i = 0; i < opt_input_paths.len; i++) {
    char *input_file = opt_input_paths.data[i];
    char *expanded_file = path_new_tmpfile();
    char *asm_file = path_new_tmpfile();
    char *obj_file = path_new_tmpfile();
    // generate *.c
    expand_macro(input_file, expanded_file, argv[0]);
    if (opt_E) {
      path_cp(opt_o ?: "-", expanded_file);
      continue;
    }
    StringArray args = {0};
    pack_args(argc, argv, &args);

    // generate *.s
    run_cc1(expanded_file, asm_file, &args);

    if (opt_ir)
      opt_S = true;
    if (opt_S) {
      path_cp(opt_o ?: "-", asm_file);
      continue;
    }
    // generate *.o
    assemble(asm_file, obj_file);

    if (opt_c) {
      path_cp(
          opt_o ?: path_new_replaced_suffix(basename(strdup(input_file)), ".o"),
          obj_file);
      continue;
    }

    strarray_push(&ld_objs, obj_file);
  }
  if (opt_E || opt_S || opt_c) {
    return 0;
  }
  run_linker(&ld_objs, opt_o ?: "a.out");
  return 0;
}
