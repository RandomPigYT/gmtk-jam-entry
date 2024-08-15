#include "doom-style-renderer.h"
#include "util/dynamic_array.h"
#include "common/camera.h"
#include "util/thread_pool.h"
#include "util/arena.h"

#include <SDL/include/SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define WIDTH 16
#define HEIGHT 9
#define FACTOR 60

#define PLAYER_SPEED 12.0f
#define PLAYER_ANGULAR_SPEED 3.5f

#define PAGE_SIZE 4096

enum direction {
  FORWARD = 0,
  BACKWARDS,
  LEFT,
  RIGHT,
  UP,
  DOWN,

  DIR_COUNT,
};

bool moving[DIR_COUNT] = { 0 };
int32_t turning = 0;

void update_dsr_surface(struct dsr_Surface *dsr_surface,
                        const SDL_Surface *sdl_surface, float factor) {
  const SDL_PixelFormatDetails *format =
    SDL_GetPixelFormatDetails(sdl_surface->format);

  struct dsr_Surface s = {
    .width = sdl_surface->w * factor,
    .height = sdl_surface->h * factor,
    .stride = sdl_surface->w * factor * format->bytes_per_pixel,

    .pixel_format = ({
      struct dsr_PixelFormat pixel_format = {
        .bits_per_pixel = format->bits_per_pixel,
        .bytes_per_pixel = format->bytes_per_pixel,

        .r_mask = format->Rmask,
        .g_mask = format->Gmask,
        .b_mask = format->Bmask,
        .a_mask = format->Amask,

        .r_bits = format->Rbits,
        .g_bits = format->Gbits,
        .b_bits = format->Bbits,
        .a_bits = format->Abits,

        .r_shift = format->Rshift,
        .g_shift = format->Gshift,
        .b_shift = format->Bshift,
        .a_shift = format->Ashift,
      };

      pixel_format;
    }),

  };
  s.pixels = realloc(dsr_surface->pixels, s.stride * s.height);
  assert(s.pixels != NULL && "Failed to allocate memory");

  *dsr_surface = s;
}

static inline bool is_zero(float n, float eps) {
  return n < eps && n > -eps;
}

static bool intersect_line_segments(const vec4 line1[2], const vec4 line2[2],
                                    vec4 intersection, float *t) {
  vec2 p = { line1[0][0], line1[0][2] };
  vec2 q = { line2[0][0], line2[0][2] };

  vec2 r = {
    line1[1][0] - line1[0][0],
    line1[1][2] - line1[0][2],
  };

  vec2 s = {
    line2[1][0] - line2[0][0],
    line2[1][2] - line2[0][2],
  };

  float r_cross_s = glm_vec2_cross(r, s);
  if (is_zero(r_cross_s, DSR_FLT_EPSILON)) {
    intersection[0] = INFINITY;
    intersection[2] = INFINITY;

    return false;
  }

  vec2 q_sub_p;
  glm_vec2_sub(q, p, q_sub_p);

  float q_sub_p_cross_s = glm_vec2_cross(q_sub_p, s);
  float q_sub_p_cross_r = glm_vec2_cross(q_sub_p, r);

  *t = q_sub_p_cross_s / r_cross_s;
  float u = q_sub_p_cross_r / r_cross_s;

  if (!(*t >= 0.0f && *t <= 1.0f && u >= 0.0f && u <= 1.0f)) {
    intersection[0] = INFINITY;
    intersection[2] = INFINITY;

    return false;
  }

  intersection[0] = p[0] + r[0] * *t;
  intersection[2] = p[1] + r[1] * *t;

  return true;
}

struct Supersample {
  struct dsr_Surface *src;
  struct dsr_Surface *dest;
};

static void *supersample(struct Supersample *args) {
  for (int32_t y = 0; y < args->dest->height; y++) {
    for (int32_t x = 0; x < args->dest->width; x++) {
      int32_t sx = (x / (float)args->dest->width) * args->src->width;
      int32_t sy = (y / (float)args->dest->height) * args->src->height;

      DSR_PIXEL_AT(args->dest, args->dest->pixels, x, y) =
        DSR_PIXEL_AT(args->src, args->src->pixels, sx, sy);
    }
  }

  return NULL;
}

