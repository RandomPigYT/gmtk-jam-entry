#include "plugin.h"
#include "load-resources.h"
#include "update-player.h"

#include <raylib/src/raylib.h>
#include <cglm/include/cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

static struct plug_State *plug_state = NULL;
static Texture2D tex;

void plug_init(void) {
  plug_state = malloc(sizeof(*plug_state));
  assert(plug_state != NULL && "Failed to initialize plugin state");

  memset(plug_state, 0, sizeof(*plug_state));

  load_resources(plug_state);

  plug_state->player.grounded = false;
  plug_state->player.pos.y = 40;
  plug_state->player.cam_move_pad_x = 50;
  plug_state->player.cam_move_pad_y = 50;
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
  if (IsWindowResized()) {
    plug_state->player.camera.offset.x = (float)GetScreenWidth() * 0.5f;
    plug_state->player.camera.offset.y = (float)GetScreenHeight() * 0.5f;
  }

  float fps = 1.0f / GetFrameTime();
  char fps_str[6] = { 0 };
  snprintf(fps_str, sizeof(fps_str), "%.2f", fps);

  //printf("%f\n", plug_state->player.vel.y);

  BeginDrawing();
  update_player(plug_state);
  ClearBackground(GetColor(0x33c6f2ff));

  BeginMode2D(plug_state->player.camera);

  draw_level(plug_state);
  draw_player(plug_state);

  //{
  //  Rectangle r = {
  //    .x = plug_state->player.pos.x + plug_state->player.hitbox.x,
  //    .y = plug_state->player.pos.y + plug_state->player.hitbox.y,
  //    .width = plug_state->player.hitbox.width,
  //    .height = plug_state->player.hitbox.height,
  //  };
  //  DrawRectangleLinesEx(r, 0.5f, RED);
  //}
  //DrawCircleV((Vector2){ 0, 0 }, 1, BLUE);

  EndMode2D();

  DrawText(fps_str, 0, 0, 20, BLACK);
  EndDrawing();
}
