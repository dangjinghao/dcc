#include "dcc.h"
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static StringArray tmpfiles;

static char *create_tmpfile(void) {
  char *path = strdup("/tmp/dcc-XXXXXX");
  int fd = mkstemp(path);
  if (fd == -1)
    error("mkstemp failed: %s", strerror(errno));
  close(fd);

  strarray_push(&tmpfiles, path);
  return path;
}

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
    fprintf(stderr, "posix_spawnp failed: %s (errno=%d)\n", strerror(ret), ret);
    return;
  }
  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
    return;
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
  FILE *ir_file = NULL;
  char *ir_tmp_path = NULL;
  if (opt_ir) {
    if (strcmp(opt_cc1_output, "-") == 0) {
      ir_file = stdout;
    } else {
      ir_file = fopen(opt_cc1_output, "w");
    }
  } else {
    ir_tmp_path = create_tmpfile();
    ir_file = fopen(ir_tmp_path, "w");
  }

  codegen(ast, ir_file);
  if (ir_file != stdout)
    fclose(ir_file);
  ir_file = NULL;

  if (!opt_ir) {
    StringArray args = {0};
    strarray_push(&args, "llc");
    strarray_push(&args, "-o");
    strarray_push(&args, opt_cc1_output);
    strarray_push(&args, ir_tmp_path);
    strarray_push(&args, NULL);
    run_subprocess(args.data);
  }

  exit(0);
}

static void cleanup(void) {
  for (int i = 0; i < tmpfiles.len; i++)
    unlink(tmpfiles.data[i]);
}

static void pack_args(int argc, char *argv[], StringArray *arr) {
  while (argc--) {
    strarray_push(arr, argv[argc]);
  }
}

int main(int argc, char *argv[]) {
  parse_args(argc, argv);

  atexit(cleanup);

  if (opt_cc1) {
    cc1();
    unreachable();
  }

  return 0;
}
