#include "update-player.h"
#include "plugin.h"

#include <raylib/src/raylib.h>
#include <cglm/include/cglm/cglm.h>

[[maybe_unused]] static bool point_inside_aabb(Rectangle aabb, Vector2 p) {
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

[[maybe_unused]] static bool aabb_collision(vec2 aabb1[2], vec2 aabb2[2]) {
  return !(aabb1[0][0] > aabb2[1][0] || aabb1[1][0] < aabb2[0][0] ||
           aabb1[0][1] > aabb2[1][1] || aabb1[1][1] < aabb2[0][1]);
}

static inline bool is_zero(float n, float eps) {
  return n < eps && n > -eps;
}

[[maybe_unused]] static bool intersect_line_segments(const vec2 line1[2],
                                                     const vec2 line2[2],
                                                     vec2 intersection,
                                                     float *t) {
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

static float resolve_collision(vec2 player_aabb[2], vec2 grid_aabb[2], vec2 vel,
                               float dt, int8_t *index) {
  //if (is_zero(vel[0], EPS) && is_zero(vel[1], EPS)) {
  //  return 0;
  //}

  if (index != NULL) {
    *index = 0;
  }

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

  vec2 raydir;
  //glm_vec2_copy(vel, raydir);
  glm_vec2_scale(vel, dt, raydir);

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

  if (index != NULL && tx_min < ty_min) {
    *index = 1;
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

static void level_collide(struct plug_State *state, bool *is_grounded,
                          float dt) {
  if (state->current_level < 0) {
    return;
  }

  struct plug_Level *level =
    &DA_AT(state->levels, (uint32_t)state->current_level);
  struct plug_Player *player = &state->player;

  vec2 min_t_xy = { 1.0f, 1.0f };
  float joint_t_min = 1.0f;
  int8_t min_index = -1;

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

      vec2 t_xy = { 0 };
      t_xy[0] = resolve_collision(player_aabb, grid_aabb,
                                  (vec2){ player->vel.x, 0.0f }, dt, NULL);
      t_xy[1] = resolve_collision(player_aabb, grid_aabb,
                                  (vec2){ 0.0f, player->vel.y }, dt, NULL);

      int8_t index;
      float t = resolve_collision(player_aabb, grid_aabb,
                                  (vec2){ player->vel.x, player->vel.y }, dt,
                                  &index);
      if (t != 1.0f) {
        if (t < joint_t_min) {
          joint_t_min = t;
          min_index = index;
        }
      }

      min_t_xy[0] = t_xy[0] < min_t_xy[0] ? t_xy[0] : min_t_xy[0];
      min_t_xy[1] = t_xy[1] < min_t_xy[1] ? t_xy[1] : min_t_xy[1];
    }
  }

  min_t_xy[min_index] = joint_t_min;

  *is_grounded = is_zero(min_t_xy[1], EPS) ? true : false;

  player->vel.x *= min_t_xy[0];
  player->vel.y *= min_t_xy[1];
}

void update_player(struct plug_State *state) {
  float dt = GetFrameTime();

  if (IsKeyDown(KEY_A)) {
    state->player.vel.x -= PLAYER_ACCELERATION * dt;
  }

  if (IsKeyDown(KEY_D)) {
    state->player.vel.x += PLAYER_ACCELERATION * dt;
  }

  state->player.vel.x = glm_clamp(state->player.vel.x, -PLAYER_TERMINAL_SPEED,
                                  PLAYER_TERMINAL_SPEED);

  if (state->player.grounded && IsKeyDown(KEY_SPACE)) {
    //state->player.grounded = false;
    state->player.vel.y = PLAYER_JUMP_SPEED;
  }

  state->player.vel.y += GRAVITY * dt;
  state->player.vel.y = fmin(state->player.vel.y, PLAYER_GRAV_TERMINAL_SPEED);

  level_collide(state, &state->player.grounded, dt);

  state->player.pos.x += state->player.vel.x * dt;
  state->player.pos.y += state->player.vel.y * dt;

  state->player.camera.target.x = state->player.pos.x + PLAYER_SIZE * 0.5f;
  state->player.camera.target.y = state->player.pos.y + PLAYER_SIZE * 0.5f;

  //DrawCircleV((Vector2){ state->player.pos.x - state->player.cam_move_pad_x,
  //                       state->player.pos.y },
  //            3.0f, MAGENTA);
  //DrawCircleV((Vector2){ state->player.pos.x + state->player.cam_move_pad_x,
  //                       state->player.pos.y },
  //            3.0f, MAGENTA);

  //if (state->player.pos.x - state->player.camera.target.x >
  //    state->player.cam_move_pad_x) {
  //  state->player.camera.target.x = glm_lerp(
  //    state->player.camera.target.x,
  //    state->player.pos.x - state->player.cam_move_pad_x + (0.5f * PLAYER_SIZE),
  //    0.10f);

  //} else if (state->player.pos.x - state->player.camera.target.x <
  //           state->player.cam_move_pad_x) {
  //  state->player.camera.target.x = glm_lerp(
  //    state->player.camera.target.x,
  //    state->player.pos.x + state->player.cam_move_pad_x + (0.5f * PLAYER_SIZE),
  //    0.10f);
  //}

  //DrawCircleV(state->player.camera.target, 1.0f, BLUE);

  //state->player.camera.target.y =
  //  glm_lerp(state->player.camera.target.y, state->player.pos.y, 0.10f);

  if (state->player.vel.x >= 0) {
    state->player.vel.x -= fmin(PLAYER_DECELERATION * dt, state->player.vel.x);

  } else {
    state->player.vel.x -= fmax(-PLAYER_DECELERATION * dt, state->player.vel.x);
  }
}
