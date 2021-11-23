/**
  @brief     glue exe and script for srlua
  @date      23.11.2021
  @copyright   Copyright 2019 Luiz Henrique de Figueiredo
               Copyright 2021 Augusto Goulart
               Permission is hereby granted, free of charge, to any person obtaining a copy
               of this software and associated documentation files (the "Software"), to deal
               in the Software without restriction, including without limitation the rights
               to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
               copies of the Software, and to permit persons to whom the Software is
               furnished to do so, subject to the following conditions:
               The above copyright notice and this permission notice shall be included in all
               copies or substantial portions of the Software.
               THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
               IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
               FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
               AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
               LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
               OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
               SOFTWARE.
**/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "srglue.h"

static const char* progname = "srglue";

static void cannot(const char* what, const char* name)
{
  char buffer[96];
  if (!_strerror_s(buffer, 96, NULL))
    fprintf(stderr, "%s: cannot %s %s: %s\n", progname, what, name, buffer);
  exit(EXIT_FAILURE);
}

static FILE* open(const char* name, const char* mode, const char* outname)
{
  if (outname != NULL && !strcmp(name, outname)) {
    errno = EPERM;
    cannot("overwrite input file", name);
  }
  else {
    FILE* f;
    if (fopen_s(&f, name, mode))
      cannot("open file", name);
    return f;
  }
  return NULL;
}

static long copy(FILE* in, const char* name, FILE* out, const char* outname)
{
  if (fseek(in, 0, SEEK_END) != 0)
    cannot("seek", name);

  long size = ftell(in);
  if (fseek(in, 0, SEEK_SET) != 0)
    cannot("seek", name);

  while (1) {
    char b[BUFSIZ];
    size_t n = fread(&b, 1, sizeof(b), in);
    if (!n) {
      if (ferror(in))
        cannot("read", name);
      else
        break;
    }
    if (fwrite(&b, n, 1, out) != 1)
      cannot("write", outname);
  }

  fclose(in);
  return size;
}

int main(int argc, char* argv[])
{
  if (argv[0] != NULL && *argv[0] != 0)
    progname = argv[0];
  if (argc != 4) {
    fprintf(stderr, "usage: %s in.exe in.lua out.exe\n", progname);
    return 1;
  }
  else {
    FILE* in1 = open(argv[1], "rb", argv[3]);
    FILE* in2 = open(argv[2], "rb", argv[3]);
    FILE* out = open(argv[3], "wb", NULL);

    Glue t = { GLUESIG, 0, 0 };
    t.size1 = copy(in1, argv[1], out, argv[3]);
    t.size2 = copy(in2, argv[2], out, argv[3]);

    if (fwrite(&t, sizeof(t), 1, out) != 1)
      cannot("write", argv[3]);
    if (fclose(out) != 0)
      cannot("close", argv[3]);
  }
  return 0;
}
