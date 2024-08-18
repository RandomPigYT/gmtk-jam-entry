#include "raylib/src/raylib.h"
#include "plugin-interface.h"
#include "hotreload.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef PLATFORM_WEB

#include <emscripten/emscripten.h>

#endif

#define WIDTH 16
#define HEIGHT 9
#define FACTOR 60

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
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);

  InitWindow(WIDTH * FACTOR, HEIGHT * FACTOR, "Game");
  SetTargetFPS(120);
  hotreload_load_plug();
  plug_init();

#ifdef PLATFORM_WEB

  emscripten_set_main_loop(plug_update, 0, 1);

#endif

#ifndef PLATFORM_WEB
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