int main(int argc, char **argv) {
  if (argc <= 1) {
    fprintf(stderr, "Bad usage\n");
    return EXIT_FAILURE;
  }

  char *scene_path = argv[1];

  SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL: %s\n",
                 SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_Window *window = SDL_CreateWindow("test", WIDTH * FACTOR, HEIGHT * FACTOR,
                                        SDL_WINDOW_RESIZABLE);
  //SDL_Window *window = SDL_CreateWindow("test", WIDTH * FACTOR, HEIGHT * FACTOR,
  //SDL_WINDOW_FULLSCREEN);
  if (!window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s\n",
                 SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_Surface *surface = SDL_GetWindowSurface(window);

  float factor = 0.75f;

  struct dsr_Surface dsr_surface = { 0 };
  update_dsr_surface(&dsr_surface, surface, factor);

  vec4 dir = { 0.0f, 0.0f, 1.0f };
  glm_vec3_normalize(dir);

  struct hog_Camera cam = {
    .fov = M_PI / 2.0f,
    .aspect_ratio = (float)dsr_surface.width / (float)dsr_surface.height,

    .near_clipping_plane = 0.1f,
    .far_clipping_plane = 100.0f,

    .position = { 0.0f, 5.0f, -5.0f },
    //.direction = { 0.0f, 0.0f, 1.0f },
    .direction = { dir[0], dir[1], dir[2] },

    .proj_type = HOG_PROJECTION_NONE,
  };

  struct dsr_Scene scene = {
    .walls = { 0 },
  };

  uint64_t current_count = SDL_GetPerformanceCounter();
  uint64_t prev_count = 0;
  double deltatime = 0;

  if (!dsr_load_scene(scene_path, &scene)) {
    SDL_DestroySurface(surface);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  struct tp_ThreadPool *pool = tp_create_pool(SDL_GetCPUCount() - 1);

  struct Arena arena = arena_create(PAGE_SIZE * 2);

  uint32_t curr_sector = 0;

  SDL_Event e;
  while (true) {
    prev_count = current_count;
    current_count = SDL_GetPerformanceCounter();

    deltatime = (double)(current_count - prev_count) /
                (double)SDL_GetPerformanceFrequency();

    //printf("FPS: %lf\n", 1.0f / deltatime);

    if (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT) {
        break;
      }

      if (e.type == SDL_EVENT_WINDOW_RESIZED ||
          e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        surface = SDL_GetWindowSurface(window);
        update_dsr_surface(&dsr_surface, surface, factor);
        cam.aspect_ratio = (float)dsr_surface.width / (float)dsr_surface.height;
      }

      if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
          case SDLK_W: {
            moving[FORWARD] = true;
          } break;

          case SDLK_A: {
            moving[LEFT] = true;
          } break;

          case SDLK_S: {
            moving[BACKWARDS] = true;
          } break;

          case SDLK_D: {
            moving[RIGHT] = true;
          } break;

          case SDLK_SPACE: {
            moving[UP] = true;
          } break;

          case SDLK_LSHIFT: {
            moving[DOWN] = true;
          } break;

          case SDLK_J: {
            turning = -1;
          } break;

          case SDLK_L: {
            turning = 1;
          } break;

          default: {
            break;
          }
        }
      }

      if (e.type == SDL_EVENT_KEY_UP) {
        switch (e.key.key) {
          case SDLK_W: {
            moving[FORWARD] = false;
          } break;

          case SDLK_A: {
            moving[LEFT] = false;
          } break;

          case SDLK_S: {
            moving[BACKWARDS] = false;
          } break;

          case SDLK_D: {
            moving[RIGHT] = false;
          } break;

          case SDLK_SPACE: {
            moving[UP] = false;
          } break;

          case SDLK_LSHIFT: {
            moving[DOWN] = false;
          } break;

          case SDLK_J: {
            if (turning == -1) {
              turning = 0;
            }
          } break;

          case SDLK_L: {
            if (turning == 1) {
              turning = 0;
            }
          } break;

          default: {
            break;
          }
        }
      }
    }

    vec3 prev_pos;
    glm_vec3_copy(cam.position, prev_pos);

    bool is_moving = false;
    if (moving[FORWARD]) {
      is_moving = true;

      vec3 dir;

      glm_vec3_copy(cam.direction, dir);

      glm_vec3_normalize(dir);

      glm_vec3_scale(dir, PLAYER_SPEED * deltatime, dir);

      glm_vec3_add(cam.position, dir, cam.position);
    }

    if (moving[BACKWARDS]) {
      is_moving = true;

      vec3 dir;

      glm_vec3_copy(cam.direction, dir);

      glm_vec3_normalize(dir);

      glm_vec3_negate(dir);
      glm_vec3_scale(dir, PLAYER_SPEED * deltatime, dir);

      glm_vec3_add(cam.position, dir, cam.position);
    }

    if (moving[RIGHT]) {
      is_moving = true;

      vec3 dir;

      glm_vec3_copy(cam.direction, dir);

      glm_vec3_negate(dir);

      glm_vec3_normalize(dir);

      glm_vec3_cross(dir, (vec3){ 0.0f, 1.0f, 0.0f }, dir);
      glm_vec3_normalize(dir);
      glm_vec3_scale(dir, PLAYER_SPEED * deltatime, dir);

      glm_vec3_add(cam.position, dir, cam.position);
    }

    if (moving[LEFT]) {
      is_moving = true;

      vec3 dir;

      glm_vec3_copy(cam.direction, dir);

      glm_vec3_normalize(dir);

      glm_vec3_cross(dir, (vec3){ 0.0f, 1.0f, 0.0f }, dir);
      glm_vec3_normalize(dir);
      glm_vec3_scale(dir, PLAYER_SPEED * deltatime, dir);

      glm_vec3_add(cam.position, dir, cam.position);
    }

    if (is_moving) {
      auto sector = &DA_AT(scene.sectors, curr_sector);
      for (uint32_t j = 0; j < sector->walls.count; j++) {
        auto wall = &DA_AT(scene.walls, DA_AT(sector->walls, j));
        vec4 l1[2] = {
          {
            [0] = DA_AT(scene.vertices, wall->vertices[0])[0],
            [2] = DA_AT(scene.vertices, wall->vertices[0])[1],
          },

          {
            [0] = DA_AT(scene.vertices, wall->vertices[1])[0],
            [2] = DA_AT(scene.vertices, wall->vertices[1])[1],
          },
        };

        vec4 l2[2] = {
          {
            [0] = cam.position[0],
            [2] = cam.position[2],
          },

          {
            [0] = prev_pos[0],
            [2] = prev_pos[2],
          },
        };

        vec4 inter = { 0 };
        float t = 0;
        bool did_isect = intersect_line_segments(l1, l2, inter, &t);

        if (wall->is_portal) {
          if (did_isect) {
            curr_sector = wall->shared_with[0] != curr_sector
                            ? wall->shared_with[0]
                            : wall->shared_with[1];

            goto Break;
          }

        } else {
          if (did_isect) {
            vec4 wall_dir;
            glm_vec4_sub(l1[1], l1[0], wall_dir);
            glm_vec4_normalize(wall_dir);

            vec4 vel = { 0 };
            glm_vec4_sub(l2[0], l2[1], vel);

            float scale = glm_vec4_dot(vel, wall_dir);
            glm_vec4_scale(wall_dir, scale, wall_dir);

            vec3 new_pos;
            glm_vec3_add(prev_pos, wall_dir, new_pos);

            glm_vec3_copy(new_pos, cam.position);

            glm_vec3_copy(inter, prev_pos);
          }
        }
      }
    }

Break:

    if (moving[UP]) {
      cam.position[1] += PLAYER_SPEED * deltatime;
    }

    if (moving[DOWN]) {
      cam.position[1] -= PLAYER_SPEED * deltatime;
    }

    if (turning != 0) {
      mat4 rot = GLM_MAT4_IDENTITY_INIT;
      glm_rotate(rot, turning * PLAYER_ANGULAR_SPEED * deltatime,
                 (vec3){ 0.0f, 1.0f, 0.0f });

      vec4 temp;
      glm_vec3_copy(cam.direction, temp);
      temp[3] = 1.0f;

      vec4 res;
      glm_mat4_mulv_sse2(rot, temp, res);

      glm_vec3_copy(res, cam.direction);
    }

    //glm_vec3_print(cam.position, stdout);
    if (scene.sectors.count > 0) {
      ArenaSaveState r;
      arena_save(arena, &r);

      //dsr_render(&arena, &dsr_surface, &scene, &cam, curr_sector);
      dsr_render_multithreaded(&arena, pool, &dsr_surface, &scene, &cam,
                               curr_sector);

      struct dsr_Surface dest = {
        .width = surface->w,
        .height = surface->h,
        .stride = surface->pitch,
        .pixel_format = dsr_surface.pixel_format,
        .pixels = surface->pixels,
      };

      supersample(&(struct Supersample){
        .src = &dsr_surface,
        .dest = &dest,
      });

      arena_restore(&arena, r);
    }

    SDL_UpdateWindowSurface(window);
  }

  tp_free_pool(pool);
  arena_free(&arena);

  SDL_DestroySurface(surface);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
