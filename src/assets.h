#ifndef ASSETS_H
#define ASSETS_H

#include <stdbool.h>
#include <stddef.h>

// Asset paths
#define ASSETS_BASE_DIR "./assets"
#define SOUNDS_DIR ASSETS_BASE_DIR "/sounds"

#define BT_CONNECT_SOUND SOUNDS_DIR "/bt_connect.wav"
#define BT_DISCONNECT_SOUND SOUNDS_DIR "/bt_disconnect.wav"
#define ERROR_SOUND SOUNDS_DIR "/error_beep.wav"

// Asset management functions
int assets_init(void);
bool assets_file_exists(const char *asset_path);
int assets_get_full_path(const char *asset_path, char *full_path, size_t path_size);
void assets_list_available(void);

#endif
