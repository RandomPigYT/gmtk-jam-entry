#include "plugin.h"
#include "load-resources.h"
#include "level.h"

#include "util/dynamic_array.h"

#include <raylib/src/raylib.h>

#include <stdlib.h>

#define ATLAS_PATH "./assets/gmtk-texture-atlas.png"

void load_resources(struct plug_State *state) {
  state->atlas = LoadTexture(ATLAS_PATH);

  import_level(NULL, state);
  state->current_level = 0;
  load_level(state);
}

void unload_resources(struct plug_State *state) {
  unload_levels(state);
  UnloadTexture(state->atlas);
}
