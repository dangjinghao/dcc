#include "dcc.h"
#include <errno.h>
#include <libgen.h>
#include <signal.h>
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
  } else if (WIFSIGNALED(status)) {
    error("run_subprocess: %s terminated by signal %s(%d)", argv[0],
          strsignal(WTERMSIG(status)), WTERMSIG(status));
  }
}

static void run_cc1(char *input, char *output, PtrArray *args,
                    char *real_filename) {
  strarray_push(args, "-cc1");
  strarray_push(args, "-cc1-input");
  strarray_push(args, input);
  strarray_push(args, "-cc1-output");
  strarray_push(args, output);
  strarray_push(args, "-cc1-filename");
  strarray_push(args, real_filename);
  strarray_push(args, NULL);

  run_subprocess((char **)args->data);
}

noreturn void cc1() {
  Token *tokens = tokenize_file(opt_cc1_input);
  Obj *ast = parse(tokens);
  FILE *output_file = fopen(opt_cc1_output, "wb");

  codegen(ast, output_file, !opt_emit_llvm);

  fclose(output_file);
  exit(0);
}

static void pack_args(int argc, char *argv[], PtrArray *arr) {
  for (int i = 0; i < argc; i++) {
    strarray_push(arr, argv[i]);
  }
}

static void expand_macro(char *input, char *output) {
  char *compiler_include = format("%s/../include", path_get_exedir());
  char *argv[] = {"cpp",
                  "-E",
                  "-w",
                  "-nostdinc",
                  "-undef",
                  "-include",
                  "dccdefs.h",
                  "-isystem",
                  compiler_include,
                  "-isystem",
                  "/usr/local/include",
                  "-isystem",
                  "/usr/include/x86_64-linux-gnu",
                  "-isystem",
                  "/usr/include",
                  NULL};

  PtrArray args_full = {};

  strarray_push_batch(&args_full, argv);
  strarray_push_batch2(&args_full, &opt_cpp_extra_args);

  if (opt_M)
    strarray_push(&args_full, "-M");
  if (opt_MD)
    strarray_push(&args_full, "-MD");
  if (opt_MM)
    strarray_push(&args_full, "-MM");
  if (opt_MMD)
    strarray_push(&args_full, "-MMD");
  if (opt_MP)
    strarray_push(&args_full, "-MP");
  if (opt_MG)
    strarray_push(&args_full, "-MG");

  if (opt_MT) {
    strarray_push(&args_full, "-MT");
    strarray_push(&args_full, opt_MT);
  } else if (opt_MD || opt_MMD || opt_M || opt_MM) {
    strarray_push(&args_full, "-MT");
    strarray_push(
        &args_full,
        opt_o ?: path_new_replaced_suffix(basename(strdup(input)), ".o"));
  }

  if (opt_MF) {
    strarray_push(&args_full, "-MF");
    strarray_push(&args_full, opt_MF);
  } else if (opt_MD || opt_MMD) {
    strarray_push(&args_full, "-MF");
    strarray_push(
        &args_full,
        opt_o ? path_new_replaced_suffix(strdup(opt_o), ".d")
              : path_new_replaced_suffix(basename(strdup(input)), ".d"));
  }

  strarray_push(&args_full, "-o");
  strarray_push(&args_full, output);
  strarray_push(&args_full, input);
  strarray_push(&args_full, NULL);

  run_subprocess((char **)args_full.data);
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

static void run_linker(PtrArray *inputs, char *output) {
  PtrArray arr = {};

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
  strarray_push(&arr, "-L/usr/lib64");
  // strarray_push(&arr, "-L/lib64");
  strarray_push(&arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-pc-linux-gnu");
  strarray_push(&arr, "-L/usr/lib/x86_64-redhat-linux");
  strarray_push(&arr, "-L/usr/lib");
  // strarray_push(&arr, "-L/lib");

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

  run_subprocess((char **)arr.data);
}

int main(int argc, char *argv[]) {
  parse_args(argc, argv);

  if (opt_cc1) {
    cc1();
    unreachable();
  }

  if (opt_inputfiles.len > 1 && opt_o && (opt_c || opt_S | opt_E))
    error("cannot specify '-o' with '-c,' '-S' or '-E' with multiple files");

  PtrArray ld_objs = {};
  for (int i = 0; i < opt_inputfiles.len; i++) {
    InputFile *input_file = opt_inputfiles.data[i];
    if (input_file->type == FILETYPE_C) {
      char *expanded_file = path_new_tmpfile();
      char *asm_file = path_new_tmpfile();
      char *obj_file = path_new_tmpfile();

      expand_macro(input_file->path, expanded_file);
      if (opt_E) {
        path_cp(opt_o ?: "-", expanded_file);
        continue;
      }
      PtrArray args = {};
      pack_args(argc, argv, &args);

      // generate *.s
      run_cc1(expanded_file, asm_file, &args, input_file->path);

      if (opt_S) {
        path_cp(opt_o ?: "-", asm_file);
        continue;
      }
      // generate *.o
      assemble(asm_file, obj_file);

      if (opt_c) {
        path_cp(opt_o
                    ?: path_new_replaced_suffix(
                           basename(strdup(input_file->path)), ".o"),
                obj_file);
        continue;
      }
      strarray_push(&ld_objs, obj_file);
      continue;
    } else if (input_file->type == FILETYPE_ASM) {
      char *asm_file = input_file->path;
      char *obj_file = path_new_tmpfile();
      // generate *.o
      assemble(asm_file, obj_file);

      if (opt_c) {
        path_cp(opt_o
                    ?: path_new_replaced_suffix(
                           basename(strdup(input_file->path)), ".o"),
                obj_file);
        continue;
      }
      strarray_push(&ld_objs, obj_file);
      continue;
    } else {
      strarray_push(&ld_objs, input_file->path);
      continue;
    }
  }

  if (opt_E || opt_S || opt_c) {
    return 0;
  }

  run_linker(&ld_objs, opt_o ?: "a.out");
  return 0;
}
