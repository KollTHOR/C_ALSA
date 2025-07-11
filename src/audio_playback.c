#include "audio_playback.h"
#include "cd_control.h"          // For cd_player_t and CD functions
#include <stdio.h>               // For printf, fprintf, stderr
#include <stdlib.h>              // For standard library functions
#include <string.h>              // For string functions
#include <stdbool.h>             // For bool, true, false
#include <unistd.h>              // For usleep
#include <pthread.h>             // For threading
#include <signal.h> 
#include <math.h>              // For signal handling
#include <alsa/asoundlib.h>      // For ALSA audio types and functions
#include <cdio/cdio.h>           // For CD-ROM constants and types
#include <cdio/cd_types.h>       // For CD types like lsn_t
#include <cdio/paranoia/paranoia.h>  // For CD audio reading

bool is_bluealsa_device(const char *device_name);

// CD audio playback thread
void* cd_playback_thread(void* arg) {
    audio_player_t *player = (audio_player_t*)arg;
    
    printf("🎵 CD playback thread started\n");
    
    // Use the working script's approach
    long start = player->track_start_sector;
    long end = player->track_end_sector;
    long total_sectors = end - start;
    
    // Seek to track start (like working script)
    cdio_paranoia_seek(player->cd_player->paranoia, start, SEEK_SET);
    
    for (long i = 0; i < total_sectors && !player->stop_playback && player->is_playing; i++) {
        if (player->is_paused) {
            usleep(100000);
            continue;
        }
        
        // Direct paranoia read (like working script)
        int16_t *audio_data = (int16_t *)cdio_paranoia_read(player->cd_player->paranoia, NULL);
        if (!audio_data) {
            printf("❌ Failed to read sector from CD\n");
            continue; // Skip this sector, don't fail
        }
        
        // Calculate frames (like working script)
        int frames_per_sector = CDIO_CD_FRAMESIZE_RAW / 4;
        
        // Simple write with basic recovery (like working script)
        int err = snd_pcm_writei(player->pcm_handle, audio_data, frames_per_sector);
        if (err < 0) {
            printf("🔧 Recovering from ALSA error...\n");
            snd_pcm_recover(player->pcm_handle, err, 0);
            // Continue to next sector - don't retry or fail
        } else {
            // Update progress
            player->elapsed_seconds = (i + 1) / 75;
            
            if ((i + 1) % 75 == 0) {
                printf("⏱️  Playing: %d:%02d (sector %ld/%ld)\n", 
                       player->elapsed_seconds / 60, player->elapsed_seconds % 60,
                       i + 1, total_sectors);
            }
        }
        
        // No artificial timing - let ALSA handle it naturally
    }
    
    printf("🎵 CD playback thread ended\n");
    return NULL;
}





int audio_play_wav_file(const char *device_id, const char *wav_file_path) {
    if (!device_id || !wav_file_path) {
        return -1;
    }
    
    printf("🎵 Playing WAV file on device: %s\n", device_id);
    
    // Create temporary audio player for notification
    audio_player_t temp_player = {0};
    
    // Initialize audio device
    if (audio_init(&temp_player, device_id) != 0) {
        printf("❌ Failed to initialize audio device for notification\n");
        return -1;
    }
    
    // Play the notification
    int result = audio_play_notification(&temp_player, wav_file_path);
    
    // Cleanup
    audio_cleanup(&temp_player);
    
    return result;
}

