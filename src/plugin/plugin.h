#ifndef PLUGIN_H
#define PLUGIN_H

#include "util/dynamic_array.h"

#include <raylib/src/raylib.h>
#include <stdint.h>

#define ATLAS_GRID_SIZE 16
#define EPS 1e-6f

#define PLAYER_TERMINAL_SPEED 100
#define PLAYER_GRAV_TERMINAL_SPEED 500
#define PLAYER_SPRITE_X 0
#define PLAYER_SPRITE_Y (ATLAS_GRID_SIZE * 4)
#define PLAYER_SIZE 20
#define PLAYER_ACCELERATION 600
#define PLAYER_DECELERATION 300

#define EPS 1e-6f

#define PLAYER_FEET_HITBOX_HEIGHT EPS
#define PLAYER_FEET_HITBOX_WIDTH_FACT 0.8f

#define GRAVITY 350

#define PLAYER_JUMP_SPEED -150

enum plug_PlayerState {
  PLAYER_STATE_NORMAL = 0,
  PLAYER_STATE_BIG,
  PLAYER_STATE_SMOL,

  PLAYER_STATE_COUNT,
};

struct plug_Player {
  float walk_acceleration;
  float walk_deceleration;
  float jump_speed;

  enum plug_PlayerState state;

  Vector2 pos;
  Vector2 vel;

  Vector2 respawn_point;

  Rectangle hitbox; // Relative

  Camera2D camera;

  float cam_move_pad_x;
  float cam_move_pad_y;

  bool grounded;
};

enum plug_CellType {
  CELL_TYPE_NONE = 0,
  CELL_TYPE_FLOOR,
  CELL_TYPE_SPIKES,
  CELL_TYPE_SPIKE_FLOOR,
  CELL_TYPE_VANISH,
  CELL_TYPE_SHRINK_PLAYER,
  CELL_TYPE_EXPAND_PLAYER,
  CELL_TYPE_CHECKPOINT,
  CELL_TYPE_FINISH,

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
