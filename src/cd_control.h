#ifndef CD_CONTROL_H
#define CD_CONTROL_H

#include <cdio/cdio.h>
#include <cdio/cd_types.h>

typedef struct {
    CdIo_t *cdio;
    int num_tracks;
    int current_track;
    bool disc_present;
} cd_player_t;

// Function declarations
int cd_init(cd_player_t *player);
int cd_detect_disc(cd_player_t *player);
int cd_get_track_info(cd_player_t *player, int track, int *length);
int cd_eject(cd_player_t *player);
void cd_cleanup(cd_player_t *player);

#endif