int audio_play_notification(audio_player_t *player, const char *wav_file_path) {
    if (!player->pcm_handle || !wav_file_path) {
        return -1;
    }
    
    printf("🔊 Playing notification sound: %s\n", wav_file_path);
    
    // Open WAV file
    FILE *wav_file = fopen(wav_file_path, "rb");
    if (!wav_file) {
        printf("❌ Failed to open WAV file: %s\n", wav_file_path);
        return -1;
    }
    
    // Skip WAV header (44 bytes for standard WAV)
    fseek(wav_file, 44, SEEK_SET);
    
    // Read and play audio data in chunks
    int16_t buffer[1024];
    size_t bytes_read;
    int total_frames = 0;
    
    while ((bytes_read = fread(buffer, sizeof(int16_t), 1024, wav_file)) > 0) {
        // Convert bytes to frames (2 bytes per sample, 2 channels)
        int frames = bytes_read / 2; // 2 channels
        
        snd_pcm_sframes_t frames_written = audio_write_samples(player, buffer, frames);
        
        if (frames_written < 0) {
            printf("❌ Failed to write audio data\n");
            break;
        }
        
        total_frames += frames_written;
    }
    
    // Ensure all audio is played
    snd_pcm_drain(player->pcm_handle);
    
    fclose(wav_file);
    printf("✅ Notification sound completed (%d frames)\n", total_frames);
    return 0;
}

bool is_bluealsa_device(const char *device_name) {
    if (!device_name) {
        return false;
    }
    
    // Check for BlueALSA device string patterns (case insensitive)
    if (strcasestr(device_name, "bluealsa:") != NULL) {
        return true;
    }
    
    // Check for BlueALSA PCM device patterns
    if (strcasestr(device_name, "bluealsa") != NULL && 
        (strstr(device_name, "DEV=") != NULL || strstr(device_name, "PROFILE=") != NULL)) {
        return true;
    }
    
    // Additional validation: check if device actually exists in BlueALSA
    if (strstr(device_name, "bluealsa") != NULL) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "bluealsa-aplay -l 2>/dev/null | grep -q '%s'", device_name);
        return (system(cmd) == 0);
    }
    
    return false;
}



