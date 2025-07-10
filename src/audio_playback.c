#include "audio_playback.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cdio/cdio.h>
#include <cdio/paranoia/paranoia.h>

#define FRAMES_PER_BUFFER 1024
#define SAMPLE_RATE 44100
#define CHANNELS 2

int audio_init(audio_player_t *player, const char *device) {
    memset(player, 0, sizeof(audio_player_t));
    
    // Store device name
    strncpy(player->device_name, device ? device : "default", 
            sizeof(player->device_name) - 1);
    
    // Open PCM device
    int err = snd_pcm_open(&player->pcm_handle, player->device_name, 
                          SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot open audio device %s: %s\n", 
                player->device_name, snd_strerror(err));
        return -1;
    }
    
    // Set hardware parameters
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    
    err = snd_pcm_hw_params_any(player->pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Cannot initialize hardware parameters: %s\n", 
                snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    // Set access type
    err = snd_pcm_hw_params_set_access(player->pcm_handle, hw_params, 
                                      SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        fprintf(stderr, "Cannot set access type: %s\n", snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    // Set sample format (16-bit signed little endian)
    err = snd_pcm_hw_params_set_format(player->pcm_handle, hw_params, 
                                      SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        fprintf(stderr, "Cannot set sample format: %s\n", snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    // Set sample rate
    unsigned int rate = SAMPLE_RATE;
    err = snd_pcm_hw_params_set_rate_near(player->pcm_handle, hw_params, 
                                         &rate, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot set sample rate: %s\n", snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    // Set number of channels
    err = snd_pcm_hw_params_set_channels(player->pcm_handle, hw_params, CHANNELS);
    if (err < 0) {
        fprintf(stderr, "Cannot set channel count: %s\n", snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    // Set buffer size
    snd_pcm_uframes_t buffer_size = FRAMES_PER_BUFFER * 4;
    err = snd_pcm_hw_params_set_buffer_size_near(player->pcm_handle, hw_params, 
                                                &buffer_size);
    if (err < 0) {
        fprintf(stderr, "Cannot set buffer size: %s\n", snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    // Apply hardware parameters
    err = snd_pcm_hw_params(player->pcm_handle, hw_params);
    if (err < 0) {
        fprintf(stderr, "Cannot set hardware parameters: %s\n", snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    // Prepare the PCM device
    err = snd_pcm_prepare(player->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Cannot prepare audio interface: %s\n", snd_strerror(err));
        audio_cleanup(player);
        return -1;
    }
    
    player->is_playing = false;
    player->is_paused = false;
    
    return 0;
}

int audio_play_track(audio_player_t *player, int track) {
    if (!player->pcm_handle) {
        return -1;
    }
    
    // This is a simplified implementation
    // In a real implementation, you would:
    // 1. Open the CD device with libcdio
    // 2. Use cdio_paranoia to read audio data
    // 3. Write the data to ALSA in a separate thread
    
    printf("Playing track %d\n", track);
    player->is_playing = true;
    player->is_paused = false;
    
    return 0;
}

int audio_pause(audio_player_t *player) {
    if (!player->pcm_handle || !player->is_playing) {
        return -1;
    }
    
    int err = snd_pcm_pause(player->pcm_handle, 1);
    if (err < 0) {
        fprintf(stderr, "Cannot pause playback: %s\n", snd_strerror(err));
        return -1;
    }
    
    player->is_paused = true;
    return 0;
}

int audio_resume(audio_player_t *player) {
    if (!player->pcm_handle || !player->is_playing || !player->is_paused) {
        return -1;
    }
    
    int err = snd_pcm_pause(player->pcm_handle, 0);
    if (err < 0) {
        fprintf(stderr, "Cannot resume playback: %s\n", snd_strerror(err));
        return -1;
    }
    
    player->is_paused = false;
    return 0;
}

int audio_stop(audio_player_t *player) {
    if (!player->pcm_handle) {
        return -1;
    }
    
    int err = snd_pcm_drop(player->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Cannot stop playback: %s\n", snd_strerror(err));
        return -1;
    }
    
    err = snd_pcm_prepare(player->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Cannot prepare audio interface: %s\n", snd_strerror(err));
        return -1;
    }
    
    player->is_playing = false;
    player->is_paused = false;
    
    return 0;
}

int audio_set_device(audio_player_t *player, const char *device) {
    if (!device) {
        return -1;
    }
    
    // Stop current playback
    if (player->is_playing) {
        audio_stop(player);
    }
    
    // Close current device
    if (player->pcm_handle) {
        snd_pcm_close(player->pcm_handle);
        player->pcm_handle = NULL;
    }
    
    // Initialize with new device
    return audio_init(player, device);
}

int audio_write_samples(audio_player_t *player, const int16_t *samples, int frames) {
    if (!player->pcm_handle || !samples) {
        return -1;
    }
    
    snd_pcm_sframes_t written = snd_pcm_writei(player->pcm_handle, samples, frames);
    if (written < 0) {
        written = snd_pcm_recover(player->pcm_handle, written, 0);
        if (written < 0) {
            fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(written));
            return -1;
        }
    }
    
    return written;
}

void audio_cleanup(audio_player_t *player) {
    if (player->is_playing) {
        audio_stop(player);
    }
    
    if (player->pcm_handle) {
        snd_pcm_close(player->pcm_handle);
        player->pcm_handle = NULL;
    }
    
    memset(player, 0, sizeof(audio_player_t));
}
