#include "plugin.h"
#include "load-resources.h"

#include <raylib/src/raylib.h>
#include <cglm/include/cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define PLAYER_TERMINAL_SPEED 5
#define PLAYER_GRAV_TERMINAL_SPEED 300.0f
#define PLAYER_SPRITE_X 0
#define PLAYER_SPRITE_Y (ATLAS_GRID_SIZE * 4)
#define PLAYER_SIZE 20
#define PLAYER_ACCELERATION 50
#define PLAYER_DECELERATION 30

#define EPS 1e-6f

#define GRAVITY -15

#define PLAYER_JUMP_SPEED (2 * GRAVITY)

static struct plug_State *plug_state = NULL;
static Texture2D tex;

void plug_init(void) {
  plug_state = malloc(sizeof(*plug_state));
  assert(plug_state != NULL && "Failed to initialize plugin state");

  memset(plug_state, 0, sizeof(*plug_state));

  load_resources(plug_state);

  plug_state->player.grounded = true;
  plug_state->player.hitbox = CLITERAL(Rectangle){
    .x = 3.0f * (PLAYER_SIZE / (float)ATLAS_GRID_SIZE),
    .y = 3.0f * (PLAYER_SIZE / (float)ATLAS_GRID_SIZE),
    .width = 10.0f * (PLAYER_SIZE / (float)ATLAS_GRID_SIZE),
    .height = 13.0f * (PLAYER_SIZE / (float)ATLAS_GRID_SIZE),
  };
  plug_state->player.camera = CLITERAL(Camera2D) {
		.offset = {
			.x = (float)GetScreenWidth() * 0.5f, 
			.y = (float)GetScreenHeight() * 0.5f,
		},

		.target = {
			.x = 0.0f, 
			.y = 0.0f,
		},

		.rotation = 0.0f,
		.zoom = 5.0f,
	};

  tex = LoadTexture("./assets/NewerCampFire_2.png");
}

void *plug_pre_reload(void) {
  unload_resources(plug_state);

  return plug_state;
}

void plug_post_reload(void *prev_state) {
  plug_state = prev_state;

  load_resources(plug_state);
}

static void draw_level(struct plug_State *state) {
  if (state->current_level < 0) {
    return;
  }

  const struct plug_Level *level =
    &DA_AT(state->levels, (uint32_t)state->current_level);
  DrawTextureV(level->grid_tex.texture, level->pos, WHITE);
}

static void level_collide(struct plug_State *state) {
}

static void update_player(struct plug_State *state) {
  if (IsKeyDown(KEY_A)) {
    plug_state->player.vel.x -= PLAYER_ACCELERATION * GetFrameTime();
  }
  if (IsKeyDown(KEY_D)) {
    plug_state->player.vel.x += PLAYER_ACCELERATION * GetFrameTime();
  }

  plug_state->player.vel.x = glm_clamp(
    plug_state->player.vel.x, -PLAYER_TERMINAL_SPEED, PLAYER_TERMINAL_SPEED);

  if (state->player.grounded && IsKeyDown(KEY_SPACE)) {
    state->player.grounded = false;
    state->player.vel.y = PLAYER_JUMP_SPEED;
  }

  // TODO: Gravity

  state->player.pos.x += state->player.vel.x;
  state->player.pos.y += state->player.vel.y;

  state->player.camera.target.x = state->player.pos.x + (PLAYER_SIZE / 2);
  state->player.camera.target.y = state->player.pos.y + (PLAYER_SIZE / 2);

  if (plug_state->player.vel.x >= 0) {
    plug_state->player.vel.x -=
      fmin(PLAYER_DECELERATION * GetFrameTime(), plug_state->player.vel.x);

  } else {
    plug_state->player.vel.x -=
      fmax(-PLAYER_DECELERATION * GetFrameTime(), plug_state->player.vel.x);
  }
}

static void draw_player(struct plug_State *state) {
  Rectangle src = {
    .x = 0,
    .y = PLAYER_SPRITE_Y,
    .width = 16,
    .height = 16,
  };

  Rectangle dest = {
    .x = state->player.pos.x,
    .y = state->player.pos.y,
    .width = PLAYER_SIZE,
    .height = PLAYER_SIZE,
  };

  DrawTexturePro(state->atlas, src, dest, CLITERAL(Vector2){ 0, 0 }, 0.0f,
                 WHITE);
}

void plug_update(void) {
  float fps = 1.0f / GetFrameTime();
  char fps_str[6] = { 0 };
  snprintf(fps_str, sizeof(fps_str), "%.2f", fps);

  update_player(plug_state);

  BeginDrawing();
  ClearBackground(GetColor(0x33c6f2ff));

  BeginMode2D(plug_state->player.camera);

  draw_level(plug_state);
  draw_player(plug_state);
  DrawCircleV(plug_state->player.camera.target, 2, BLUE);

  {
    Rectangle r = {
      .x = plug_state->player.pos.x + plug_state->player.hitbox.x,
      .y = plug_state->player.pos.y + plug_state->player.hitbox.y,
      .width = plug_state->player.hitbox.width,
      .height = plug_state->player.hitbox.height,
    };
    DrawRectangleLinesEx(r, 0.5f, BLACK);
  }

  EndMode2D();

  DrawText(fps_str, 0, 0, 20, BLACK);
  EndDrawing();
}
