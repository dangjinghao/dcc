#include "dcc.h"
#include <stdlib.h>
#include <string.h>

bool opt_E;              // expand macro
bool opt_S;              // generate *.s
bool opt_c;              // generate *.o
bool opt_hash_hash_hash; // dump the subprocess's command line
bool opt_ir;             // generate llvm ir file *.ll
bool opt_static;
bool opt_shared;
bool opt_fcommon;
bool opt_fpic;
char *opt_o;

StringArray opt_input_paths;
StringArray opt_ld_extra_args;
StringArray opt_cpp_extra_args;

// cc1 mode
bool opt_cc1;
char *opt_cc1_input;
char *opt_cc1_output;

static void usage(int status, char *argv0) {
  fprintf(stderr,
          "%s [ -o <path> ] <file>\n"
          "cc1 mode:\n"
          "\t-cc1 -cc1-input <path> -cc1-output <path> [-ir]\n",
          argv0);
  exit(status);
}

void parse_args(int argc, char **argv) {

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-cc1")) {
      opt_cc1 = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1-input")) {
      opt_cc1_input = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-cc1-output")) {
      opt_cc1_output = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-o")) {
      opt_o = argv[++i];
      continue;
    }

    if (!strncmp(argv[i], "-o", 2)) {
      opt_o = argv[i] + 2;
      continue;
    }

    if (!strcmp(argv[i], "-I")) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      strarray_push(&opt_cpp_extra_args, argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-I", 2)) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-D")) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      strarray_push(&opt_cpp_extra_args, argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-D", 2)) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-U")) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      strarray_push(&opt_cpp_extra_args, argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-U", 2)) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-include")) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      strarray_push(&opt_cpp_extra_args, argv[++i]);
      continue;
    }

    // if (!strcmp(argv[i], "-x")) {
    //   opt_x = parse_opt_x(argv[++i]);
    //   continue;
    // }

    // if (!strncmp(argv[i], "-x", 2)) {
    //   opt_x = parse_opt_x(argv[i] + 2);
    //   continue;
    // }

    if (!strncmp(argv[i], "-l", 2) || !strncmp(argv[i], "-Wl,", 4)) {
      strarray_push(&opt_input_paths, argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-Xlinker")) {
      strarray_push(&opt_ld_extra_args, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-s")) {
      strarray_push(&opt_ld_extra_args, argv[i]);
      continue;
    }

    if (!strncmp(argv[i], "-M", 2)) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      switch (argv[i][2]) {
      case 'F':
      case 'Q':
      case 'T':
        strarray_push(&opt_cpp_extra_args, argv[++i]);
      default:
        break;
      }
      continue;
    }

    if (!strcmp(argv[i], "-fpic") || !strcmp(argv[i], "-fPIC")) {
      opt_fpic = true;
      continue;
    }

    if (!strcmp(argv[i], "-idirafter")) {
      strarray_push(&opt_cpp_extra_args, argv[i]);
      strarray_push(&opt_cpp_extra_args, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-###")) {
      opt_hash_hash_hash = true;
      continue;
    }

    if (!strcmp(argv[i], "-ir")) {
      opt_ir = true;
      continue;
    }

    if (!strcmp(argv[i], "-S")) {
      opt_S = true;
      continue;
    }

    if (!strcmp(argv[i], "-E")) {
      opt_E = true;
      continue;
    }

    if (!strcmp(argv[i], "-c")) {
      opt_c = true;
      continue;
    }

    if (!strcmp(argv[i], "-static")) {
      opt_static = true;
      strarray_push(&opt_ld_extra_args, "-static");
      continue;
    }

    if (!strcmp(argv[i], "-shared")) {
      opt_shared = true;
      strarray_push(&opt_ld_extra_args, "-shared");
      continue;
    }

    if (!strcmp(argv[i], "-L")) {
      strarray_push(&opt_ld_extra_args, "-L");
      strarray_push(&opt_ld_extra_args, argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-L", 2)) {
      strarray_push(&opt_ld_extra_args, "-L");
      strarray_push(&opt_ld_extra_args, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-fcommon")) {
      opt_fcommon = true;
      continue;
    }

    if (!strcmp(argv[i], "-fno-common")) {
      opt_fcommon = false;
      continue;
    }

    if (!strcmp(argv[i], "--help"))
      usage(0, argv[0]);

    // These options are ignored for now.
    if (!strncmp(argv[i], "-O", 2) || !strncmp(argv[i], "-W", 2) ||
        !strncmp(argv[i], "-g", 2) || !strncmp(argv[i], "-std=", 5) ||
        !strcmp(argv[i], "-ffreestanding") ||
        !strcmp(argv[i], "-fno-builtin") ||
        !strcmp(argv[i], "-fno-omit-frame-pointer") ||
        !strcmp(argv[i], "-fno-stack-protector") ||
        !strcmp(argv[i], "-fno-strict-aliasing") || !strcmp(argv[i], "-m64") ||
        !strcmp(argv[i], "-mno-red-zone") || !strcmp(argv[i], "-w"))
      continue;

    if (argv[i][0] == '-' && argv[i][1] != '\0')
      error("unknown argument: %s", argv[i]);

    strarray_push(&opt_input_paths, argv[i]);
  }

  if (!opt_cc1) {
    if (opt_input_paths.len == 0)
      error("no input file");
  }
}
