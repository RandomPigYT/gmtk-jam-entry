#include "plugin.h"
#include "load-resources.h"

#include <raylib/src/raylib.h>
#include <cglm/include/cglm/cglm.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define PLAYER_TERMINAL_SPEED 2
#define PLAYER_GRAV_TERMINAL_SPEED 75
#define PLAYER_SPRITE_X 0
#define PLAYER_SPRITE_Y (ATLAS_GRID_SIZE * 4)
#define PLAYER_SIZE 20
#define PLAYER_ACCELERATION 50
#define PLAYER_DECELERATION 30

#define EPS 1e-6f

#define PLAYER_FEET_HITBOX_HEIGHT EPS
#define PLAYER_FEET_HITBOX_WIDTH_FACT 0.8f

#define GRAVITY 5

#define PLAYER_JUMP_SPEED -3

static struct plug_State *plug_state = NULL;
static Texture2D tex;

void plug_init(void) {
  plug_state = malloc(sizeof(*plug_state));
  assert(plug_state != NULL && "Failed to initialize plugin state");

  memset(plug_state, 0, sizeof(*plug_state));

  load_resources(plug_state);

  plug_state->player.grounded = false;
  plug_state->player.pos.y = 40;
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

static bool point_inside_aabb(Rectangle aabb, Vector2 p) {
  if (p.x < aabb.x) {
    return false;
  }

  if (p.x > aabb.x + aabb.width) {
    return false;
  }

  if (p.y < aabb.y) {
    return false;
  }

  if (p.y > aabb.y + aabb.height) {
    return false;
  }

  return true;
}

static bool aabb_collision(vec2 aabb1[2], vec2 aabb2[2]) {
  return !(aabb1[0][0] > aabb2[1][0] || aabb1[1][0] < aabb2[0][0] ||
           aabb1[0][1] > aabb2[1][1] || aabb1[1][1] < aabb2[0][1]);
}

static inline bool is_zero(float n, float eps) {
  return n < eps && n > -eps;
}

static bool intersect_line_segments(const vec2 line1[2], const vec2 line2[2],
                                    vec2 intersection, float *t) {
  vec2 p = { line1[0][0], line1[0][1] };
  vec2 q = { line2[0][0], line2[0][1] };

  vec2 r = {
    line1[1][0] - line1[0][0],
    line1[1][1] - line1[0][1],
  };

  vec2 s = {
    line2[1][0] - line2[0][0],
    line2[1][1] - line2[0][1],
  };

  float r_cross_s = glm_vec2_cross(r, s);
  if (is_zero(r_cross_s, EPS)) {
    intersection[0] = INFINITY;
    intersection[1] = INFINITY;

    return false;
  }

  vec2 q_sub_p;
  glm_vec2_sub(q, p, q_sub_p);

  float q_sub_p_cross_s = glm_vec2_cross(q_sub_p, s);
  float q_sub_p_cross_r = glm_vec2_cross(q_sub_p, r);

  *t = q_sub_p_cross_s / r_cross_s;
  float u = q_sub_p_cross_r / r_cross_s;

  if (!(fabs(*t) >= 0.0f && fabs(*t) <= 1.0f && fabs(u) >= 0.0f &&
        fabs(u) <= 1.0f)) {
    intersection[0] = INFINITY;
    intersection[1] = INFINITY;

    return false;
  }

  intersection[0] = p[0] + r[0] * (*t);
  intersection[1] = p[1] + r[1] * (*t);

  return true;
}

static float resolve_collision(vec2 player_aabb[2], vec2 grid_aabb[2],
                               vec2 vel) {
  //if (is_zero(vel[0], EPS) && is_zero(vel[1], EPS)) {
  //  return 0;
  //}

  vec2 minkowski_aabb[2] = {
    {
      grid_aabb[0][0] - player_aabb[1][0],
      grid_aabb[0][1] - player_aabb[1][1],
    },

    {
      grid_aabb[1][0] - player_aabb[0][0],
      grid_aabb[1][1] - player_aabb[0][1],
    },
  };

  Rectangle m = {
    .x = minkowski_aabb[0][0],
    .y = minkowski_aabb[0][1],
    .width = minkowski_aabb[1][0] - minkowski_aabb[0][0],
    .height = minkowski_aabb[1][1] - minkowski_aabb[0][1],
  };

  vec2 raydir;
  glm_vec2_copy(vel, raydir);

  float tx_1;
  float tx_2;
  float ty_1;
  float ty_2;

  tx_1 = minkowski_aabb[0][0] / raydir[0];
  tx_2 = minkowski_aabb[1][0] / raydir[0];

  ty_1 = minkowski_aabb[0][1] / raydir[1];
  ty_2 = minkowski_aabb[1][1] / raydir[1];

  float tx_min = fmin(tx_1, tx_2);
  float tx_max = fmax(tx_1, tx_2);

  float ty_min = fmin(ty_1, ty_2);
  float ty_max = fmax(ty_1, ty_2);

  float t_min = fmax(tx_min, ty_min);
  float t_max = fmin(tx_max, ty_max);

  if (t_max < 0.0f) {
    return 1.0f;
  }
  if (t_min > t_max) {
    return 1.0f;
  }

  if (tx_min < 0.0f && ty_min < 0.0f) {
    return 1.0f;
  }

  if (tx_min > 1.0f || ty_min > 1.0f) {
    return 1.0f;
  }

  //printf("%f\n", t_min);

  //vec2 point;
  //glm_vec2_scale(raydir, t_min, point);

  //DrawRectangleRec(m, ORANGE);
  //DrawCircleV((Vector2){ point[0], point[1] }, 1, GRAY);
  //if (minkowski_aabb[0][0] <= 0 && minkowski_aabb[1][0] >= 0 &&
  //    minkowski_aabb[0][1] <= 0 && minkowski_aabb[1][1] >= 0) {
  //  return true;
  //} else {
  //  return false;
  //}

  return t_min;
}

static void level_collide(struct plug_State *state, bool *is_grounded) {
  if (state->current_level < 0) {
    return;
  }

  struct plug_Level *level =
    &DA_AT(state->levels, (uint32_t)state->current_level);
  struct plug_Player *player = &state->player;

  bool did_feet_collide = false;
  vec2 res_vector = { 0 };

  float min_t = 1.0f;

  for (uint32_t y = 0; y < level->grid_height; y++) {
    for (uint32_t x = 0; x < level->grid_width; x++) {
      uint32_t grid_index = y * level->grid_width + x;

      if (level->grid[grid_index] == 0) {
        continue;
      }

      Rectangle cell_rect = {
        .x = level->pos.x + (x * level->cell_size),
        .y = level->pos.y + (y * level->cell_size),
        .width = level->cell_size,
        .height = level->cell_size,
      };

      vec2 grid_aabb[2] = {
        {
          cell_rect.x,
          cell_rect.y,
        },
        {
          cell_rect.x + cell_rect.width,
          cell_rect.y + cell_rect.height,
        },
      };

      Rectangle *hitbox = &player->hitbox;
      Rectangle player_rect = {
        .x = player->pos.x + hitbox->x,
        .y = player->pos.y + hitbox->y,

        .width = hitbox->width,
        .height = hitbox->height,
      };

      vec2 player_aabb[2] = {
        {
          player->pos.x + hitbox->x,
          player->pos.y + hitbox->y,
        },
        {
          player->pos.x + hitbox->x + hitbox->width,
          player->pos.y + hitbox->y + hitbox->height,
        },
      };

      float t = resolve_collision(player_aabb, grid_aabb,
                                  (vec2){ player->vel.x, player->vel.y });

      min_t = t < min_t ? t : min_t;

      //if (!is_zero(res_vector[0], EPS) && !is_zero(res_vector[1], EPS)) {
      //  player->vel.x = 0;
      //  player->vel.y = 0;
      //}

      vec2 feet_hitbxox[2] = {
        {
          player_rect.x +
            ((1.0f - PLAYER_FEET_HITBOX_WIDTH_FACT) / 2.0f) * player_rect.width,
          player_rect.y + player_rect.height,
        },

        {
          player_rect.x + player_rect.width -
            ((1.0f - PLAYER_FEET_HITBOX_WIDTH_FACT) / 2.0f) * player_rect.width,
          player_rect.y + player_rect.height + PLAYER_FEET_HITBOX_WIDTH_FACT,
        },
      };

      Rectangle feet_rect = {
        .x = player_rect.x,
        .y = player_rect.y + player_rect.height,

        .width = player_rect.width,
        .height = PLAYER_FEET_HITBOX_WIDTH_FACT,
      };

      bool feet_inter = aabb_collision(feet_hitbxox, grid_aabb);
      //printf("%d\n", feet_inter);
      if (feet_inter) {
        *is_grounded = true;
        player->vel.y = 0;

        did_feet_collide = true;

      } else if (!feet_inter && !did_feet_collide) {
        *is_grounded = false;
      }
    }
  }

  player->pos.x += min_t * player->vel.x;
  if (!*is_grounded) {
    player->pos.y += min_t * player->vel.y;
  }
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

  //if (IsKeyDown(KEY_SPACE)) {
  //  state->player.pos.y -= PLAYER_ACCELERATION * GetFrameTime();
  //  state->player.vel.y = -PLAYER_ACCELERATION;
  //}
  //if (IsKeyDown(KEY_LEFT_SHIFT)) {
  //  state->player.pos.y += PLAYER_ACCELERATION * GetFrameTime();
  //  state->player.vel.y = PLAYER_ACCELERATION;
  //}

  if (state->player.grounded && IsKeyDown(KEY_SPACE)) {
    state->player.grounded = false;
    state->player.vel.y = PLAYER_JUMP_SPEED;
  }

  if (!state->player.grounded) {
    state->player.vel.y += GRAVITY * GetFrameTime();
    state->player.vel.y = fmin(state->player.vel.y, PLAYER_GRAV_TERMINAL_SPEED);
  }

  level_collide(state, &state->player.grounded);

  //state->player.pos.x += state->player.vel.x;
  //state->player.pos.y += state->player.vel.y;

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
  if (IsWindowResized()) {
    plug_state->player.camera.offset.x = (float)GetScreenWidth() * 0.5f;
    plug_state->player.camera.offset.y = (float)GetScreenHeight() * 0.5f;
  }

  float fps = 1.0f / GetFrameTime();
  char fps_str[6] = { 0 };
  snprintf(fps_str, sizeof(fps_str), "%.2f", fps);

  //printf("%f\n", plug_state->player.vel.y);

  BeginDrawing();
  ClearBackground(GetColor(0x33c6f2ff));

  BeginMode2D(plug_state->player.camera);

  draw_level(plug_state);
  draw_player(plug_state);

  {
    Rectangle r = {
      .x = plug_state->player.pos.x + plug_state->player.hitbox.x,
      .y = plug_state->player.pos.y + plug_state->player.hitbox.y,
      .width = plug_state->player.hitbox.width,
      .height = plug_state->player.hitbox.height,
    };
    DrawRectangleLinesEx(r, 0.5f, BLACK);
  }
  update_player(plug_state);
  DrawCircleV((Vector2){ 0, 0 }, 1, BLUE);

  EndMode2D();

  DrawText(fps_str, 0, 0, 20, BLACK);
  EndDrawing();
}
