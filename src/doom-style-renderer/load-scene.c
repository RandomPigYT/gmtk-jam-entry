#include "doom-style-renderer.h"

#ifndef UTIL_FILE_IO_IMPLEMENTATION
#define UTIL_FILE_IO_IMPLEMENTATION
#endif
#include "util/fileIO.h"
#include "util/dynamic_array.h"

#include "cglm/include/cglm/cglm.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define TOKENS_IN_HEADER_GUARD 3

struct StringView {
  const char *view;
  uint64_t length;
};

enum BlockType {

  BLOCK_TYPE_NONE = 0,

  BLOCK_TYPE_VERTS,
  BLOCK_TYPE_WALLS,
  BLOCK_TYPE_SECTORS,

  BLOCK_COUNT,

};

struct Tokens {
  struct StringView *items;
  uint64_t count;
  uint64_t capacity;
};

static const char seperators[] = " \t"; // Removed
static const char special[] = "[]#,\n"; // Preseverved as tokens

#define BLOCKS_GUARDS() \
  X(BLOCK_GUARD_NONE)   \
                        \
  X(BEGIN_VERTS)        \
  X(END_VERTS)          \
                        \
  X(BEGIN_WALLS)        \
  X(END_WALLS)          \
                        \
  X(BEGIN_SECTORS)      \
  X(END_SECTORS)

#define PARSE_ERRORS()               \
  X(PARSE_ERROR_NONE)                \
  X(PARSE_ERROR_EXPECTED_TOKEN)      \
  X(PARSE_ERROR_UNEXPECTED_TOKEN)    \
  X(PARSE_ERROR_ID_OUT_OF_RANGE)     \
  X(PARSE_ERROR_FLOAT_OUT_OF_RANGE)  \
  X(PARSE_ERROR_UNDEFINED_REFERENCE) \
  X(PARSE_ERROR_INVALID_WALL)

// clang-format off

#define X(name) name,
enum BlockGuard {
  BLOCKS_GUARDS()

	BLOCK_GUARDS_COUNT,
};
#undef X

#define X(name) #name,
static const char *block_guards[] = { 

	BLOCKS_GUARDS()

};
#undef X


#define X(name) name,
enum ParseErrors {
	PARSE_ERRORS()
	
	PARSE_ERROR_COUNT,
};
#undef X

#define X(name) #name,
static const char *parse_errors[] = { 

	PARSE_ERRORS()

};
#undef X

// clang-format on

/*
 * The values of 'enum WallFields' must match the offsets of the fields
 * of 'struct dsr_Wall'.
 */
enum WallFields {

  WALL_FIELD_VERT1,
  WALL_FIELD_VERT2,

  WALL_FIELD_IS_PORTAL,

  WALL_FIELDS_COUNT,

};

/*
 * The values of 'enum SectorBuffers' must match the offsets of the fields
 * of 'struct dsr_Sector'.
 */
enum SectorFields {

  SECTOR_FIELD_FLOOR_HEIGHT,
  SECTOR_FIELD_CEIL_HEIGHT,

  SECTOR_FIELDS_COUNT,

  /* WALL1 WALL2 ... */

};

// Returned dynamic array must be freed by caller
static struct Tokens tokenize(const char *src) {
  struct Tokens toks = { 0 };

  struct StringView token = { 0 };
  while (*src) {
    if (strstr(seperators, (char[]){ *src, 0 })) {
      if (token.length) {
        DA_APPEND(&toks, token);
      }

      token = (struct StringView){ 0 };

    } else if (strstr(special, (char[]){ *src, 0 })) {
      if (token.length) {
        DA_APPEND(&toks, token);
      }

      token.view = src;
      token.length = 1;
      DA_APPEND(&toks, token);

      token = (struct StringView){ 0 };

    } else {
      if (token.view == NULL) {
        token.view = src;
      }

      token.length++;
    }

    src++;
  }

  return toks;
}

