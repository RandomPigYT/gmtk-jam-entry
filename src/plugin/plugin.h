#ifndef PLUGIN_H
#define PLUGIN_H

#include "util/dynamic_array.h"

#include <raylib/src/raylib.h>
#include <stdint.h>

#define ATLAS_GRID_SIZE 16

struct plug_Player {
  Vector2 pos;
  Vector2 vel;

  Rectangle hitbox; // Relative

  Camera2D camera;

  float cam_move_pad_x;
  float cam_move_pad_y;

  bool grounded;
};

enum plug_CellType {
  CELL_TYPE_NONE = 0,
  CELL_TYPE_FLOOR,

  CELL_TYPES_COUNT,
};

struct plug_Level {
  uint32_t grid_width, grid_height;

  uint32_t cell_size;
  Vector2 pos;

  enum plug_CellType *grid;
  RenderTexture2D grid_tex;

  bool loaded;
};

struct plug_State {
  struct plug_Player player;

  DA_TYPE(struct plug_Level) levels;
  int32_t current_level;

  Texture2D atlas;
};

void plug_init(void);
void *plug_pre_reload(void);
void plug_post_reload(void *prev_state);
void plug_update(void);

#endif // PLUGIN_H
