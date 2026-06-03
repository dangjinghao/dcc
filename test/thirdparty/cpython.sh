#!/bin/bash
repo='https://github.com/python/cpython.git'

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
. "$SCRIPT_DIR/common"

git_checkout c75330605d4795850ec74fdc4d69aa5d92f76c00

CC=$dcc ./configure --without-ensurepip

$make clean
$make

# Exclude network-dependent tests
$make test \
  TESTOPTS="-x test_socket test_ssl test_httplib test_ftplib \
  test_poplib test_smtplib test_smtpnet test_urllib test_urllib2 \
  test_urllib2net test_urllib2_localnet test_urllibnet test_urllib_response \
  test_asynchat test_asyncore test_socketserver \
  test_telnetlib test_imaplib test_nntplib test_xmlrpc_net \
  test_http_cookiejar test_http_cookies test_httpservers \
  test_docxmlrpc test_robotparser test_timeout"
