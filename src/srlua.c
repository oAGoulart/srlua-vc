/*
  @brief     Lua interpreter for self-running programs
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
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#if LUA_VERSION_NUM <= 501
#define lua_load(L, r, d, n, m) (lua_load)(L, r, d, n)
#define luaL_traceback(L, M, m, l) traceback(L, m)

static void traceback(lua_State* L, const char* message)
{
  lua_getglobal(L, "debug");
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  lua_getfield(L, -1, "traceback");
  if (!lua_isfunction(L, -1)) {
    lua_pop(L, 2);
    return;
  }

  lua_pushstring(L, message);
  lua_pushinteger(L, 2);
  lua_call(L, 2, 1);
}
#endif

static const char* progname = "srlua";

static void fatal(const char* message)
{
  fprintf(stderr, "%s: %s\n", progname, message);
  exit(EXIT_FAILURE);
}

static void cannot(lua_State* L, const char* what, const char* name)
{
  char* err = "fatal";
  char buffer[96];
  if (!_strerror_s(buffer, 96, NULL))
    err = buffer;

  const char* why = (!memcmp(what, "find", 4)) ? "no glue" : err;
  const char* message = lua_pushfstring(L, "cannot %s %s: %s", what, name, why);
  fatal(message);
}

typedef struct state_t {
  FILE* f;
  size_t size;
  char buff[BUFSIZ];
} State;

static const char* myget(lua_State* L, void* data, size_t* size)
{
  State* s = data;
  (void)L;
  size_t n = (sizeof(s->buff) <= s->size) ? sizeof(s->buff) : s->size;
  n = fread(s->buff, 1, n, s->f);
  s->size -= n;
  *size = n;
  return (n>0) ? s->buff : NULL;
}

static void load(lua_State* L, const char* name)
{
  FILE* f;
  if (fopen_s(&f, name, "rb"))
    cannot(L, "open", name);

  Glue t;
  if (fseek(f, 0 - sizeof(t), SEEK_END) != 0)
    cannot(L, "seek", name);
  if (fread(&t, sizeof(t), 1, f) != 1)
    cannot(L, "read", name);
  if (memcmp(t.sig, GLUESIG,GLUELEN) != 0)
    cannot(L, "find a Lua program in", name);
  if (fseek(f, t.size1, SEEK_SET) != 0)
    cannot(L, "seek", name);

  State S = { f, t.size2 };

  int c = getc(f);
  if (c == '#') {
    while (--S.size > 0 && c != '\n')
      c = getc(f);
  }
  ungetc(c, f);

  if (lua_load(L, myget, &S, "=", NULL) != 0)
    fatal(lua_tostring(L, -1));
  fclose(f);
}

static int pmain(lua_State* L)
{
  int argc = (int)lua_tointeger(L, 1);
  char** argv = lua_touserdata(L, 2);

  luaL_openlibs(L);
  load(L, argv[0]);
  lua_createtable(L, argc, 0);

  int i;
  for (i = 0; i < argc; ++i) {
    lua_pushstring(L, argv[i]);
    lua_rawseti(L, -2, i);
  }

  lua_setglobal(L, "arg");
  luaL_checkstack(L, argc - 1, "too many arguments to script");
  for (i = 1; i < argc; ++i)
    lua_pushstring(L, argv[i]);

  lua_call(L, argc - 1, 0);
  return 0;
}

static int msghandler(lua_State* L)
{
  const char* message = lua_tostring(L, 1);
  if (message == NULL) {
    if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
      return 1;
    message = lua_pushfstring(L, "(error object is a %s value)", luaL_typename(L, 1));
  }
  luaL_traceback(L, L, message, 1);
  return 1;
}

int main(int argc, char* argv[])
{
  if (argv[0] == NULL)
    fatal("cannot locate this executable");

  progname = argv[0];
  lua_State* L = luaL_newstate();
  if (L == NULL)
    fatal("cannot create state: not enough memory");

  lua_pushcfunction(L, msghandler);
  lua_pushcfunction(L, &pmain);
  lua_pushinteger(L, argc);
  lua_pushlightuserdata(L, argv);
  if (lua_pcall(L, 2, 0, 1) != 0)
    fatal(lua_tostring(L, -1));

  lua_close(L);
  return EXIT_SUCCESS;
}
