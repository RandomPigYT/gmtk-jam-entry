#pragma once
#ifndef DOOM_STYLE_RENDERER_H
#define DOOM_STYLE_RENDERER_H

#include "common/camera.h"
#include "util/dynamic_array.h"
#include "util/thread_pool.h"
#include "util/arena.h"

#include <cglm/include/cglm/cglm.h>
#include <stdint.h>
#include <assert.h>

#define DSR_COLOUR(format, r, g, b, a)     \
  ({                                       \
    uint32_t __colour = 0;                 \
                                           \
    __colour |= (r) << (format).r_shift;   \
    __colour |= (g) << (format).g_shift;   \
    __colour |= (b) << (format).b_shift;   \
    if ((format).a_bits) {                 \
      __colour |= (a) << (format).a_shift; \
    }                                      \
                                           \
    __colour;                              \
  })

#define DSR_PIXEL_AT(surface, pixels, x, y)                                 \
  (*({                                                                      \
    assert((pixels) != NULL && "Null pixel buffer");                        \
                                                                            \
    uint32_t *pixel = (uint32_t *)(&(                                       \
      (uint8_t *)(pixels))[(y) * (surface)->stride +                        \
                           (x) * (surface)->pixel_format.bytes_per_pixel]); \
                                                                            \
    pixel;                                                                  \
  }))

#define DSR_FLT_EPSILON 1e-6f

struct dsr_PixelFormat {
  uint8_t bits_per_pixel;
  uint8_t bytes_per_pixel;

  uint32_t r_mask;
  uint32_t g_mask;
  uint32_t b_mask;
  uint32_t a_mask;

  uint8_t r_bits;
  uint8_t g_bits;
  uint8_t b_bits;
  uint8_t a_bits;

  uint8_t r_shift;
  uint8_t g_shift;
  uint8_t b_shift;
  uint8_t a_shift;
};

struct dsr_Surface {
  int32_t width, height;
  int32_t stride; // In bytes

  struct dsr_PixelFormat pixel_format;

  void *pixels;
};

struct dsr_Wall {
  uint64_t vertices[2]; // Vertex indices

  bool is_portal;

  uint32_t shared_with[2]; // Indices of sectors that share this wall
  uint8_t shared_count;
};

struct dsr_Sector {
  float floor_height, ceil_height;

  DA_TYPE(uint32_t) walls; // Indices
  DA_TYPE(uint32_t) cycle_ends;
};

struct dsr_Scene {
  DA_TYPE(vec2) vertices;
  DA_TYPE(struct dsr_Wall) walls;
  DA_TYPE(struct dsr_Sector) sectors;
};

// Parses the file specified by "scene_source" and stores inside "scene".
// Does not allocate any memory so, caller must provide a valid pointer.
bool dsr_load_scene(const char *scene_path, struct dsr_Scene *scene);

// The direction vector of the camera must be normalized.
void dsr_render(struct Arena *arena, struct dsr_Surface *surface,
                const struct dsr_Scene *scene, const struct hog_Camera *camera,
                int64_t current_sector);

// if num_cpus <= 0, DSR_DEFAULT_THREAD_COUNT threads will be created
void dsr_render_multithreaded(struct Arena *arena, struct tp_ThreadPool *pool,
                              struct dsr_Surface *surface,
                              const struct dsr_Scene *scene,
                              const struct hog_Camera *camera,
                              int64_t current_sector);

#endif // DOOM_STYLE_RENDERER_H
