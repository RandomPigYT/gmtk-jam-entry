#ifndef PLUGIN_H
#define PLUGIN_H

struct PluginState {};

void plug_init(void);
void *plug_pre_reload(void);
void plug_post_reload(void *prev_state);
void plug_update(void);

#endif // PLUGIN_H
