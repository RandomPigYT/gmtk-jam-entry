#ifndef UTIL_THREAD_POOL_IMPLEMENTATION
#define UTIL_THREAD_POOL_IMPLEMENTATION
#endif
#include "util/thread_pool.h"

#ifndef UTIL_ARENA_H_IMPLEMENTATION
#define UTIL_ARENA_H_IMPLEMENTATION
#endif
#include "util/arena.h"

#include "doom-style-renderer.h"
#include "render-walls/render-walls.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

static void render_impl(struct Arena *arena, struct dsr_Surface *surface,
                        const struct dsr_Scene *scene,
                        const struct hog_Camera *camera, int64_t current_sector,
                        struct tp_ThreadPool *pool) {
  assert(surface->pixel_format.bytes_per_pixel == 4 &&
         "Unsupported pixel format");

  vec2 proj_plane_size = { 0 };

  proj_plane_size[0] =
    tan(camera->fov * 0.5f) * camera->near_clipping_plane * 2;
  proj_plane_size[1] = proj_plane_size[0] / camera->aspect_ratio;

  //memset(surface->pixels, 0,
  //       surface->width * surface->height *
  //         surface->pixel_format.bytes_per_pixel);

  dsr_render_walls(arena, pool, surface, scene, camera, current_sector,
                   proj_plane_size);
}

void dsr_render(struct Arena *arena, struct dsr_Surface *surface,
                const struct dsr_Scene *scene, const struct hog_Camera *camera,
                int64_t current_sector) {
  render_impl(arena, surface, scene, camera, current_sector, NULL);
}

void dsr_render_multithreaded(struct Arena *arena, struct tp_ThreadPool *pool,
                              struct dsr_Surface *surface,
                              const struct dsr_Scene *scene,
                              const struct hog_Camera *camera,
                              int64_t current_sector) {
  render_impl(arena, surface, scene, camera, current_sector, pool);
}
