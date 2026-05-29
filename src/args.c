#include "dcc.h"
#include <stdlib.h>
#include <string.h>

bool opt_E;              // expand macro
bool opt_S;              // generate *.s
bool opt_c;              // generate *.o
bool opt_hash_hash_hash; // dump the subprocess's command line
bool opt_emit_llvm;      // generate llvm ir file *.ll
bool opt_static;
bool opt_shared;
bool opt_fcommon;
bool opt_fpic;
char *opt_o;

bool opt_M;
bool opt_MD;
bool opt_MM;
bool opt_MMD;
bool opt_MP;
bool opt_MG;
char *opt_MF;
char *opt_MT;

PtrArray opt_inputfiles;
PtrArray opt_ld_extra_args;
PtrArray opt_cpp_extra_args;

// cc1 mode
bool opt_cc1;
char *opt_cc1_input;
char *opt_cc1_output;
char *opt_cc1_filename;

static InputFileType force_input_file_type = FILETYPE_NONE;

static InputFileType get_file_type(char *filename) {
  if (force_input_file_type != FILETYPE_NONE) {
    return force_input_file_type;
  }

  if (str_endswith(filename, ".a"))
    return FILETYPE_AR;
  if (str_endswith(filename, ".so"))
    return FILETYPE_DSO;
  if (str_endswith(filename, ".o"))
    return FILETYPE_OBJ;
  if (str_endswith(filename, ".c"))
    return FILETYPE_C;
  if (str_endswith(filename, ".s"))
    return FILETYPE_ASM;

  return FILETYPE_NONE;
}

static void usage(int status) {
  fprintf(
      stderr,
      "Usage: dcc [options] <file>...\n"
      "Options:\n"
      "  -o <path>              place output into <path>\n"
      "  -I<dir> / -I <dir>     add include directory\n"
      "  -D<macro> / -D <macro> define macro\n"
      "  -U<macro> / -U <macro> undefine macro\n"
      "  -include <file>        include header before main input\n"
      "  -x <lang>              force input language (c|assembler|none)\n"
      "  -l<lib>                link with library\n"
      "  -Wl,<args>             pass comma-separated args to linker\n"
      "  -Xlinker <arg>         pass arg to linker\n"
      "  -s                     pass -s to linker\n"
      "  -M, -MD, -MM, -MMD     dependency generation options\n"
      "  -MF <file>             write deps to file\n"
      "  -MT <target>           set dependency target\n"
      "  -MG, -MP               dependency options\n"
      "  -fpic, -fPIC           generate position-independent code\n"
      "  -idirafter <dir>       add include directory after others\n"
      "  -###                   dump subprocess command line\n"
      "  -emit-llvm             generate LLVM IR\n"
      "  -S                     stop after assembly (output .s)\n"
      "  -E                     preprocess only (output .i)\n"
      "  -c                     compile only (output .o)\n"
      "  -static, -shared       pass to linker\n"
      "  -L<dir> / -L <dir>     add library search path\n"
      "  -fcommon / -fno-common\n"
      "  --help                show this help\n"
      "cc1 mode:\n"
      "  -cc1 -cc1-input <path> -cc1-output <path> [-emit-llvm] -cc1-filename "
      "<path>\n"
      "Infomation:\n"
      "\n"
      "dcc include path: %s/../include\n",
      path_get_exedir());
  exit(status);
}

static InputFile *new_inputfile(char *filepath, InputFileType t) {
  InputFile *r = calloc(1, sizeof(InputFile));
  r->path = filepath;
  r->type = t;
  return r;
}

static InputFileType parse_opt_x(char *s) {
  if (!strcmp(s, "c"))
    return FILETYPE_C;
  if (!strcmp(s, "assembler"))
    return FILETYPE_ASM;
  if (!strcmp(s, "none"))
    return FILETYPE_NONE;
  error("CLI: unknown argument for -x: %s", s);
}

void parse_args(int argc, char **argv) {

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-cc1")) {
      opt_cc1 = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1-input")) {
      opt_cc1 = true;
      opt_cc1_input = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-cc1-output")) {
      opt_cc1 = true;
      opt_cc1_output = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-cc1-filename")) {
      opt_cc1 = true;
      opt_cc1_filename = argv[++i];
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

    if (!strcmp(argv[i], "-x")) {
      force_input_file_type = parse_opt_x(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-x", 2)) {
      force_input_file_type = parse_opt_x(argv[i] + 2);
      continue;
    }

    if (!strncmp(argv[i], "-l", 2)) {
      strarray_push(&opt_ld_extra_args, argv[i]);
      continue;
    }

    if (!strncmp(argv[i], "-Wl,", 4)) {
      char *s = strdup(argv[i] + 4);
      char *arg = strtok(s, ",");

      while (arg) {
        strarray_push(&opt_ld_extra_args, arg);
        arg = strtok(NULL, ",");
      }
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

    if (!strcmp(argv[i], "-M")) {
      opt_M = true;
      opt_E = true;
      continue;
    }

    if (!strcmp(argv[i], "-MD")) {
      opt_MD = true;
      continue;
    }

    if (!strcmp(argv[i], "-MM")) {
      opt_MM = true;
      opt_E = true;
      continue;
    }

    if (!strcmp(argv[i], "-MMD")) {
      opt_MMD = true;
      continue;
    }

    if (!strcmp(argv[i], "-MF")) {
      opt_MF = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-MT")) {
      opt_MT = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-MG")) {
      opt_MG = true;
      continue;
    }

    if (!strcmp(argv[i], "-MP")) {
      opt_MP = true;
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

    if (!strcmp(argv[i], "-emit-llvm")) {
      opt_emit_llvm = true;
      opt_S = true;
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
      strarray_push(&opt_ld_extra_args, argv[i]);
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
      usage(0);

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

    inputfiles_push(&opt_inputfiles,
                    new_inputfile(argv[i], get_file_type(argv[i])));
  }

  if (!opt_cc1) {
    if (opt_inputfiles.len == 0)
      error("no input file");
  }
}
