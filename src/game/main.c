#include "plugin/plugin.h"
#include "raylib/src/raylib.h"

#ifndef PLATFORM_WEB
#include <stdio.h>
#endif

#define WIDTH 16
#define HEIGHT 9
#define FACTOR 60

void raylib_js_set_entry(void (*entry)(void));

void game_frame() {
  BeginDrawing();

  foo();

  EndDrawing();
}

int main(void) {
  InitWindow(WIDTH * FACTOR, HEIGHT * FACTOR, "Game");
  SetTargetFPS(60);

#ifdef PLATFORM_WEB

  raylib_js_set_entry(game_frame);

#else

  while (!WindowShouldClose()) {
    game_frame();
  }

#endif

  return 0;
}
