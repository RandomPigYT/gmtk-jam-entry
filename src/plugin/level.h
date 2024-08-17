#ifndef PLUGIN_LEVEL_H
#define PLUGIN_LEVEL_H

#include "plugin.h"

#define DEFAULT_LEVEL_WIDTH 20
#define DEFAULT_LEVEL_HEIGHT 6

#define DEFAULT_LEVEL_CELL_SIZE 25

void import_level(const char *level_path, struct plug_State *state);
void unload_levels(struct plug_State *state);

// Loads current_level. Exits if current_level < 0.
void load_level(struct plug_State *state);

// Unoads current_level. Exits if current_level < 0.
void unload_level(struct plug_State *state);

#endif // PLUGIN_LEVEL_H