static enum ParseErrors parse_block_guard(const struct Tokens *tokens,
                                          uint64_t index,
                                          enum BlockGuard *guard,
                                          uint64_t *error_index) {
  assert(guard != NULL);
  assert(tokens != NULL);

  uint64_t error_index__ = 0;
  enum ParseErrors error_type__ = PARSE_ERROR_NONE;

  *guard = BLOCK_GUARD_NONE;

  if (index >= tokens->count - 2) {
    error_index__ = tokens->count - 1;
    error_type__ = PARSE_ERROR_EXPECTED_TOKEN;

    goto Error;
  }

  for (uint32_t i = 1; i < BLOCK_GUARDS_COUNT; i++) {
    const struct StringView *tok_view = &DA_AT(*tokens, index + 1);

    char *tmp = malloc(tok_view->length + 1);
    assert(tmp != NULL && "Failed to allocate memory");

    memcpy(tmp, tok_view->view, tok_view->length);
    tmp[tok_view->length] = 0;

    if (strcmp(tmp, block_guards[i]) == 0) {
      *guard = i;

      free(tmp);
      break;
    }

    free(tmp);
  }

  if (*guard == BLOCK_GUARD_NONE) {
    error_index__ = index + 1;
    error_type__ = PARSE_ERROR_UNEXPECTED_TOKEN;

    goto Error;
  }

  if (DA_AT(*tokens, index + 2).view[0] != ']') {
    error_index__ = index + 2;
    error_type__ = PARSE_ERROR_UNEXPECTED_TOKEN;

    goto Error;
  }

  return PARSE_ERROR_NONE;

Error:
  if (error_index != NULL) {
    *error_index = error_index__;
  }

  return error_type__;
}

static inline uint64_t get_line_length(const struct Tokens *tokens,
                                       uint64_t index) {
  uint64_t line_length = 0;
  for (uint64_t i = index; i < tokens->count; i++) {
    if (DA_AT(*tokens, i).view[0] == '\n' || DA_AT(*tokens, i).view[0] == '#') {
      break;
    }

    line_length++;
  }

  return line_length;
}

static uint32_t get_uint32(const struct Tokens *tokens, uint64_t index,
                           enum ParseErrors *error_type,
                           uint64_t *error_index) {
  struct StringView *tok = &DA_AT(*tokens, index);

  char *tmp = malloc(tok->length + 1);
  assert(tmp != NULL && "Failed to allocate memory");

  memcpy(tmp, tok->view, tok->length);
  tmp[tok->length] = 0;

  errno = 0;

  char *end = NULL;
  int64_t res = strtoll(tmp, &end, 10);

  if (*end != 0) {
    free(tmp);
    *error_index = index;
    *error_type = PARSE_ERROR_UNEXPECTED_TOKEN;

    return 0;
  }

  if (res < 0 || errno == ERANGE) {
    free(tmp);
    *error_index = index;
    *error_type = PARSE_ERROR_ID_OUT_OF_RANGE;

    return 0;
  }

  free(tmp);

  return res;
}

static float get_float(const struct Tokens *tokens, uint64_t index,
                       enum ParseErrors *error_type, uint64_t *error_index) {
  struct StringView *tok = &DA_AT(*tokens, index);

  char *tmp = malloc(tok->length + 1);
  assert(tmp != NULL && "Failed to allocate memory");

  memcpy(tmp, tok->view, tok->length);
  tmp[tok->length] = 0;

  errno = 0;

  char *end = NULL;
  float res = strtof(tmp, &end);

  if (*end != 0) {
    free(tmp);
    *error_index = index;
    *error_type = PARSE_ERROR_UNEXPECTED_TOKEN;

    return NAN;
  }

  if (errno == ERANGE) {
    free(tmp);
    *error_index = index;
    *error_type = PARSE_ERROR_FLOAT_OUT_OF_RANGE;

    return NAN;
  }

  free(tmp);

  return res;
}

// 'index' points to the start of the line
static enum ParseErrors parse_vert(const struct Tokens *tokens, uint64_t index,
                                   vec2 vert, uint64_t *error_index) {
  assert(tokens != NULL);
  assert(vert != NULL);

  uint64_t error_index__ = 0;
  enum ParseErrors error_type__ = PARSE_ERROR_NONE;

  uint64_t line_length = get_line_length(tokens, index);

  assert(line_length != 0);

  if (line_length != 2) {
    error_index__ = index + line_length - 1;
    error_type__ = (line_length > 2) ? PARSE_ERROR_UNEXPECTED_TOKEN
                                     : PARSE_ERROR_EXPECTED_TOKEN;

    goto Error;
  }

  // Vert
  for (uint8_t i = 0; i < 2; i++) {
    vert[i] = get_float(tokens, index + i, &error_type__, &error_index__);

    if (error_type__ != PARSE_ERROR_NONE) {
      goto Error;
    }
  }

  return PARSE_ERROR_NONE;

Error:
  if (error_index != NULL) {
    *error_index = error_index__;
  }

  printf("Error in vertex\n");

  return error_type__;
}

