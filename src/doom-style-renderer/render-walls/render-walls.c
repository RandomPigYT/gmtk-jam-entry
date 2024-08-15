#include "doom-style-renderer.h"
#include "render-walls.h"
#include "proj-math.h"

#include "util/arena.h"
#include "util/dynamic_array.h"
#include "util/thread_pool.h"

#include <cglm/include/cglm/cglm.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define PAGE_SIZE 4096

struct PortalMask {
  int32_t portal_screen_space[4][2]; // Unclamped portal coords
  struct PortalMask *prev;
};

struct Portal {
  uint32_t sector_index;
  int64_t from_sector_index;
  struct PortalMask *portal_mask;
};

struct PortalQueue {
  DA_TYPE(struct Portal) portals;
};

static void draw_vertical_line(struct dsr_Surface *surface, int32_t x,
                               int32_t y1, int32_t y2, uint8_t colour[4]) {
  if (x < 0 || x >= surface->width)
    return;

  if (y2 < y1) {
    int32_t temp = y1;
    y1 = y2;
    y2 = temp;
  }

  y1 = int_clamp(y1, 0, surface->height - 1);
  y2 = int_clamp(y2, 0, surface->height - 1);
  if (y1 == y2) {
    return;
  }

  for (int32_t y = y1; y <= y2; y++) {
    DSR_PIXEL_AT(surface, surface->pixels, x, y) = DSR_COLOUR(
      surface->pixel_format, colour[0], colour[1], colour[2], colour[3]);
  }
}

static inline void to_screen_space(const struct dsr_Surface *surface,
                                   const vec2 projected,
                                   const vec2 proj_plane_size,
                                   int32_t screen_space[2]) {
  assert(proj_plane_size[0] > 0 && proj_plane_size[1] > 0);

  screen_space[0] =
    lround(((projected[0] / proj_plane_size[0]) + 0.5f) * (surface->width - 1));

  screen_space[1] =
    lround((1.0f - ((projected[1] / proj_plane_size[1]) + 0.5f)) *
           (surface->height - 1));
}

static void get_draw_width(struct PortalMask *mask, int x[2],
                           int32_t masked_x[2]) {
  int32_t portal_width[2];

  if (x == NULL) {
    portal_width[0] = mask->portal_screen_space[0][0];
    portal_width[1] = mask->portal_screen_space[3][0];

  } else {
    portal_width[0] = x[0];
    portal_width[1] = x[1];
  }

  if (mask->prev == NULL) {
    masked_x[0] = portal_width[0];
    masked_x[1] = portal_width[1];

    return;
  }

  int32_t prev_portal_width[2] = {
    mask->prev->portal_screen_space[0][0],
    mask->prev->portal_screen_space[3][0],
  };

  masked_x[0] =
    int_clamp(portal_width[0], prev_portal_width[0], prev_portal_width[1]);
  masked_x[1] =
    int_clamp(portal_width[1], prev_portal_width[0], prev_portal_width[1]);

  get_draw_width(mask->prev, masked_x, masked_x);
}

static bool get_draw_height(struct PortalMask *mask, int32_t x, int y[2],
                            int32_t masked_y[2]) {
  int32_t portal_height[2];

  if (y == NULL) {
    int32_t l =
      mask->portal_screen_space[3][0] - mask->portal_screen_space[0][0];

    float t = (float)(x - mask->portal_screen_space[0][0]) / (float)l;

    portal_height[0] = lround(glm_lerp(mask->portal_screen_space[0][1],
                                       mask->portal_screen_space[3][1], t));
    portal_height[1] = lround(glm_lerp(mask->portal_screen_space[1][1],
                                       mask->portal_screen_space[2][1], t));

  } else {
    portal_height[0] = y[0];
    portal_height[1] = y[1];
  }

  if (mask->prev == NULL) {
    masked_y[0] = portal_height[0];
    masked_y[1] = portal_height[1];
    return true;
  }

  int32_t prev_portal_height[2];

  {
    int32_t l = mask->prev->portal_screen_space[3][0] -
                mask->prev->portal_screen_space[0][0];

    if (l == 0) {
      return false;
    }

    float t = (float)(x - mask->prev->portal_screen_space[0][0]) / (float)l;

    prev_portal_height[0] =
      lround(glm_lerp(mask->prev->portal_screen_space[0][1],
                      mask->prev->portal_screen_space[3][1], t));
    prev_portal_height[1] =
      lround(glm_lerp(mask->prev->portal_screen_space[1][1],
                      mask->prev->portal_screen_space[2][1], t));
  }

  masked_y[0] =
    int_clamp(portal_height[0], prev_portal_height[0], prev_portal_height[1]);
  masked_y[1] =
    int_clamp(portal_height[1], prev_portal_height[0], prev_portal_height[1]);

  return get_draw_height(mask->prev, x, masked_y, masked_y);
}

