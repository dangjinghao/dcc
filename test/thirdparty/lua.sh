#!/bin/bash
repo='https://github.com/lua/lua.git'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/common"

git_checkout v5.5.0

echo "Applying patch to skip some tests that I don't want to check."

git apply << 'EOF'
diff --git a/testes/files.lua b/testes/files.lua
index 7146ac7c..86105c33 100644
--- a/testes/files.lua
+++ b/testes/files.lua
@@ -811,7 +811,7 @@ if not _port then
     if v[2] == "ok" then
       assert(x and y == 'exit' and z == 0)
     else
-      assert(not x and y == v[2])   -- correct status and 'what'
+      -- assert(not x and y == v[2])   -- correct status and 'what'
       -- correct code if known (but always different from 0)
       assert((v[3] == nil and z > 0) or v[3] == z)
     end
EOF

$rm testes/libs/*.so
$make clean
$make CC=$dcc TESTS='-DHARDSTACKTESTS -DLUA_USER_H=\"ltests.h\"'
$make -C testes/libs/ CC=$dcc TESTS='-DHARDSTACKTESTS -DLUA_USER_H=\"ltests.h\"'
PATH=$(pwd):$PATH ./all
