#ifndef AUDIO_PLAYBACK_H
#define AUDIO_PLAYBACK_H

#include <stdbool.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

// Forward declaration to avoid circular dependency
struct cd_player_t;
bool is_bluealsa_device(const char *device_name);

typedef struct {
    snd_pcm_t *pcm_handle;
    char device_name[64];
    bool is_playing;
    bool is_paused;
    
    // CD playback support
    struct cd_player_t *cd_player;  // Use struct prefix
    int current_track;
    int current_sector;
    int track_start_sector;
    int track_end_sector;
    pthread_t playback_thread;
    bool stop_playback;
    
    // Timing information
    int elapsed_seconds;
    int track_length_seconds;
} audio_player_t;

// Function declarations
int audio_init(audio_player_t *player, const char *device);
int audio_play_track(audio_player_t *player, int track);
int audio_pause(audio_player_t *player);
int audio_resume(audio_player_t *player);
int audio_stop(audio_player_t *player);
int audio_set_device(audio_player_t *player, const char *device);
int audio_write_samples(audio_player_t *player, const int16_t *samples, int frames);
int audio_play_notification(audio_player_t *player, const char *wav_file_path);
int audio_play_wav_file(const char *device_id, const char *wav_file_path);
int audio_test_device_with_notification(const char *device_id, const char *wav_file_path);
int audio_set_cd_player(audio_player_t *player, struct cd_player_t *cd_player);
int audio_get_position(audio_player_t *player, int *elapsed, int *total);
void audio_cleanup(audio_player_t *player);
int audio_validate_device(audio_player_t *player);

#endif
