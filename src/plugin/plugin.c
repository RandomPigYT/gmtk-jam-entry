#include "plugin.h"
#include "load-resources.h"

#include <raylib/src/raylib.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define PLAYER_SPEED 100.0f

static struct plug_State *plug_state = NULL;
static Texture2D tex;

void plug_init(void) {
  plug_state = malloc(sizeof(*plug_state));
  assert(plug_state != NULL && "Failed to initialize plugin state");

  memset(plug_state, 0, sizeof(*plug_state));

  load_resources(plug_state);

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

void plug_update(void) {
  if (IsKeyDown(KEY_W)) {
    plug_state->player.camera.target.y -= PLAYER_SPEED * GetFrameTime();
  }
  if (IsKeyDown(KEY_A)) {
    plug_state->player.camera.target.x -= PLAYER_SPEED * GetFrameTime();
  }
  if (IsKeyDown(KEY_S)) {
    plug_state->player.camera.target.y += PLAYER_SPEED * GetFrameTime();
  }
  if (IsKeyDown(KEY_D)) {
    plug_state->player.camera.target.x += PLAYER_SPEED * GetFrameTime();
  }

  float fps = 1.0f / GetFrameTime();
  char fps_str[6] = { 0 };
  snprintf(fps_str, 6, "%.2f", fps);

  BeginDrawing();
  ClearBackground(GetColor(0x33c6f2ff));

  BeginMode2D(plug_state->player.camera);
  //DrawTexture(plug_state->atlas, 0, 0, WHITE);
  draw_level(plug_state);

  EndMode2D();

  DrawText(fps_str, 0, 0, 20, BLACK);
  EndDrawing();
}
