#ifndef DSR_RENDER_WALL_PROJ_MATH_H
#define DSR_RENDER_WALL_PROJ_MATH_H

#include "doom-style-renderer.h"
#include "common/camera.h"
#include <cglm/include/cglm/cglm.h>

static inline void get_world_coords(const vec2 map_coords, vec4 world_coords) {
  world_coords[0] = map_coords[0];
  world_coords[1] = 0.0f;
  world_coords[2] = map_coords[1];
  world_coords[3] = 1.0f;
}

static inline void get_relative_coords(const struct hog_Camera *camera,
                                       const vec4 world_coords,
                                       vec4 relative_coords) {
  glm_vec3_sub((float *)world_coords, (float *)camera->position,
               relative_coords);
  relative_coords[3] = 1.0f;

  float theta = atan2(camera->direction[0], camera->direction[2]);
  mat4 rotate = GLM_MAT4_IDENTITY_INIT;
  glm_rotate(rotate, -theta, (vec3){ 0.0f, 1.0f, 0.0f });

  vec4 temp;
  glm_mat4_mulv_sse2(rotate, relative_coords, temp);

  glm_vec4_copy(temp, relative_coords);
}

static inline float x_projection(const struct hog_Camera *camera,
                                 vec4 relative_coords) {
  //printf("relative: %f\n", relative_coords[2]);
  return relative_coords[0] * camera->near_clipping_plane /
         (relative_coords[2] + DSR_FLT_EPSILON);
}

static inline void y_projection(const struct hog_Camera *camera,
                                const vec4 relative_coords, float floor_height,
                                float ceil_height, vec2 projected) {
  float depth = relative_coords[2] + DSR_FLT_EPSILON;
  float cam_height = camera->position[1];

  projected[0] = ceil_height - cam_height;
  projected[1] = floor_height - cam_height;

  glm_vec2_divs(projected, depth, projected);
  glm_vec2_scale(projected, camera->near_clipping_plane, projected);
}

static inline bool is_zero(float n, float eps) {
  return n < eps && n > -eps;
}

static bool intersect_line_segments(const vec4 line1[2], const vec4 line2[2],
                                    vec4 intersection) {
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

  float t = q_sub_p_cross_s / r_cross_s;
  float u = q_sub_p_cross_r / r_cross_s;

  if (!(t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f)) {
    intersection[0] = INFINITY;
    intersection[2] = INFINITY;

    return false;
  }

  intersection[0] = p[0] + r[0] * t;
  intersection[2] = p[1] + r[1] * t;

  return true;
}

static bool clipped_wall_positions(const vec4 relative_coords[2],
                                   const struct hog_Camera *camera,
                                   vec4 clipped[2]) {
  glm_vec4_copy((float *)relative_coords[0], clipped[0]);
  glm_vec4_copy((float *)relative_coords[1], clipped[1]);

  // Clip by the z = 0 line.

  if (clipped[0][2] < 0.0f) {
    vec4 dir;
    glm_vec4_sub((float *)clipped[0], (float *)clipped[1], dir);

    if (is_zero(dir[2], DSR_FLT_EPSILON)) {
      return false;
    }

    float t = (0.0f - clipped[1][2]) / dir[2];

    clipped[0][2] = 0.0f;
    clipped[0][0] = clipped[1][0] + dir[0] * t;

    //glm_vec4_print(clipped[0], stdout);

  } else if (clipped[1][2] < 0.0f) {
    vec4 dir;
    glm_vec4_sub((float *)clipped[1], (float *)clipped[0], dir);

    if (is_zero(dir[2], DSR_FLT_EPSILON)) {
      return false;
    }

    float t = (0.0f - clipped[0][2]) / dir[2];

    clipped[1][2] = 0.0f;
    clipped[1][0] = clipped[0][0] + dir[0] * t;
  }

  // Clip by far clipping plane

  if (clipped[0][2] > camera->far_clipping_plane) {
    vec4 dir;
    glm_vec4_sub((float *)clipped[0], (float *)clipped[1], dir);

    if (is_zero(dir[2], DSR_FLT_EPSILON)) {
      return false;
    }

    float t = (camera->far_clipping_plane - clipped[1][2]) / dir[2];

    clipped[0][2] = camera->far_clipping_plane;
    clipped[0][0] = clipped[1][0] + dir[0] * t;

  } else if (clipped[1][2] > camera->far_clipping_plane) {
    vec4 dir;
    glm_vec4_sub((float *)clipped[1], (float *)clipped[0], dir);

    if (is_zero(dir[2], DSR_FLT_EPSILON)) {
      return false;
    }

    float t = (camera->far_clipping_plane - clipped[0][2]) / dir[2];

    clipped[1][2] = camera->far_clipping_plane;
    clipped[1][0] = clipped[0][0] + dir[0] * t;
  }

  // Clip by view frustum

  vec2 thetas = {
    atan2(clipped[0][0], clipped[0][2]),
    atan2(clipped[1][0], clipped[1][2]),
  };

  if ((thetas[0] > camera->fov * 0.5f && thetas[1] > camera->fov * 0.5f) ||
      (thetas[0] < -camera->fov * 0.5f && thetas[1] < -camera->fov * 0.5f)) {
    return false;
  }

  vec4 intersections[2] = { 0 };
  bool intersected[2] = { 0 };

  float half_far_clipping_plane_length =
    camera->far_clipping_plane * tan(camera->fov * 0.5f);

  vec4 fov_boundary[4] = {
    { 0.0f, 0.0, 0.0f, 0.0f },
    { half_far_clipping_plane_length, 0.0f, camera->far_clipping_plane, 0.0f },
    { 0.0f, 0.0, 0.0f, 0.0f },
    { -half_far_clipping_plane_length, 0.0f, camera->far_clipping_plane, 0.0f },
  };

  intersected[0] =
    intersect_line_segments(clipped, fov_boundary, intersections[0]);
  intersected[1] =
    intersect_line_segments(clipped, fov_boundary + 2, intersections[1]);

  //printf("Intersections: \n");
  //glm_vec4_print(intersections[0], stdout);
  //glm_vec4_print(intersections[1], stdout);

  vec4 dv;
  glm_vec4_sub(clipped[1], clipped[0], dv);
  //printf("dv: \n");
  //glm_vec4_print(dv, stdout);

  if (intersected[0]) {
    if (thetas[0] <= thetas[1]) {
      glm_vec4_copy(intersections[0], clipped[1]);
      //printf("1. Copied to 2\n");

    } else {
      glm_vec4_copy(intersections[0], clipped[0]);
      //printf("1. Copied to 1\n");
    }
  }

  if (intersected[1]) {
    if (thetas[0] <= thetas[1]) {
      glm_vec4_copy(intersections[1], clipped[0]);
      //printf("2. Copied to 1\n");

    } else {
      glm_vec4_copy(intersections[1], clipped[1]);
      //printf("2. Copied to 2\n");
    }
  }
  //printf("Intersected: %d %d\n", intersected[0], intersected[1]);

  //printf("Clipped: \n");
  //glm_vec4_print(clipped[0], stdout);
  //glm_vec4_print(clipped[1], stdout);

  return true;
}

static inline int32_t int_clamp(int32_t val, int32_t min_val, int32_t max_val) {
  if (val < min_val) {
    return min_val;

  } else if (val > max_val) {
    return max_val;

  } else {
    return val;
  }
}

#endif // DSR_RENDER_WALL_PROJ_MATH_H