// 'index' points to the start of the line
static enum ParseErrors parse_wall(const struct Tokens *tokens, uint64_t index,
                                   struct dsr_Wall *wall,
                                   uint64_t *error_index) {
  assert(tokens != NULL);
  assert(wall != NULL);

  uint64_t error_index__ = 0;
  enum ParseErrors error_type__ = PARSE_ERROR_NONE;

  uint64_t line_length = get_line_length(tokens, index);

  assert(line_length != 0);

  if (line_length != WALL_FIELDS_COUNT) {
    error_index__ = index + line_length - 1;
    error_type__ = line_length > WALL_FIELDS_COUNT
                     ? PARSE_ERROR_UNEXPECTED_TOKEN
                     : PARSE_ERROR_EXPECTED_TOKEN;
    goto Error;
  }

  // Wall vertices
  {
    for (uint8_t i = 0; i < 2; i++) {
      wall->vertices[i] =
        get_uint32(tokens, index + i, &error_type__, &error_index__);

      if (error_type__ != PARSE_ERROR_NONE) {
        goto Error;
      }
    }
  }

  // Is portal
  {
    struct StringView *tok = &DA_AT(*tokens, index + WALL_FIELD_IS_PORTAL);

    if (strncmp(tok->view, "true", tok->length) == 0) {
      wall->is_portal = true;

    } else if (strncmp(tok->view, "false", tok->length) == 0) {
      wall->is_portal = false;

    } else {
      error_index__ = index + WALL_FIELD_IS_PORTAL;
      error_type__ = PARSE_ERROR_UNEXPECTED_TOKEN;

      goto Error;
    }
  }

  wall->shared_count = 0;

  return PARSE_ERROR_NONE;

Error:
  if (error_index != NULL) {
    *error_index = error_index__;
  }

  return error_type__;
}

// 'index' points to the start of the line
static enum ParseErrors parse_sector(const struct Tokens *tokens,
                                     uint64_t index, struct dsr_Sector *sector,
                                     uint64_t *error_index, uint64_t *inc_by) {
  assert(tokens != NULL);
  assert(sector != NULL);
  assert(inc_by != NULL);

  uint64_t error_index__ = 0;
  enum ParseErrors error_type__ = PARSE_ERROR_NONE;

  uint64_t line_length = get_line_length(tokens, index);

  assert(line_length != 0);

  if (line_length < SECTOR_FIELDS_COUNT) {
    error_index__ = index + line_length - 1;
    error_type__ = PARSE_ERROR_EXPECTED_TOKEN;
    goto Error;
  }

  {
    sector->floor_height = get_float(tokens, index + SECTOR_FIELD_FLOOR_HEIGHT,
                                     &error_type__, &error_index__);

    if (error_type__ != PARSE_ERROR_NONE) {
      goto Error;
    }
  }

  {
    sector->ceil_height = get_float(tokens, index + SECTOR_FIELD_CEIL_HEIGHT,
                                    &error_type__, &error_index__);

    if (error_type__ != PARSE_ERROR_NONE) {
      goto Error;
    }
  }

  for (uint32_t i = SECTOR_FIELDS_COUNT; i < line_length; i++) {
    if (i != SECTOR_FIELDS_COUNT) {
      if (DA_AT(*tokens, index + i).view[0] == ',' &&
          DA_AT(*tokens, index + i - 1).view[0] != ',') {
        DA_APPEND(&sector->cycle_ends, sector->walls.count - 1);
        continue;
      }
    }

    uint32_t wall =
      get_uint32(tokens, index + i, &error_type__, &error_index__);

    if (error_type__ != PARSE_ERROR_NONE) {
      goto Error;
    }

    DA_APPEND(&sector->walls, wall);
  }

  if (sector->cycle_ends.count == 0) {
    DA_APPEND(&sector->cycle_ends, sector->walls.count - 1);
  }

  *inc_by = line_length;

  return PARSE_ERROR_NONE;

Error:
  if (error_index != NULL) {
    *error_index = error_index__;
  }

  return error_type__;
}

