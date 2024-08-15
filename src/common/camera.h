#ifndef COMMON_CAMERA_H
#define COMMON_CAMERA_H

#include <cglm/include/cglm/cglm.h>

enum com_ProjType {
  com_PROJECTION_NONE = 0,

  com_PERSPECTIVE_PROJECTION,
  com_ORTHOGRAPHIC_PROJECTION,
};

struct com_Camera {
  float fov; // In radians
  float aspect_ratio;

  float near_clipping_plane;
  float far_clipping_plane;

  vec3 position;
  vec3 direction;

  enum com_ProjType proj_type;
};

#endif // COMMON_CAMERA_H
