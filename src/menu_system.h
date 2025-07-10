#ifndef MENU_SYSTEM_H
#define MENU_SYSTEM_H

#include <stdbool.h>
#include "lcd_display.h"
#include "cd_control.h"
#include "audio_playback.h"
#include "bluetooth_manager.h"
#include "button_input.h"

#define MAX_AUDIO_DEVICES 10
#define MAX_BT_DEVICES 10

typedef enum {
    MENU_MAIN = 0,
    MENU_PLAYBACK,
    MENU_AUDIO_OUTPUT,
    MENU_AUDIO_DEVICE_LIST,
    MENU_BLUETOOTH,
    MENU_BT_DEVICE_LIST,
    MENU_CD_INFO
} menu_state_t;

typedef enum {
    PLAYBACK_STOPPED = 0,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED
} playback_state_t;

typedef struct {
    char name[64];
    char device_id[64];
    bool is_bluetooth;
    bool is_available;
} audio_device_info_t;

typedef struct {
    char name[64];
    char address[18];
    bool is_paired;
    bool is_connected;
} bt_device_info_t;

typedef struct {
    menu_state_t current_menu;
    int menu_selection;
    int max_selections;
    
    playback_state_t playback_state;
    int current_track;
    int elapsed_time;
    int track_length;
    
    bool use_bluetooth;
    char current_audio_device[64];
    
    // Audio device management
    audio_device_info_t audio_devices[MAX_AUDIO_DEVICES];
    int num_audio_devices;
    int selected_audio_device;
    
    // Bluetooth device management
    bt_device_info_t bt_devices[MAX_BT_DEVICES];
    int num_bt_devices;
    int selected_bt_device;
    bool bt_scanning;
    
    // Component references
    lcd_t *lcd;
    cd_player_t *cd_player;
    audio_player_t *audio_player;
    bluetooth_manager_t *bluetooth_manager;
} menu_system_t;

// Function declarations
int menu_init(menu_system_t *menu, lcd_t *lcd, cd_player_t *cd_player, 
              audio_player_t *audio_player, bluetooth_manager_t *bluetooth_manager);
void menu_handle_button(menu_system_t *menu, button_event_t event);
void menu_update_display(menu_system_t *menu);
void menu_update_playback_info(menu_system_t *menu);
void menu_cleanup(menu_system_t *menu);

// Device management functions
int menu_scan_audio_devices(menu_system_t *menu);
int menu_scan_bt_devices(menu_system_t *menu);

#endif
