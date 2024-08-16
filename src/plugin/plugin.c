#include "plugin.h"
#include <raylib/src/raylib.h>

#ifndef PLATFORM_WEB
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#endif

static struct PluginState *plug_state = NULL;

void plug_init(void) {
#ifndef PLATFORM_WEB

  plug_state = malloc(sizeof(*plug_state));
  assert(plug_state != NULL && "Failed to initialize plugin state");

  memset(plug_state, 0, sizeof(*plug_state));

#endif
}

void *plug_pre_reload(void) {
  return plug_state;
}

void plug_post_reload(void *prev_state) {
  plug_state = prev_state;
}

void plug_update(void) {
  BeginDrawing();
  ClearBackground(GetColor(0x181818ff));
  EndDrawing();
}
