#ifndef PLUG_INTERFACE_H
#define PLUG_INTERFACE_H

#include <stdlib.h>

// enum plug_callback_mode {
//   PLUG_GLFW_WINDOW_SIZE,
//   PLUG_GLFW_NONE = -1,
// };

//typedef void (*plug_glfw_window_callback)(enum plug_callback_mode, ...);

#define PLUGIN_FUNCTIONS()          \
  X(plug_init, void, void)          \
  X(plug_pre_reload, void *, void)  \
  X(plug_post_reload, void, void *) \
  X(plug_update, void, void)

#ifdef HOT_RELOAD

#define X(name, ret, ...) extern ret (*name)(__VA_ARGS__);
PLUGIN_FUNCTIONS();

#else

#define X(name, ret, ...) ret name(__VA_ARGS__);
PLUGIN_FUNCTIONS();

#endif

#undef X

// extern void (*plug_keyboard_input)(int key, int scancode, int action, int mods);
// extern void (*plug_mouse_position_input)(double xpos, double ypos);
// extern void (*plug_mouse_button_input)(int button, int action, int mods);

#endif // PLUG_INTERFACE_H
