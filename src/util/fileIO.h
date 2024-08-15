#pragma once
#ifndef UTIL_FILE_IO_H
#define UTIL_FILE_IO_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdint.h>

// Compatible with Windows
char *fio_read_file(const char *path);

#define UTIL_FILE_IO_IMPLEMENTATION
#ifdef UTIL_FILE_IO_IMPLEMENTATION

char *fio_read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    fprintf(stderr, "[ERROR]: Failed to open file %s: ", path);
    perror(NULL);

    return NULL;
  }

  struct stat s;
  fstat(fileno(f), &s);

  uint64_t size = s.st_size;
  char *buf = malloc(size + 1);

  fread(buf, 1, size, f);
  buf[size] = 0;

  fclose(f);

  return buf;
}

#endif // UTIL_FILE_IO_IMPLEMENTATION

#endif // UTIL_FILE_IO_H
