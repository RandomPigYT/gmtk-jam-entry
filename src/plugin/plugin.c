#include "plugin.h"
#include "raylib/src/raylib.h"

#ifndef PLATFORM_WEB
#include <stdio.h>
#endif

void foo(void) {
#ifndef PLATFORM_WEB
  printf("Hi from plugin!\n");
#endif

  ClearBackground(BLUE);
}
