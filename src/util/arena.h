#ifndef UTIL_ARENA_H
#define UTIL_ARENA_H

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#define arena_save(arena, r)      \
  do {                            \
    (*(r)) = (arena).base_offset; \
  } while (0)

#define arena_restore(arena, r) \
  do {                          \
    (arena)->base_offset = (r); \
  } while (0)

typedef size_t ArenaSaveState;

struct Arena {
  void *block;
  size_t base_offset;
  size_t size;
};

struct Arena arena_create(size_t size);
void *arena_alloc(struct Arena *arena, size_t size);
void arena_free(struct Arena *arena);
void arena_resize(struct Arena *arena, size_t size);

#ifdef UTIL_ARENA_H_IMPLEMENTATION

struct Arena arena_create(size_t size) {
  struct Arena a = { 0 };

  a.block = malloc(size);
  assert(a.block != NULL && "Failed to allocate memory");

  a.size = size;
  a.base_offset = 0;

  return a;
}

void *arena_alloc(struct Arena *arena, size_t size) {
  assert(arena->block != NULL && "Tried to allocate to NULL arena");

  void *allocated = (char *)arena->block + arena->base_offset;

  size_t tmp = arena->base_offset + size;
  if (tmp >= arena->size) {
    return NULL;
  }
  arena->base_offset = tmp;

  return allocated;
}

void arena_resize(struct Arena *arena, size_t size) {
  arena->block = realloc(arena->block, size);
  assert(arena->block != NULL && "Failed to allocate memory");

  arena->size = size;
}

void arena_free(struct Arena *arena) {
  free(arena->block);
  arena->base_offset = 0;
  arena->size = 0;
}

#endif // UTIL_ARENA_H_IMPLEMENTATION
#endif // UTIL_ARENA_H
