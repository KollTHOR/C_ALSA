#include "cd_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>

int cd_init(cd_player_t *player) {
    memset(player, 0, sizeof(cd_player_t));
    
    printf("🔵 Initializing CD player...\n");
    
    // Try to open CD-ROM device
    player->cdio = cdio_open("/dev/cdrom", DRIVER_LINUX);
    if (!player->cdio) {
        printf("⚠️  /dev/cdrom not found, trying /dev/sr0...\n");
        player->cdio = cdio_open("/dev/sr0", DRIVER_LINUX);
    }
    
    if (!player->cdio) {
        fprintf(stderr, "❌ Failed to open CD-ROM device\n");
        return -1;
    }
    
    printf("✅ CD-ROM device opened successfully\n");
    
    player->current_track = 1;
    player->disc_present = false;
    player->is_audio_cd = false;
    strcpy(player->disc_title, "Unknown Disc");
    
    // Perform initial disc detection
    cd_detect_disc(player);
    
    return 0;
}

int cd_detect_disc(cd_player_t *player) {
    if (!player->cdio) {
        return -1;
    }
    
    printf("🔍 Detecting CD disc...\n");
    
    // Check if disc is present
    discmode_t disc_mode = cdio_get_discmode(player->cdio);
    
    printf("📀 Disc mode: %d\n", disc_mode);
    
    if (disc_mode == CDIO_DISC_MODE_NO_INFO || disc_mode == CDIO_DISC_MODE_ERROR) {
        printf("❌ No disc detected or disc error\n");
        player->disc_present = false;
        player->is_audio_cd = false;
        player->num_tracks = 0;
        return 0;
    }
    
    // Check if it's an audio CD
    if (disc_mode != CDIO_DISC_MODE_CD_DA && disc_mode != CDIO_DISC_MODE_CD_MIXED) {
        printf("⚠️  Disc is not an audio CD (mode: %d)\n", disc_mode);
        player->disc_present = true;
        player->is_audio_cd = false;
        player->num_tracks = 0;
        return 0;
    }
    
    // Get number of tracks
    track_t first_track = cdio_get_first_track_num(player->cdio);
    track_t last_track = cdio_get_last_track_num(player->cdio);
    
    printf("📊 Track range: %d to %d\n", first_track, last_track);
    
    if (first_track == CDIO_INVALID_TRACK || last_track == CDIO_INVALID_TRACK) {
        printf("❌ Invalid track information\n");
        player->disc_present = false;
        player->is_audio_cd = false;
        player->num_tracks = 0;
        return 0;
    }
    
    player->disc_present = true;
    player->is_audio_cd = true;
    player->num_tracks = last_track - first_track + 1;
    
    // Ensure current track is valid
    if (player->current_track < 1 || player->current_track > player->num_tracks) {
        player->current_track = 1;
    }
    
    printf("✅ Audio CD detected: %d tracks\n", player->num_tracks);
    
    if (player->disc_present && player->is_audio_cd) {
        // Initialize paranoia like the test script
        if (player->paranoia) {
            cdio_paranoia_free(player->paranoia);
        }
        
        cdrom_drive_t *drive = cdio_cddap_identify_cdio(player->cdio, 0, NULL);
        if (drive) {
            if (cdio_cddap_open(drive) == 0) {
                player->paranoia = cdio_paranoia_init(drive);
                if (player->paranoia) {
                    cdio_paranoia_modeset(player->paranoia, PARANOIA_MODE_FULL);
                    printf("✅ Paranoia initialized for audio extraction\n");
                }
            }
        }
    }
    
    return 1;
}

int cd_get_disc_info(cd_player_t *player) {
    if (!player->cdio || !player->disc_present) {
        return -1;
    }
    
    printf("📀 Getting disc information...\n");
    
    // Try to get CD-TEXT information
    cdtext_t *cdtext = cdio_get_cdtext(player->cdio);
    if (cdtext) {
        const char *title = cdtext_get_const(cdtext, CDTEXT_FIELD_TITLE, 0);
        if (title) {
            strncpy(player->disc_title, title, sizeof(player->disc_title) - 1);
            printf("📀 Disc title: %s\n", player->disc_title);
        }
    }
    
    // Calculate total disc time
    int total_seconds = 0;
    for (int track = 1; track <= player->num_tracks; track++) {
        int track_length;
        if (cd_get_track_info(player, track, &track_length) == 0) {
            total_seconds += track_length;
        }
    }
    
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    printf("⏱️  Total disc time: %02d:%02d\n", minutes, seconds);
    
    return 0;
}