static enum ParseErrors
get_block_type(enum BlockGuard guard, enum BlockType old, enum BlockType *new) {
  //printf("Block type: %d\n", old);
  //printf("Block guard: %s\n\n", block_guards[guard]);

  switch (guard) {
    case BEGIN_WALLS: {
      if (old != BLOCK_TYPE_NONE) {
        return PARSE_ERROR_UNEXPECTED_TOKEN;
      }

      *new = BLOCK_TYPE_WALLS;
    } break;

    case BEGIN_SECTORS: {
      if (old != BLOCK_TYPE_NONE) {
        return PARSE_ERROR_UNEXPECTED_TOKEN;
      }

      *new = BLOCK_TYPE_SECTORS;
    } break;

    case BEGIN_VERTS: {
      if (old != BLOCK_TYPE_NONE) {
        return PARSE_ERROR_UNEXPECTED_TOKEN;
      }

      *new = BLOCK_TYPE_VERTS;
    } break;

    case END_VERTS:
    case END_WALLS:
    case END_SECTORS: {
      if (old == BLOCK_TYPE_NONE) {
        printf("Next error from here\n");
        return PARSE_ERROR_UNEXPECTED_TOKEN;
      }

      *new = BLOCK_TYPE_NONE;
    } break;

    default: {
      return PARSE_ERROR_UNEXPECTED_TOKEN;
    } break;
  }

  return PARSE_ERROR_NONE;
}

