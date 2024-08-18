#include "plugin.h"
#include "level.h"
#include "util/dynamic_array.h"

#include <stdlib.h>

// clang-format off

 static const enum plug_CellType defualt_grid[DEFAULT_LEVEL_WIDTH * DEFAULT_LEVEL_HEIGHT] = {
 	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
 	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
 	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 
 	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 
 	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 
 	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 
 };

//static const enum plug_CellType defualt_grid[DEFAULT_LEVEL_WIDTH * DEFAULT_LEVEL_HEIGHT] = {
//	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//	0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
//};

// clang-format on

void import_level(const char *level_path, struct plug_State *state) {
  (void)level_path;

  // Load default level for now
  struct plug_Level level = {
    .grid_width = DEFAULT_LEVEL_WIDTH,
    .grid_height = DEFAULT_LEVEL_HEIGHT,

    .cell_size = DEFAULT_LEVEL_CELL_SIZE,

    .pos = { 0 },

    .grid = (enum plug_CellType *)defualt_grid,
    .grid_tex = { 0 },

    .loaded = false,
  };

  DA_APPEND(&state->levels, level);
}

void unload_levels(struct plug_State *state) {
  if (state->current_level >= 0) {
    UnloadRenderTexture(
      DA_AT(state->levels, (uint32_t)state->current_level).grid_tex);

    DA_AT(state->levels, (uint32_t)state->current_level).loaded = false;
    state->current_level = -1;
  }

  DA_FREE(&state->levels);
}

void load_level(struct plug_State *state) {
  if (state->current_level < 0) {
    return;
  }

  struct plug_Level *level =
    &DA_AT(state->levels, (uint32_t)state->current_level);
  if (level->loaded) {
    return;
  }

  level->grid_tex = LoadRenderTexture(level->grid_width * level->cell_size,
                                      level->grid_height * level->cell_size);

  BeginTextureMode(level->grid_tex);

  const int32_t atlas_grid_tex_off = -16;

  for (uint32_t y = 0; y < level->grid_height; y++) {
    for (uint32_t x = 0; x < level->grid_width; x++) {
      enum plug_CellType cell = level->grid[y * level->grid_width + x];

      if (cell == 0) {
        continue;
      }

      uint32_t atlas_index = (cell * ATLAS_GRID_SIZE) + atlas_grid_tex_off;

      uint32_t atlas_x = atlas_index % state->atlas.width;
      uint32_t atlas_y = atlas_index / state->atlas.width;
      Rectangle src = {
        .x = atlas_x,
        .y = atlas_y,
        .width = 16,
        .height = 16,
      };

      Rectangle dest = {
        .x = x * level->cell_size,
        .y = (level->grid_height - y - 1) * level->cell_size,
        .width = level->cell_size,
        .height = level->cell_size,
      };

      DrawTexturePro(state->atlas, src, dest, CLITERAL(Vector2){ 0.0f, 0.0f },
                     0.0f, WHITE);
    }
  }

  EndTextureMode();
  level->loaded = true;
}

void unload_level(struct plug_State *state) {
  if (state->current_level < 0) {
    return;
  }

  if (DA_AT(state->levels, (uint32_t)state->current_level).loaded) {
    return;
  }

  UnloadRenderTexture(
    DA_AT(state->levels, (uint32_t)state->current_level).grid_tex);
}
