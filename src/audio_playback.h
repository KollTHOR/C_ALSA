#ifndef AUDIO_PLAYBACK_H
#define AUDIO_PLAYBACK_H

#include <alsa/asoundlib.h>
#include <stdbool.h>

typedef struct {
    snd_pcm_t *pcm_handle;
    char device_name[64];
    bool is_playing;
    bool is_paused;
} audio_player_t;

// Function declarations
int audio_init(audio_player_t *player, const char *device);
int audio_play_track(audio_player_t *player, int track);
int audio_pause(audio_player_t *player);
int audio_resume(audio_player_t *player);
int audio_stop(audio_player_t *player);
int audio_set_device(audio_player_t *player, const char *device);
void audio_cleanup(audio_player_t *player);

#endif