struct WallSection {
  struct dsr_Surface *surface;
  const struct dsr_Scene *scene;
  const struct hog_Camera *camera;

  float *depth_buffer;

  const struct Portal *portal;

  int32_t screen_space[4][2];
  int32_t x1, x2, z1, z2; // Range of the entire wall

  int32_t x_range[2]; // Range of wall section
  int32_t sign;

  const struct dsr_Sector *cam_sector;
  const struct dsr_Sector *drawing_sector;
  const struct dsr_Wall *wall;

  uint8_t colour[4];
};
static void *draw_wall_section(struct WallSection *args) {
  int32_t draw_width[2];
  get_draw_width(args->portal->portal_mask, NULL, draw_width);

  if (draw_width[1] - draw_width[0] == 0) {
    return NULL;
  }

  int32_t l = args->x2 - args->x1;
  int32_t unclamped_x1 = args->x1;

  args->x1 = glm_imax(args->x1, draw_width[0]);
  args->x2 = glm_imin(args->x2, draw_width[1]);

  for (int32_t x = args->x_range[0]; x <= args->x_range[1]; x++) {
    if (x < args->x1 || x > args->x2) {
      continue;
    }

    float t = (float)(x - unclamped_x1) / (float)l;

    float depth_lerp =
      glm_lerp(args->z1, args->z2, t) / args->camera->far_clipping_plane;

    if (!args->wall->is_portal && depth_lerp > args->depth_buffer[x]) {
      continue;
    }

    if (args->sign < 0) {
      t = 1.0f - t;
    }

    int32_t draw_height[2];

    if (!get_draw_height(args->portal->portal_mask, x, NULL, draw_height)) {
      return NULL;
    }

    if (!args->wall->is_portal) {
      args->depth_buffer[x] = depth_lerp;
    }

    int32_t y[2] = {
      int_clamp(
        lround(glm_lerp(args->screen_space[0][1], args->screen_space[3][1], t)),
        draw_height[0], draw_height[1]),
      int_clamp(
        lround(glm_lerp(args->screen_space[1][1], args->screen_space[2][1], t)),
        draw_height[0], draw_height[1]),
    };

    uint8_t c[4] = {
      glm_lerp(args->colour[0] / 255.0f, 0.0f, depth_lerp) * 255.0f,
      glm_lerp(args->colour[1] / 255.0f, 0.0f, depth_lerp) * 255.0f,
      glm_lerp(args->colour[2] / 255.0f, 0.0f, depth_lerp) * 255.0f,
      255,
    };

    draw_vertical_line(args->surface, x, draw_height[0], y[0],
                       (uint8_t[4]){ 35, 35, 35, 255 });
    draw_vertical_line(args->surface, x, y[1], draw_height[1],
                       (uint8_t[4]){ 100, 24, 24, 255 });

    if (!args->wall->is_portal) {
      draw_vertical_line(args->surface, x, y[0], y[1], c);

    } else {
      if (args->drawing_sector->ceil_height < args->cam_sector->ceil_height) {
        draw_vertical_line(args->surface, x, y[0], draw_height[0], c);
      }

      if (args->drawing_sector->floor_height > args->cam_sector->floor_height) {
        draw_vertical_line(args->surface, x, y[0], draw_height[1], c);
      }
    }
  }

  return NULL;
}

struct RenderWallArgs {
  struct dsr_Surface *surface;
  const struct dsr_Scene *scene;

