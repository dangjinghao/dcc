
define rcc1
  run -cc1 -cc1-input $arg0 -cc1-output /tmp/dcc_$(basename $arg0).o -cc1-filename $arg0
end
document rcc1
  Run dcc with -cc1 (internal codegen path).
  Usage: rcc1 <source_file>
  Output: /tmp/<basename>.out
end