int audio_init(audio_player_t *player, const char *device) {
    memset(player, 0, sizeof(audio_player_t));
    
    // Store device name
    snprintf(player->device_name, sizeof(player->device_name), "%s", device ? device : "default");
    
    printf("🎵 Initializing audio device: %s\n", player->device_name);
    
    // Simple PCM device opening (like your test script)
    int err = snd_pcm_open(&player->pcm_handle, player->device_name, 
                          SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        fprintf(stderr, "❌ Failed to open PCM device: %s\n", snd_strerror(err));
        return -1;
    }
    
    // Set hardware parameters (simplified like test script)
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    
    snd_pcm_hw_params_any(player->pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(player->pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(player->pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(player->pcm_handle, hw_params, 2);
    
    unsigned int rate = 44100;
    snd_pcm_hw_params_set_rate_near(player->pcm_handle, hw_params, &rate, 0);
    snd_pcm_hw_params(player->pcm_handle, hw_params);
    
    printf("✅ Sample rate set to: %u Hz\n", rate);
    
    // Simple pre-buffering for Bluetooth stability (like test script)
    bool is_bluetooth = (strstr(player->device_name, "bluealsa") != NULL);
    if (is_bluetooth) {
        printf("🔧 Pre-buffering for Bluetooth stability...\n");
        usleep(2000000); // 2 seconds sleep (exactly like test script)
    }
    
    player->is_playing = false;
    player->is_paused = false;
    
    printf("✅ Audio device initialized successfully\n");
    return 0;
}



int audio_write_samples(audio_player_t *player, const int16_t *samples, int frames) {
    if (!player->pcm_handle || !samples) {
        return -1;
    }
    
    // ✅ FIRST: Actually write the audio data
    snd_pcm_sframes_t written = snd_pcm_writei(player->pcm_handle, samples, frames);
    
    // ✅ THEN: Check for errors and recover (like your working script)
    if (written < 0) {
        printf("🔧 Recovering from ALSA error...\n");
        snd_pcm_recover(player->pcm_handle, written, 0);
        // Just return 0 and continue - don't retry (this prevents I/O errors)
        return 0;
    }
    
    return written;
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

int audio_test_device_with_notification(const char *device_id, const char *wav_file_path) {
    printf("🧪 Testing Bluetooth audio device with notification\n");
    printf("📱 Device: %s\n", device_id);
    printf("🔊 Sound file: %s\n", wav_file_path);
    
    // Test if the device is accessible
    snd_pcm_t *test_handle;
    int err = snd_pcm_open(&test_handle, device_id, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    
    if (err < 0) {
        printf("❌ Cannot access audio device %s: %s\n", device_id, snd_strerror(err));
        return -1;
    }
    
    snd_pcm_close(test_handle);
    printf("✅ Audio device is accessible\n");
    
    // Play notification sound
    return audio_play_wav_file(device_id, wav_file_path);
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


int audio_play_track(audio_player_t *player, int track) {
    if (!player->pcm_handle || !player->cd_player) {
        return -1;
    }
    
    printf("🎵 Starting playback of track %d\n", track);
    printf("📱 Using audio device: %s\n", player->device_name);
    
    // Verify the audio device is still valid and matches current selection
    snd_pcm_state_t state = snd_pcm_state(player->pcm_handle);
    if (state == SND_PCM_STATE_DISCONNECTED) {
        printf("⚠️  Audio device disconnected, reinitializing...\n");
        
        // Reinitialize with current device
        char current_device[64];
        snprintf(current_device, sizeof(current_device), "%s", player->device_name);
        
        if (player->pcm_handle) {
            snd_pcm_close(player->pcm_handle);
            player->pcm_handle = NULL;
        }
        
        if (audio_init(player, current_device) != 0) {
            printf("❌ Failed to reinitialize audio device\n");
            return -1;
        }
    }
    
    // Stop any existing playback
    if (player->is_playing) {
        audio_stop(player);
    }
    
    // Get track information
    int track_length;
    if (cd_get_track_info(player->cd_player, track, &track_length) != 0) {
        printf("❌ Failed to get track %d information\n", track);
        return -1;
    }
    
    // Get track sector positions
    player->track_start_sector = cd_get_track_position(player->cd_player, track);
    if (player->track_start_sector < 0) {
        printf("❌ Failed to get track %d start position\n", track);
        return -1;
    }
    
    // Calculate end sector
    lsn_t end_lsn = cdio_get_track_last_lsn(player->cd_player->cdio, track);
    if (end_lsn == CDIO_INVALID_LSN) {
        printf("❌ Failed to get track %d end position\n", track);
        return -1;
    }
    player->track_end_sector = end_lsn;
    
    // Initialize playback state
    player->current_track = track;
    player->current_sector = player->track_start_sector;
    player->elapsed_seconds = 0;
    player->track_length_seconds = track_length;
    player->is_playing = true;
    player->is_paused = false;
    player->stop_playback = false;
    
    printf("📊 Track %d: sectors %d to %d (%d seconds)\n", 
           track, player->track_start_sector, player->track_end_sector, track_length);
    
    // Prepare PCM device for playback
    int err = snd_pcm_prepare(player->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Cannot prepare audio interface: %s\n", snd_strerror(err));
        return -1;
    }
    
    // Start playback thread
    if (pthread_create(&player->playback_thread, NULL, cd_playback_thread, player) != 0) {
        printf("❌ Failed to create playback thread\n");
        return -1;
    }
    
    printf("✅ CD playback started successfully\n");
    return 0;
}

int audio_validate_device(audio_player_t *player) {
    if (!player->pcm_handle) {
        printf("❌ No audio device initialized\n");
        return -1;
    }
    
    // Test if device is accessible
    snd_pcm_state_t state = snd_pcm_state(player->pcm_handle);
    printf("🔍 Audio device state: %d\n", state);
    
    switch (state) {
        case SND_PCM_STATE_OPEN:
        case SND_PCM_STATE_SETUP:
        case SND_PCM_STATE_PREPARED:
        case SND_PCM_STATE_RUNNING:
        case SND_PCM_STATE_PAUSED:
            printf("✅ Audio device is ready\n");
            return 0;
            
        case SND_PCM_STATE_DISCONNECTED:
            printf("❌ Audio device disconnected\n");
            return -1;
            
        default:
            printf("⚠️  Audio device in unknown state\n");
            return -1;
    }
}

int audio_pause(audio_player_t *player) {
    if (!player->pcm_handle || !player->is_playing) {
        return -1;
    }
    
    if (player->is_paused) {
        return 0; // Already paused
    }
    
    printf("⏸️  Pausing playback\n");
    
    // Check PCM state before pausing
    snd_pcm_state_t state = snd_pcm_state(player->pcm_handle);
    printf("🔍 PCM state before pause: %d\n", state);
    
    if (state == SND_PCM_STATE_RUNNING) {
        int err = snd_pcm_pause(player->pcm_handle, 1);
        if (err < 0) {
            fprintf(stderr, "Cannot pause playback: %s\n", snd_strerror(err));
            
            // Fallback: use drop instead of pause
            printf("🔄 Trying alternative pause method...\n");
            err = snd_pcm_drop(player->pcm_handle);
            if (err < 0) {
                fprintf(stderr, "Cannot drop playback: %s\n", snd_strerror(err));
                return -1;
            }
        }
    }
    
    player->is_paused = true;
    return 0;
}

int audio_resume(audio_player_t *player) {
    if (!player->pcm_handle || !player->is_playing || !player->is_paused) {
        return -1;
    }
    
    printf("▶️  Resuming playback\n");
    
    // Check PCM state before resuming
    snd_pcm_state_t state = snd_pcm_state(player->pcm_handle);
    printf("🔍 PCM state before resume: %d\n", state);
    
    if (state == SND_PCM_STATE_PAUSED) {
        int err = snd_pcm_pause(player->pcm_handle, 0);
        if (err < 0) {
            fprintf(stderr, "Cannot resume playback: %s\n", snd_strerror(err));
            
            // Fallback: prepare and restart
            printf("🔄 Trying alternative resume method...\n");
            err = snd_pcm_prepare(player->pcm_handle);
            if (err < 0) {
                fprintf(stderr, "Cannot prepare for resume: %s\n", snd_strerror(err));
                return -1;
            }
        }
    } else if (state == SND_PCM_STATE_SETUP || state == SND_PCM_STATE_PREPARED) {
        // Device was dropped, just prepare it
        int err = snd_pcm_prepare(player->pcm_handle);
        if (err < 0) {
            fprintf(stderr, "Cannot prepare for resume: %s\n", snd_strerror(err));
            return -1;
        }
    }
    
    player->is_paused = false;
    return 0;
}

int audio_stop(audio_player_t *player) {
    if (!player->pcm_handle) {
        return -1;
    }
    
    printf("⏹️  Stopping playback\n");
    
    // Signal thread to stop
    player->stop_playback = true;
    player->is_playing = false;
    player->is_paused = false;
    
    // Wait for playback thread to finish
    if (player->playback_thread) {
        pthread_join(player->playback_thread, NULL);
        player->playback_thread = 0;
    }
    
    // Stop and drain the PCM device
    int err = snd_pcm_drop(player->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Cannot stop playback: %s\n", snd_strerror(err));
    }
    
    err = snd_pcm_prepare(player->pcm_handle);
    if (err < 0) {
        fprintf(stderr, "Cannot prepare audio interface: %s\n", snd_strerror(err));
    }
    
    // Reset timing
    player->elapsed_seconds = 0;
    player->current_sector = 0;
    
    printf("✅ Playback stopped\n");
    return 0;
}

// Add function to get current playback time
int audio_get_position(audio_player_t *player, int *elapsed, int *total) {
    if (!player) {
        return -1;
    }
    
    *elapsed = player->elapsed_seconds;
    *total = player->track_length_seconds;
    
    return 0;
}

int audio_set_cd_player(audio_player_t *player, cd_player_t *cd_player) {
    if (!player) {
        return -1;
    }
    
    player->cd_player = cd_player;
    printf("✅ CD player reference set in audio player\n");
    return 0;
}
