#!/bin/bash
repo='https://github.com/c-testsuite/c-testsuite.git'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/common"

git_checkout 5c7275656d751de0e68b2d340a95b5681858ed07

echo "Adding -lm to the runner command line"

git apply << 'EOF'
diff --git a/single-exec b/single-exec
index e6952d9..83ea95d 100755
--- a/single-exec
+++ b/single-exec
@@ -56,7 +56,7 @@ do
         continue
     fi
 
-    if ! timeout 5m $runner $t > $scratchdir/t.out 2>&1
+    if ! timeout 5m $runner -lm $t > $scratchdir/t.out 2>&1
     then
         result="not ok"
     fi
EOF

ln -s $dcc runners/single-exec/dcc

./single-exec dcc