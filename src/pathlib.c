

#include "dcc.h"
#include <errno.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static PtrArray tmp_files;

static void path_tmp_cleanup() {
  for (int i = 0; i < tmp_files.len; i++)
    unlink(tmp_files.data[i]);
}

char *path_new_tmpfile() {
  if (tmp_files.len == 0) {
    atexit(path_tmp_cleanup);
  }
  char *path = strdup("/tmp/dcc-XXXXXX");
  int fd = mkstemp(path);
  if (fd == -1)
    error("mkstemp failed: %s", strerror(errno));
  close(fd);

  strarray_push(&tmp_files, path);
  return path;
}

void path_fcp(FILE *dst, FILE *src) {
  char buf[8192];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
    if (fwrite(buf, 1, n, dst) != n) {
      error("fwrite: %s", strerror(errno));
    }
  }
  fflush(dst);
}

void path_cp(const char *dst, const char *src) {
  FILE *fsrc = NULL;
  FILE *fdst = NULL;

  if (!strcmp(src, "-")) {
    fsrc = stdin;
  } else {
    fsrc = fopen(src, "rb");
  }
  if (!fsrc) {
    error("fopen %s: %s", src, strerror(errno));
  }
  if (!strcmp(dst, "-")) {
    fdst = stdout;
  } else {
    fdst = fopen(dst, "wb");
  }
  if (!fdst) {
    error("fopen %s: %s", dst, strerror(errno));
  }
  path_fcp(fdst, fsrc);
  if (fdst != stdout)
    fclose(fdst);
  if (fsrc != stdin)
    fclose(fsrc);
}

char *path_new_replaced_suffix(const char *path, const char *suffix) {
  if (!path || !suffix)
    return NULL;

  const char *last_dot = strrchr(path, '.');
  size_t base_len;
  if (last_dot) {
    base_len = last_dot - path;
  } else {
    base_len = strlen(path);
  }
  size_t suffix_len = strlen(suffix);

  size_t new_len = base_len + suffix_len + 1;
  char *new_path = (char *)malloc(new_len);
  if (!new_path)
    return NULL;

  if (base_len > 0) {
    strncpy(new_path, path, base_len);
  }

  strcpy(new_path + base_len, suffix);
  return new_path;
}

char *path_find_file(const char *pattern) {
  char *path = NULL;
  glob_t buf = {};
  glob(pattern, 0, NULL, &buf);
  if (buf.gl_pathc > 0)
    path = strdup(buf.gl_pathv[buf.gl_pathc - 1]);
  globfree(&buf);
  return path;
}

bool path_exists(const char *path) {
  struct stat st;
  return !stat(path, &st);
}

char *path_get_exedir() {
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n < 0)
    error("readlink /proc/self/exe: %s", strerror(errno));
  buf[n] = '\0';
  return dirname(strdup(buf));
}
