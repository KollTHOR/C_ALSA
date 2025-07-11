#ifndef CD_CONTROL_H
#define CD_CONTROL_H

#include <stdbool.h>
#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/paranoia/paranoia.h>

typedef struct cd_player_t {
    CdIo_t *cdio;
    cdrom_paranoia_t *paranoia;  // Ensure this member exists
    int num_tracks;
    int current_track;
    bool disc_present;
    bool is_audio_cd;
    char disc_title[256];
} cd_player_t;

// Function declarations
int cd_init(cd_player_t *player);
int cd_detect_disc(cd_player_t *player);
int cd_get_track_info(cd_player_t *player, int track, int *length);
int cd_get_disc_info(cd_player_t *player);
int cd_read_audio_sector(cd_player_t *player, int track, int sector, int16_t *buffer);
int cd_eject(cd_player_t *player);
int cd_close_tray(cd_player_t *player);
int cd_get_track_position(cd_player_t *player, int track);
void cd_cleanup(cd_player_t *player);

#endif
