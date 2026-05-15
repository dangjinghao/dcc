#include "dcc.h"
#include <libgen.h>
#include <string.h>

// bool opt_E;              // expand macro
// bool opt_S;              // generate *.s
// bool opt_c;              // generate *.o
bool opt_hash_hash_hash; // dump the subprocess's command line
bool opt_ir;             // generate llvm ir file *.ll
char *opt_o;
StringArray opt_input_paths;

bool opt_cc1; // run in cc1 mode
char *opt_cc1_input;
char *opt_cc1_output;

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

    if (!strcmp(argv[i], "-###")) {
      opt_hash_hash_hash = true;
      continue;
    }

    if (!strcmp(argv[i], "-ir")) {
      opt_ir = true;
      continue;
    }

    if (argv[i][0] == '-' && argv[i][1] != '\0')
      error("unknown argument: %s", argv[i]);

    strarray_push(&opt_input_paths, argv[i]);
  }
  if (!opt_cc1) {
    if (opt_input_paths.len == 0)
      error("no input file");
  }
}