static enum ParseErrors parse(const struct Tokens *tokens,
                              struct dsr_Scene *scene, uint64_t *error_index) {
#ifndef RELEASE

  for (uint64_t i = 0; i < tokens->count; i++) {
    assert(DA_AT(*tokens, i).length > 0 &&
           "Should only contain non-zero length tokens");
  }

#endif

  uint64_t error_index__ = 0;
  enum ParseErrors error_type__ = PARSE_ERROR_NONE;

  bool is_comment = false;
  enum BlockType block_type = BLOCK_TYPE_NONE;

  uint64_t inc_by = 1;
  for (uint64_t i = 0; i < tokens->count; i += inc_by) {
    inc_by = 1;

    struct StringView *tok = &DA_AT(*tokens, i);

    if (tok->view[0] == '\n') {
      is_comment = false;

      continue;
    }

    if (is_comment) {
      continue;
    }

    if (strstr(special, (char[]){ tok->view[0], 0 })) {
      switch (tok->view[0]) {
        case '[': {
          enum BlockGuard guard = BLOCK_GUARD_NONE;

          {
            error_type__ = parse_block_guard(tokens, i, &guard, &error_index__);

            if (error_type__ != PARSE_ERROR_NONE) {
              printf("Error!\n");
              goto Error;
            }
          }

          {
            error_type__ = get_block_type(guard, block_type, &block_type);

            if (error_type__ != PARSE_ERROR_NONE) {
              printf("Error from header, type: %s\n",
                     parse_errors[error_type__]);
              error_index__ = i;
              goto Error;
            }
          }

          inc_by = TOKENS_IN_HEADER_GUARD;
        } break;

        case '#': {
          is_comment = true;
        } break;

        default: {
          error_index__ = i;
          error_type__ = PARSE_ERROR_UNEXPECTED_TOKEN;
          goto Error;
        }
      }

      continue;
    }

    if (block_type == BLOCK_TYPE_VERTS) {
      uint32_t vert_index = scene->vertices.count;
      DA_APPEND_NO_ASSIGN(&scene->vertices);

      vec2 *vert = &DA_AT(scene->vertices, vert_index);

      error_type__ = parse_vert(tokens, i, *vert, &error_index__);

      if (error_type__ != PARSE_ERROR_NONE) {
        goto Error;
      }

      printf("Vertex {\n"
             "  %f, %f,\n"
             "}\n\n",
             (*vert)[0], (*vert)[1]);

      inc_by = 2;
    } else if (block_type == BLOCK_TYPE_WALLS) {
      struct dsr_Wall wall = { 0 };

      error_type__ = parse_wall(tokens, i, &wall, &error_index__);

      if (error_type__ != PARSE_ERROR_NONE) {
        goto Error;
      }
      printf("wall {\n"
             "  .vertices = {\n"
             "    %lu, %lu,\n"
             "  },\n\n"
             "  .is_portal = %s,\n"
             "}\n\n",
             wall.vertices[0], wall.vertices[1],
             ((char *[]){ "false", "true" })[wall.is_portal]);

      DA_APPEND(&scene->walls, wall);

      inc_by = WALL_FIELDS_COUNT;
    } else if (block_type == BLOCK_TYPE_SECTORS) {
      struct dsr_Sector sector = { 0 };

      error_type__ = parse_sector(tokens, i, &sector, &error_index__, &inc_by);

      if (error_type__ != PARSE_ERROR_NONE) {
        goto Error;
      }
      printf("Sector {\n"

             "  .floor_height = %f,\n"
             "  .ceil_height = %f,\n\n"

             "  .walls = {\n",
             sector.floor_height, sector.ceil_height);

      for (uint32_t j = 0; j < sector.walls.count; j++) {
        printf("    %d,\n", DA_AT(sector.walls, j));
      }

      printf("  },\n");

      printf("  .cycle_ends = {\n");

      for (uint32_t j = 0; j < sector.cycle_ends.count; j++) {
        printf("    %d,\n", DA_AT(sector.cycle_ends, j));
      }

      printf("  },\n"
             "}\n\n");

      DA_APPEND(&scene->sectors, sector);

    } else {
      error_index__ = i;
      error_type__ = PARSE_ERROR_UNEXPECTED_TOKEN;
      goto Error;
    }
  }

  /*
	 * Negative indices throw errors in the parsing stage and
	 * hence, don't need to be checked here.
	 */
  for (uint32_t i = 0; i < scene->sectors.count; i++) {
    struct dsr_Sector *sector = &DA_AT(scene->sectors, i);

    for (uint32_t j = 0; j < sector->walls.count; j++) {
      if (DA_AT(sector->walls, j) >= scene->walls.count) {
        error_type__ = PARSE_ERROR_UNDEFINED_REFERENCE;
        goto Error;
      }
    }
  }

  for (uint32_t i = 0; i < scene->walls.count; i++) {
    struct dsr_Wall *wall = &DA_AT(scene->walls, i);

    for (uint8_t j = 0; j < 2; j++) {
      if (wall->vertices[j] >= scene->vertices.count) {
        error_type__ = PARSE_ERROR_UNDEFINED_REFERENCE;
        goto Error;
      }
    }
  }

  for (uint32_t i = 0; i < scene->sectors.count; i++) {
    struct dsr_Sector *sector = &DA_AT(scene->sectors, i);

    for (uint32_t j = 0; j < sector->walls.count; j++) {
      struct dsr_Wall *wall = &DA_AT(scene->walls, DA_AT(sector->walls, j));

      if (wall->shared_count >= 2) {
        error_type__ = PARSE_ERROR_INVALID_WALL;
        goto Error;
      }

      wall->shared_with[wall->shared_count++] = i;
    }
  }

  return PARSE_ERROR_NONE;

Error:
  if (error_index != NULL) {
    *error_index = error_index__;
  }

  return error_type__;
}

bool dsr_load_scene(const char *scene_path, struct dsr_Scene *scene) {
  (void)scene;

  char *scene_src = fio_read_file(scene_path);
  if (!scene_src) {
    return false;
  }

  struct Tokens toks = tokenize(scene_src);

  const int toks_per_line = 7;

  printf("Tokens {\n");
  for (uint32_t i = 0; i < toks.count; i++) {
    if (i % toks_per_line == 1 || i == 0) {
      printf("  ");
    }

    if (*DA_AT(toks, i).view == '\n') {
      printf("\"\\n\", ");
    } else {
      printf("\"%.*s\", ", (int)DA_AT(toks, i).length, DA_AT(toks, i).view);
    }

    if ((i % toks_per_line == 0 && i != 0) || i == toks.count - 1) {
      if (i != toks.count - 1)
        printf("\n");
      printf("\n");
    }
  }
  printf("}\n");
  printf("\nblock guards count: %d\n", BLOCK_GUARDS_COUNT);

  uint64_t error_index = 0;
  auto res = parse(&toks, scene, &error_index);
  printf("Parse result: %d\n", res);

  if (res != PARSE_ERROR_NONE) {
    return false;
  }

  DA_FREE(&toks);
  free(scene_src);

  return true;
}

/*
 * TODO: Set parse error messages
 */