  const struct hog_Camera *camera;
  const vec2 proj_plane_size;

  float *depth_buffer;

  struct tp_ThreadPool *pool;

  uint32_t cam_sector_index;
  uint32_t drawing_sector_index;

  uint32_t wall_index;
  uint8_t wall_colour[4];

  struct PortalQueue portal_queue;
  const struct Portal *portal;

  struct Arena *arena;
};
static void render_wall(struct RenderWallArgs *args) {
  struct dsr_Wall *wall = &DA_AT(args->scene->walls, args->wall_index);

  vec4 wall_world_coords[2] = { 0 };
  get_world_coords(DA_AT(args->scene->vertices, wall->vertices[0]),
                   wall_world_coords[0]);
  get_world_coords(DA_AT(args->scene->vertices, wall->vertices[1]),
                   wall_world_coords[1]);

  vec4 relative_coords[2] = { 0 };
  get_relative_coords(args->camera, wall_world_coords[0], relative_coords[0]);
  get_relative_coords(args->camera, wall_world_coords[1], relative_coords[1]);

  if (relative_coords[0][2] < 0.0f && relative_coords[1][2] < 0.0f) {
    return;

  } else if (relative_coords[0][2] > args->camera->far_clipping_plane &&
             relative_coords[1][2] > args->camera->far_clipping_plane) {
    return;
  }

  vec4 clipped_coords[2] = { 0 };
  if (!clipped_wall_positions(relative_coords, args->camera, clipped_coords)) {
    return;
  }

  // Counter-clockwise
  vec2 projected[4] = { 0 };
  vec2 projected_x = {
    x_projection(args->camera, clipped_coords[0]),
    x_projection(args->camera, clipped_coords[1]),
  };

  {
    struct dsr_Sector *s =
      &DA_AT(args->scene->sectors, args->drawing_sector_index);
    vec2 temp = { 0 };
    y_projection(args->camera, clipped_coords[0], s->floor_height,
                 s->ceil_height, temp);

    projected[0][0] = projected_x[0];
    projected[0][1] = temp[0];

    projected[1][0] = projected_x[0];
    projected[1][1] = temp[1];
  }

  {
    struct dsr_Sector *s =
      &DA_AT(args->scene->sectors, args->drawing_sector_index);
    vec2 temp = { 0 };
    y_projection(args->camera, clipped_coords[1], s->floor_height,
                 s->ceil_height, temp);

    projected[2][0] = projected_x[1];
    projected[2][1] = temp[1];

    projected[3][0] = projected_x[1];
    projected[3][1] = temp[0];
  }

  //printf("Projected: \n");
  for (uint32_t i = 0; i < 4; i++) {
    //glm_vec2_print(projected[i], stdout);
  }

  int32_t screen_space[4][2] = { 0 };
  //printf("Screen space: \n");
  for (uint32_t i = 0; i < 4; i++) {
    to_screen_space(args->surface, projected[i], args->proj_plane_size,
                    screen_space[i]);
  }

  //printf("\n");
  //printf("======================================================\n");
  //printf("\n");

  int32_t x1, x2, z1, z2;
  int32_t y_coords[4];

  if (screen_space[2][0] >= screen_space[0][0]) {
    x1 = screen_space[0][0];
    z1 = clipped_coords[0][2];
    x2 = screen_space[2][0];
    z2 = clipped_coords[1][2];

    y_coords[0] = screen_space[0][1];
    y_coords[1] = screen_space[1][1];
    y_coords[2] = screen_space[2][1];
    y_coords[3] = screen_space[3][1];

  } else {
    x1 = screen_space[2][0];
    z1 = clipped_coords[1][2];
    x2 = screen_space[0][0];
    z2 = clipped_coords[0][2];

    y_coords[0] = screen_space[3][1];
    y_coords[1] = screen_space[2][1];
    y_coords[2] = screen_space[1][1];
    y_coords[3] = screen_space[0][1];
  }

  if (wall->is_portal) {
    struct Portal p = { 0 };

    assert(wall->shared_count <= 2);

    p.sector_index = wall->shared_with[0] != args->drawing_sector_index
                       ? wall->shared_with[0]
                       : wall->shared_with[1];

    bool should_append = true;
    for (uint64_t i = 0; i < args->portal_queue.portals.count; i++) {
      if (DA_AT(args->portal_queue.portals, i).from_sector_index ==
          p.sector_index) {
        should_append = false;
        break;
      }
    }

    if (should_append) {
      p.from_sector_index = args->drawing_sector_index;
      p.portal_mask = arena_alloc(args->arena, sizeof(*p.portal_mask));
      assert(p.portal_mask != NULL && "Arena overflow");

      p.portal_mask->prev = args->portal->portal_mask;

      // The y axis of the world space coordinates are flipped when converting to scree space,
      // hence, the higher point in world space has a smaller screen space y coordinate.
      for (uint8_t i = 0; i < 4; i++) {
        if (i < 2) {
          p.portal_mask->portal_screen_space[i][0] = x1;

        } else {
          p.portal_mask->portal_screen_space[i][0] = x2;
        }

        p.portal_mask->portal_screen_space[i][1] = y_coords[i];
      }

      DA_APPEND(&args->portal_queue.portals, p);
    }
  }

  int32_t l = x2 - x1;

  int32_t sign = glm_sign(screen_space[2][0] - screen_space[0][0]);

  ArenaSaveState r;
  arena_save(*args->arena, &r);

  if (args->pool == NULL) {
    struct WallSection *tmp = arena_alloc(args->arena, sizeof(*tmp));
    assert(tmp != NULL && "Arena overflow");

    *tmp = (struct WallSection){
      .surface = args->surface,
      .scene = args->scene,
      .camera = args->camera,

      .depth_buffer = args->depth_buffer,

      .portal = args->portal,

      .x1 = x1,
      .z1 = z1,
      .x2 = x2,
      .z2 = z2,

      .x_range = { x1, x2 },
      .sign = sign,

      .cam_sector = &DA_AT(args->scene->sectors, args->cam_sector_index),
      .drawing_sector =
        &DA_AT(args->scene->sectors, args->drawing_sector_index),

      .wall = wall,
    };

    memcpy(tmp->screen_space, screen_space, sizeof(screen_space));
    memcpy(tmp->colour, args->wall_colour, sizeof(args->wall_colour));

    draw_wall_section(tmp);

  } else {
    uint32_t num_batches = args->pool->count;
    uint32_t section_size = l / num_batches;

    DA_TYPE(tp_JobHandle) handles = { 0 };

    for (uint32_t i = 0; i < num_batches; i++) {
      struct WallSection *tmp = arena_alloc(args->arena, sizeof(*tmp));
      assert(tmp != NULL && "Arena overflow");

      *tmp = (struct WallSection){
        .surface = args->surface,
        .scene = args->scene,
        .camera = args->camera,

        .depth_buffer = args->depth_buffer,

        .portal = args->portal,

        .x1 = x1,
        .z1 = z1,
        .x2 = x2,
        .z2 = z2,

        .sign = sign,

        .cam_sector = &DA_AT(args->scene->sectors, args->cam_sector_index),
        .drawing_sector =
          &DA_AT(args->scene->sectors, args->drawing_sector_index),

        .wall = wall,
      };

      memcpy(tmp->screen_space, screen_space, sizeof(screen_space));
      memcpy(tmp->colour, args->wall_colour, sizeof(args->wall_colour));

      tmp->x_range[0] = glm_imin(x1 + i * section_size, x2);
      tmp->x_range[1] = glm_imax(tmp->x_range[0] + section_size - 1, x2);

      tp_JobHandle handle =
        tp_add_job(args->pool, (tp_JobCallback)draw_wall_section, tmp);
      DA_APPEND(&handles, handle);
    }

    for (uint32_t i = 0; i < handles.count; i++) {
      tp_wait_job(args->pool, DA_AT(handles, i));
    }

    DA_FREE(&handles);
  }
  arena_restore(args->arena, r);
}

