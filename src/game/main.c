#include "raylib/src/raylib.h"
#include "plugin-interface.h"
#include "hotreload.h"

#ifndef PLATFORM_WEB
#include <stdio.h>
#endif

#define WIDTH 16
#define HEIGHT 9
#define FACTOR 60

void raylib_js_set_entry(void (*entry)(void));

void game_frame() {
  BeginDrawing();

  ClearBackground(CLITERAL(Color){
    GetRandomValue(0, 255),
    GetRandomValue(0, 255),
    GetRandomValue(0, 255),
    255,
  });

  EndDrawing();
}

int main(void) {
  InitWindow(WIDTH * FACTOR, HEIGHT * FACTOR, "Game");
  SetTargetFPS(60);
  hotreload_load_plug();
  plug_init();

#ifdef PLATFORM_WEB

  raylib_js_set_entry(plug_update);

#else
  SetRandomSeed(69);

  while (!WindowShouldClose()) {
    plug_update();

#ifdef HOT_RELOAD
    if (IsKeyDown(KEY_LEFT_CONTROL) && IsKeyDown(KEY_R)) {
      void *state = plug_pre_reload();
      hotreload_load_plug();
      plug_post_reload(state);
    }
#endif
  }

#endif

  return 0;
}
