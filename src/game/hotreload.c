#ifndef PLATFORM_WEB
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#endif

#include "hotreload.h"
#include "plugin-interface.h"

#ifdef linux

#include <dlfcn.h>
#define PLUG_PATH "bin/libplug.so"

#elif (defined(_WIN32) || defined(WIN32))

#include <windows.h>
#define PLUG_PATH "bin/libplug.dll"

#define dlopen(filename, flags) (void *)LoadLibrary(filename)
#define dlclose(handle) !FreeLibrary(handle)
#define dlerror() "Failed to load DLL. Path: " PLUG_PATH
#define dlsym(handle, symbol) (void *)GetProcAddress(handle, symbol)

#endif

#ifdef HOT_RELOAD

#define X(name, ret, ...) ret (*name)(__VA_ARGS__) = nullptr;
PLUGIN_FUNCTIONS();

#endif

static void *plug = nullptr;

void hotreload_load_plug(void) {
#ifdef HOT_RELOAD
  if (plug) {
    if (dlclose(plug) != 0) {
      fprintf(stderr, "%s\n", dlerror());
      exit(EXIT_FAILURE);
    }
  }

  plug = dlopen(PLUG_PATH, RTLD_NOW);
  if (!plug) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  plug_init = dlsym(plug, "plug_init");
  if (!plug_init) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  plug_pre_reload = dlsym(plug, "plug_pre_reload");
  if (!plug_pre_reload) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  plug_post_reload = dlsym(plug, "plug_post_reload");
  if (!plug_post_reload) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

  plug_update = dlsym(plug, "plug_update");
  if (!plug_update) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }

#endif
}

void hotreload_cleanup(void) {
#ifndef PLATFORM_WEB
  assert(plug != nullptr);
  if (dlclose(plug) != 0) {
    fprintf(stderr, "%s\n", dlerror());
    exit(EXIT_FAILURE);
  }
#endif
}
