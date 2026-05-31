#!/bin/bash
repo='https://github.com/git/git.git'

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/common"

git checkout .
git checkout 54e85e7af1ac9e9a92888060d6811ae767fea1bc

git apply << EOF
diff --git a/http.h b/http.h
index 5de792ef3f..02a4198b0a 100644
--- a/http.h
+++ b/http.h
@@ -51,7 +51,7 @@
 */
 #if !defined(CURLOPT_USE_SSL) && defined(CURLOPT_FTP_SSL)
 #define CURLOPT_USE_SSL CURLOPT_FTP_SSL
-#define CURLUSESSL_TRY CURLFTPSSL_TRY
+//#define CURLUSESSL_TRY CURLFTPSSL_TRY
 #endif
 
 struct slot_results {

EOF

$make clean
$make V=1 CC=$dcc test

# In my platform, the test suite fails with(ignore known breakages):
#
# not ok 35 - basic atom: head objectsize:disk
# not ok 90 - basic atom: tag objectsize:disk
# not ok 91 - basic atom: tag *objectsize:disk
#
# but it looks like this is a problem of the platform(zlib-ng) rather than the test suite, so just ignore it.