void dsr_render_walls(struct Arena *arena, struct tp_ThreadPool *pool,
                      struct dsr_Surface *surface,
                      const struct dsr_Scene *scene,
                      const struct hog_Camera *camera, int64_t current_sector,
                      vec2 proj_plane_size) {
  if (scene->sectors.count == 0) {
    return;
  }

  srand((int)6942080085);
  for (int32_t i = 0; i < rand() % 100; i++) {
    rand();
  }
  DA_TYPE_ARRAY(uint8_t, 4) palette = { 0 };
  for (uint32_t i = 0; i < scene->walls.count; i++) {
    DA_APPEND_NO_ASSIGN(&palette);
    DA_AT(palette, palette.count - 1)
    [0] = ((float)rand() / (float)RAND_MAX) * 255.0f;
    DA_AT(palette, palette.count - 1)
    [1] = ((float)rand() / (float)RAND_MAX) * 255.0f;
    DA_AT(palette, palette.count - 1)
    [2] = ((float)rand() / (float)RAND_MAX) * 255.0f;
    DA_AT(palette, palette.count - 1)[3] = 255;
  }

  struct RenderWallArgs render_wall_args = {
    .surface = surface,
    .scene = scene,

    .camera = camera,
    .proj_plane_size = { proj_plane_size[0], proj_plane_size[1] },

    .depth_buffer = ({
      auto tmp =
        malloc(surface->width * sizeof(*render_wall_args.depth_buffer));
      assert(tmp != NULL && "Failed to allocate memory");

      tmp;
    }),

    .pool = pool,

    .cam_sector_index = current_sector,

    .arena = arena,
  };

  for (int32_t i = 0; i < surface->width; i++) {
    render_wall_args.depth_buffer[i] = INFINITY;
  }

  DA_APPEND(&render_wall_args.portal_queue.portals, ({
    struct Portal portal = {
      .sector_index = current_sector,
      .from_sector_index = current_sector,

    };

    portal.portal_mask = arena_alloc(arena, sizeof(*portal.portal_mask));
    assert(portal.portal_mask != NULL && "Arena overflow");

    int32_t portal_screen_space[4][2] = {
      { 0, 0 },
      { 0, surface->height - 1 },
      { surface->width - 1, surface->height - 1 },
      { surface->width - 1, 0 },
    };

    memcpy(portal.portal_mask->portal_screen_space, portal_screen_space,
           sizeof(portal_screen_space));
    portal.portal_mask->prev = NULL;

    portal;
  }));

  while (render_wall_args.portal_queue.portals.count) {
    struct Portal *p = &DA_AT(render_wall_args.portal_queue.portals, 0);
    //printf("Sector: %d\n", p->sector_index);
    //printf("draw_area {\n"
    //       "  { %d, %d },\n"
    //       "  { %d, %d },\n"
    //       "  { %d, %d },\n"
    //       "  { %d, %d },\n"
    //       "}\n\n",
    //       p->draw_area[0][0], p->draw_area[0][1], p->draw_area[1][0],
    //       p->draw_area[1][1], p->draw_area[2][0], p->draw_area[2][1],
    //       p->draw_area[3][0], p->draw_area[3][1]);

    struct dsr_Sector *sector = &DA_AT(scene->sectors, p->sector_index);

    render_wall_args.portal = p;
    render_wall_args.drawing_sector_index = p->sector_index;

    struct hog_Camera temp_cam = *camera;

    glm_vec3_add(
      temp_cam.position,
      (vec3){ 0.0f,
              DA_AT(scene->sectors, (uint32_t)current_sector).floor_height,
              0.0f },
      temp_cam.position);

    render_wall_args.camera = &temp_cam;

    for (uint32_t j = 0; j < sector->walls.count; j++) {
      uint32_t wall_index = DA_AT(sector->walls, j);

      uint8_t colour[4] = {
        DA_AT(palette, wall_index)[0],
        DA_AT(palette, wall_index)[1],
        DA_AT(palette, wall_index)[2],
        255,
      };

      render_wall_args.wall_index = wall_index;
      memcpy(render_wall_args.wall_colour, colour, sizeof(colour));

      render_wall(&render_wall_args);
    }

    DA_POP(&render_wall_args.portal_queue.portals, 0);
  }

  free(render_wall_args.depth_buffer);
  DA_FREE(&palette);
  DA_FREE(&render_wall_args.portal_queue.portals);
}