int cd_get_track_info(cd_player_t *player, int track, int *length) {
    if (!player->cdio || !player->disc_present || !player->is_audio_cd || 
        track < 1 || track > player->num_tracks) {
        return -1;
    }
    
    // Get track length in sectors
    lsn_t start_lsn = cdio_get_track_lsn(player->cdio, track);
    lsn_t end_lsn = cdio_get_track_last_lsn(player->cdio, track);
    
    if (start_lsn == CDIO_INVALID_LSN || end_lsn == CDIO_INVALID_LSN) {
        printf("❌ Invalid LSN for track %d\n", track);
        return -1;
    }
    
    // Convert sectors to seconds (75 sectors per second for CD audio)
    *length = (end_lsn - start_lsn + 1) / 75;
    
    printf("📊 Track %d: %d seconds (%d sectors)\n", track, *length, end_lsn - start_lsn + 1);
    
    return 0;
}

int cd_read_audio_sector(cd_player_t *player, int track, int sector, int16_t *buffer) {
    (void)track;   // Suppress unused parameter warning
    (void)sector;  // Suppress unused parameter warning
    
    if (!player->paranoia || !player->disc_present || !player->is_audio_cd) {
        return -1;
    }
    
    // Direct paranoia read (like your working test script)
    int16_t *audio_data = (int16_t *)cdio_paranoia_read(player->paranoia, NULL);
    if (!audio_data) {
        printf("❌ Failed to read sector from CD\n");
        return -1;
    }
    
    // Copy the audio data to the buffer
    int frames_per_sector = CDIO_CD_FRAMESIZE_RAW / sizeof(int16_t);
    memcpy(buffer, audio_data, CDIO_CD_FRAMESIZE_RAW);
    
    return frames_per_sector;
}

int cd_eject(cd_player_t *player) {
    printf("⏏️  Ejecting CD...\n");
    
    int fd = open("/dev/cdrom", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fd = open("/dev/sr0", O_RDONLY | O_NONBLOCK);
    }
    
    if (fd < 0) {
        perror("Failed to open CD-ROM for eject");
        return -1;
    }
    
    int result = ioctl(fd, CDROMEJECT);
    close(fd);
    
    if (result == 0) {
        printf("✅ CD ejected successfully\n");
        player->disc_present = false;
        player->is_audio_cd = false;
        player->num_tracks = 0;
        
        // Clean up paranoia
        if (player->paranoia) {
            cdio_paranoia_free(player->paranoia);
            player->paranoia = NULL;
        }
    } else {
        printf("❌ Failed to eject CD\n");
    }
    
    return result;
}

int cd_close_tray(cd_player_t *player) {
    printf("📥 Closing CD tray...\n");
    
    int fd = open("/dev/cdrom", O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        fd = open("/dev/sr0", O_RDONLY | O_NONBLOCK);
    }
    
    if (fd < 0) {
        perror("Failed to open CD-ROM for close");
        return -1;
    }
    
    int result = ioctl(fd, CDROMCLOSETRAY);
    close(fd);
    
    // Wait a bit for the tray to close and disc to be detected
    if (result == 0) {
        printf("✅ CD tray closed\n");
        sleep(3); // Increased wait time
        cd_detect_disc(player);
    } else {
        printf("❌ Failed to close CD tray\n");
    }
    
    return result;
}

int cd_get_track_position(cd_player_t *player, int track) {
    if (!player->cdio || !player->disc_present || !player->is_audio_cd || 
        track < 1 || track > player->num_tracks) {
        return -1;
    }
    
    return cdio_get_track_lsn(player->cdio, track);
}

void cd_cleanup(cd_player_t *player) {
    printf("🧹 Cleaning up CD player...\n");
    
    if (player->paranoia) {
        cdio_paranoia_free(player->paranoia);
        player->paranoia = NULL;
    }
    
    if (player->cdio) {
        cdio_destroy(player->cdio);
        player->cdio = NULL;
    }
    
    memset(player, 0, sizeof(cd_player_t));
    printf("✅ CD player cleanup completed\n");
}
