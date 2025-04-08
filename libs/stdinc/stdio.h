#ifndef __STDIO_H
#define __STDIO_H
#define NULL ((void *)0)
#define EOF (-1)

typedef unsigned long size_t;
typedef long ssize_t;
typedef void FILE;
int printf(const char *fmt, ...);
int sprintf(char *, const char *, ...);
FILE *fopen(const char *filename, const char *mode);
size_t fread(void *ptr, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t count, FILE *stream);
int fclose(FILE *stream);
int fgetc(FILE *stream);
char *fgets(char *str, int n, FILE *stream);
int getc(FILE *stream);
#endif
