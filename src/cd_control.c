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
    
    // Try to open CD-ROM device
    player->cdio = cdio_open("/dev/cdrom", DRIVER_LINUX);
    if (!player->cdio) {
        player->cdio = cdio_open("/dev/sr0", DRIVER_LINUX);
    }
    
    if (!player->cdio) {
        fprintf(stderr, "Failed to open CD-ROM device\n");
        return -1;
    }
    
    player->current_track = 1;
    player->disc_present = false;
    
    return 0;
}

int cd_detect_disc(cd_player_t *player) {
    if (!player->cdio) {
        return -1;
    }
    
    // Check if disc is present
    discmode_t disc_mode = cdio_get_discmode(player->cdio);
    
    if (disc_mode == CDIO_DISC_MODE_NO_INFO || disc_mode == CDIO_DISC_MODE_ERROR) {
        player->disc_present = false;
        player->num_tracks = 0;
        return 0;
    }
    
    // Get number of tracks
    track_t first_track = cdio_get_first_track_num(player->cdio);
    track_t last_track = cdio_get_last_track_num(player->cdio);
    
    if (first_track == CDIO_INVALID_TRACK || last_track == CDIO_INVALID_TRACK) {
        player->disc_present = false;
        player->num_tracks = 0;
        return 0;
    }
    
    player->disc_present = true;
    player->num_tracks = last_track - first_track + 1;
    
    // Ensure current track is valid
    if (player->current_track < 1 || player->current_track > player->num_tracks) {
        player->current_track = 1;
    }
    
    return 1; // Disc present
}

int cd_get_track_info(cd_player_t *player, int track, int *length) {
    if (!player->cdio || !player->disc_present || track < 1 || track > player->num_tracks) {
        return -1;
    }
    
    // Get track length in sectors
    lsn_t start_lsn = cdio_get_track_lsn(player->cdio, track);
    lsn_t end_lsn = cdio_get_track_last_lsn(player->cdio, track);
    
    if (start_lsn == CDIO_INVALID_LSN || end_lsn == CDIO_INVALID_LSN) {
        return -1;
    }
    
    // Convert sectors to seconds (75 sectors per second for CD audio)
    *length = (end_lsn - start_lsn + 1) / 75;
    
    return 0;
}

int cd_eject(cd_player_t *player) {
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
        player->disc_present = false;
        player->num_tracks = 0;
    }
    
    return result;
}

int cd_close_tray(cd_player_t *player) {
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
        sleep(2);
        cd_detect_disc(player);
    }
    
    return result;
}

int cd_get_track_position(cd_player_t *player, int track) {
    if (!player->cdio || !player->disc_present || track < 1 || track > player->num_tracks) {
        return -1;
    }
    
    return cdio_get_track_lsn(player->cdio, track);
}

void cd_cleanup(cd_player_t *player) {
    if (player->cdio) {
        cdio_destroy(player->cdio);
        player->cdio = NULL;
    }
    
    memset(player, 0, sizeof(cd_player_t));
}
