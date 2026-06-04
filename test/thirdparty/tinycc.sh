#!/bin/bash
repo='https://github.com/TinyCC/tinycc.git'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/common"

git_checkout df67d8617b7d1d03a480a28f9f901848ffbfb7ec

git apply << 'EOF'
diff --git a/tccgen.c b/tccgen.c
index 18fbd6b8..6c9b0e49 100644
--- a/tccgen.c
+++ b/tccgen.c
@@ -7217,7 +7217,7 @@ static void init_putv(CType *type, Section *sec, unsigned long c)
 #if defined TCC_IS_NATIVE_387
                 if (sizeof (long double) >= 10) /* zero pad ten-byte LD */
                     memcpy(ptr, &vtop->c.ld, 10);
-#ifdef __TINYC__
+#if defined(__TINYC__) && (!defined(__dcc__))
                 else if (sizeof (long double) == sizeof (double))
                     __asm__("fldl %1\nfstpt %0\n" : "=m" (*ptr) : "m" (vtop->c.ld));
 #endif
EOF

./configure --cc=$dcc
$make clean
$make
# CFLAGS="-fpermissive -fno-strict-aliasing" is needed to compile the test suite in gcc 16
make CC=cc CFLAGS="-fpermissive -fno-strict-aliasing -I.." test
