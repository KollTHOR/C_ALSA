#include "menu_system.h"
#include "button_input.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

static const char* main_menu_items[] = {
    "Play CD",
    "Audio Output",
    "Bluetooth",
    "CD Info",
    "Eject CD"
};

static const char* audio_output_items[] = {
    "Select Device",
    "Refresh List",
    "Back"
};

static const char* bluetooth_items[] = {
    "Scan New Devices",
    "Device List",
    "Disconnect",
    "Back"
};

static void scan_bluetooth_audio_devices(menu_system_t *menu);

int menu_init(menu_system_t *menu, lcd_t *lcd, cd_player_t *cd_player, 
              audio_player_t *audio_player, bluetooth_manager_t *bluetooth_manager) {
    memset(menu, 0, sizeof(menu_system_t));
    
    menu->lcd = lcd;
    menu->cd_player = cd_player;
    menu->audio_player = audio_player;
    menu->bluetooth_manager = bluetooth_manager;
    
    menu->current_menu = MENU_MAIN;
    menu->menu_selection = 0;
    menu->max_selections = 5;
    menu->playback_state = PLAYBACK_STOPPED;
    menu->current_track = 1;
    menu->use_bluetooth = false;
    strcpy(menu->current_audio_device, "default");
    
    // Initialize device lists
    menu_scan_audio_devices(menu);
    
    menu_update_display(menu);
    return 0;
}

int menu_scan_audio_devices(menu_system_t *menu) {
    printf("Scanning all audio devices...\n");
    menu->num_audio_devices = 0;
    
    // Add default wired audio devices
    strcpy(menu->audio_devices[0].name, "Built-in Audio");
    strcpy(menu->audio_devices[0].device_id, "hw:0,0");
    menu->audio_devices[0].is_bluetooth = false;
    menu->audio_devices[0].is_available = true;
    menu->num_audio_devices++;
    
    strcpy(menu->audio_devices[1].name, "HDMI Audio");
    strcpy(menu->audio_devices[1].device_id, "hw:1,0");
    menu->audio_devices[1].is_bluetooth = false;
    menu->audio_devices[1].is_available = true;
    menu->num_audio_devices++;
    
    // Scan for connected Bluetooth audio devices
    scan_bluetooth_audio_devices(menu);
    
    printf("Total audio devices found: %d\n", menu->num_audio_devices);
    return menu->num_audio_devices;
}



static void scan_bluetooth_audio_devices(menu_system_t *menu) {
    printf("Scanning for Bluetooth audio devices...\n");
    
    // Parse bluealsa-aplay -l output
    FILE *fp = popen("bluealsa-aplay -l 2>/dev/null", "r");
    if (!fp) {
        printf("Failed to run bluealsa-aplay\n");
        return;
    }
    
    char line[512];
    bool in_playback_section = false;
    
    while (fgets(line, sizeof(line), fp) && menu->num_audio_devices < MAX_AUDIO_DEVICES) {
        // Check for section headers
        if (strstr(line, "**** List of PLAYBACK Bluetooth Devices ****")) {
            in_playback_section = true;
            continue;
        }
        if (strstr(line, "**** List of CAPTURE Bluetooth Devices ****")) {
            in_playback_section = false;
            continue;
        }
        
        // Only process playback devices
        if (!in_playback_section) {
            continue;
        }
        
        // Parse device line: "hci0: FC:E8:06:6E:1C:38 [FC-E8-06-6E-1C-38], trusted audio-headset"
        if (strncmp(line, "hci", 3) == 0) {
            char hci[16], mac[18], mac_brackets[32], device_name[128];
            
            // Parse the line format: "hci0: MAC [MAC-FORMAT], device name"
            if (sscanf(line, "%15[^:]: %17s [%31[^]]], %127[^\n]", 
                      hci, mac, mac_brackets, device_name) == 4) {
                
                printf("Found BlueALSA device: %s (%s)\n", device_name, mac);
                
                // Truncate name if too long and add BT prefix
                char truncated_name[60];
                strncpy(truncated_name, device_name, sizeof(truncated_name) - 1);
                truncated_name[sizeof(truncated_name) - 1] = '\0';
                
                snprintf(menu->audio_devices[menu->num_audio_devices].name, 
                        sizeof(menu->audio_devices[menu->num_audio_devices].name),
                        "BT: %s", truncated_name);
                
                // Create BlueALSA device string
                snprintf(menu->audio_devices[menu->num_audio_devices].device_id,
                        sizeof(menu->audio_devices[menu->num_audio_devices].device_id),
                        "bluealsa:DEV=%s,PROFILE=a2dp", mac);
                
                menu->audio_devices[menu->num_audio_devices].is_bluetooth = true;
                menu->audio_devices[menu->num_audio_devices].is_available = true;
                menu->num_audio_devices++;
                
                printf("Added to audio device list: BT: %s\n", truncated_name);
            }
        }
    }
    pclose(fp);
    
    printf("Bluetooth audio scan complete. Total devices: %d\n", menu->num_audio_devices);
}



int menu_scan_bt_devices(menu_system_t *menu) {
    menu->bt_scanning = true;
    
    // Use the improved bluetooth manager scanning
    int result = bluetooth_scan_devices(menu->bluetooth_manager);
    
    // Copy devices from bluetooth manager to menu system
    menu->num_bt_devices = 0;
    for (int i = 0; i < menu->bluetooth_manager->num_devices && i < MAX_BT_DEVICES; i++) {
        strcpy(menu->bt_devices[i].name, menu->bluetooth_manager->devices[i].name);
        strcpy(menu->bt_devices[i].address, menu->bluetooth_manager->devices[i].address);
        menu->bt_devices[i].is_paired = menu->bluetooth_manager->devices[i].is_paired;
        menu->bt_devices[i].is_connected = menu->bluetooth_manager->devices[i].is_connected;
        menu->num_bt_devices++;
    }
    
    menu->bt_scanning = false;
    return result;
}


static int menu_load_known_bt_devices(menu_system_t *menu) {
    // Load paired and connected devices without scanning
    int result = bluetooth_load_paired_devices(menu->bluetooth_manager);
    
    // Copy devices from bluetooth manager to menu system
    menu->num_bt_devices = 0;
    for (int i = 0; i < menu->bluetooth_manager->num_devices && i < MAX_BT_DEVICES; i++) {
        strcpy(menu->bt_devices[i].name, menu->bluetooth_manager->devices[i].name);
        strcpy(menu->bt_devices[i].address, menu->bluetooth_manager->devices[i].address);
        menu->bt_devices[i].is_paired = menu->bluetooth_manager->devices[i].is_paired;
        menu->bt_devices[i].is_connected = menu->bluetooth_manager->devices[i].is_connected;
        menu->num_bt_devices++;
    }
    
    return result;
}






static void menu_display_main(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    lcd_print(menu->lcd, 0, 0, "Main Menu");
    
    char line2[32];
    snprintf(line2, sizeof(line2), ">%s", main_menu_items[menu->menu_selection]);
    lcd_print(menu->lcd, 1, 0, line2);
}

static void menu_display_playback(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    
    char line1[32];
    if (menu->cd_player->disc_present) {
        snprintf(line1, sizeof(line1), "Track %02d/%02d", 
                menu->current_track, menu->cd_player->num_tracks);
    } else {
        strcpy(line1, "No Disc");
    }
    lcd_print(menu->lcd, 0, 0, line1);
    
    char line2[32];
    if (menu->playback_state == PLAYBACK_PLAYING) {
        int minutes = menu->elapsed_time / 60;
        int seconds = menu->elapsed_time % 60;
        int total_min = menu->track_length / 60;
        int total_sec = menu->track_length % 60;
        snprintf(line2, sizeof(line2), "%02d:%02d/%02d:%02d", 
                minutes, seconds, total_min, total_sec);
    } else if (menu->playback_state == PLAYBACK_PAUSED) {
        strcpy(line2, "PAUSED");
    } else {
        strcpy(line2, "STOPPED");
    }
    lcd_print(menu->lcd, 1, 0, line2);
}

static void menu_display_audio_output(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    lcd_print(menu->lcd, 0, 0, "Audio Output");
    
    char line2[32];
    snprintf(line2, sizeof(line2), ">%s", audio_output_items[menu->menu_selection]);
    lcd_print(menu->lcd, 1, 0, line2);
}

static void menu_display_audio_device_list(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    
    char line1[32];
    snprintf(line1, sizeof(line1), "Audio %d/%d", 
             menu->menu_selection + 1, menu->num_audio_devices);
    lcd_print(menu->lcd, 0, 0, line1);
    
    if (menu->num_audio_devices > 0) {
        char line2[32];
        char device_name[13]; // Truncate for display
        strncpy(device_name, menu->audio_devices[menu->menu_selection].name, 12);
        device_name[12] = '\0';
        
        char status = menu->audio_devices[menu->menu_selection].is_available ? '*' : ' ';
        snprintf(line2, sizeof(line2), "%c%s", status, device_name);
        lcd_print(menu->lcd, 1, 0, line2);
    } else {
        lcd_print(menu->lcd, 1, 0, "No devices");
    }
}

static void menu_display_bluetooth(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    lcd_print(menu->lcd, 0, 0, "Bluetooth");
    
    char line2[32];
    if (menu->bt_scanning) {
        strcpy(line2, "Scanning...");
    } else {
        snprintf(line2, sizeof(line2), ">%s", bluetooth_items[menu->menu_selection]);
    }
    lcd_print(menu->lcd, 1, 0, line2);
}

static void menu_display_bt_device_list(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    
    char line1[32];
    
    // Add "Back" option as the last item
    int total_items = menu->num_bt_devices + 1; // +1 for "Back" option
    
    if (menu->menu_selection < menu->num_bt_devices) {
        // Showing a Bluetooth device
        snprintf(line1, sizeof(line1), "BT %d/%d", 
                 menu->menu_selection + 1, total_items);
        lcd_print(menu->lcd, 0, 0, line1);
        
        char line2[17];
        char device_name[13];
        strncpy(device_name, menu->bt_devices[menu->menu_selection].name, 12);
        device_name[12] = '\0';
        
        char status = menu->bt_devices[menu->menu_selection].is_connected ? '*' : 
                     (menu->bt_devices[menu->menu_selection].is_paired ? '+' : ' ');
        snprintf(line2, sizeof(line2), "%c%s", status, device_name);
        lcd_print(menu->lcd, 1, 0, line2);
    } else {
        // Showing "Back" option
        snprintf(line1, sizeof(line1), "BT %d/%d", total_items, total_items);
        lcd_print(menu->lcd, 0, 0, line1);
        lcd_print(menu->lcd, 1, 0, ">Back");
    }
}

static void menu_handle_bt_device_list(menu_system_t *menu, button_event_t event) {
    int total_items = menu->num_bt_devices + 1; // +1 for "Back" option
    
    switch (event) {
        case BUTTON_PREV:
            menu->menu_selection = (menu->menu_selection - 1 + total_items) % total_items;
            menu_update_display(menu);
            break;
            
        case BUTTON_NEXT:
            menu->menu_selection = (menu->menu_selection + 1) % total_items;
            menu_update_display(menu);
            break;
            
        case BUTTON_PLAY_PAUSE:
            if (menu->menu_selection < menu->num_bt_devices) {
                // Handle device selection (existing code)
                bt_device_info_t *device = &menu->bt_devices[menu->menu_selection];
                
                if (device->is_connected) {
                    // Disconnect using bluetoothctl
                    char cmd[128];
                    snprintf(cmd, sizeof(cmd), "bluetoothctl disconnect %s", device->address);
                    if (system(cmd) == 0) {
                        device->is_connected = false;
                        menu_scan_audio_devices(menu);
                        lcd_print(menu->lcd, 1, 0, "Disconnected");
                    }
                } else if (device->is_paired) {
                    // Connect using bluetoothctl
                    char cmd[128];
                    snprintf(cmd, sizeof(cmd), "bluetoothctl connect %s", device->address);
                    if (system(cmd) == 0) {
                        device->is_connected = true;
                        menu_scan_audio_devices(menu);
                        lcd_print(menu->lcd, 1, 0, "Connected");
                    } else {
                        lcd_print(menu->lcd, 1, 0, "Connect Failed");
                    }
                } else {
                    // Pair using bluetoothctl
                    lcd_print(menu->lcd, 1, 0, "Pairing...");
                    char cmd[128];
                    snprintf(cmd, sizeof(cmd), "bluetoothctl pair %s", device->address);
                    if (system(cmd) == 0) {
                        device->is_paired = true;
                        lcd_print(menu->lcd, 1, 0, "Paired");
                    } else {
                        lcd_print(menu->lcd, 1, 0, "Pair Failed");
                    }
                }
                
                usleep(1500000); // Show message for 1.5 seconds
                menu_update_display(menu);
            } else {
                // "Back" option selected
                menu->current_menu = MENU_BLUETOOTH;
                menu->menu_selection = 0;
                menu->max_selections = 4;
                menu_update_display(menu);
            }
            break;
            
        default:
            break;
    }
}


static void menu_display_cd_info(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    lcd_print(menu->lcd, 0, 0, "CD Info");
    
    char line2[32];
    if (menu->cd_player->disc_present) {
        snprintf(line2, sizeof(line2), "%d tracks", menu->cd_player->num_tracks);
    } else {
        strcpy(line2, "No disc");
    }
    lcd_print(menu->lcd, 1, 0, line2);
}

void menu_update_display(menu_system_t *menu) {
    switch (menu->current_menu) {
        case MENU_MAIN:
            menu_display_main(menu);
            break;
        case MENU_PLAYBACK:
            menu_display_playback(menu);
            break;
        case MENU_AUDIO_OUTPUT:
            menu_display_audio_output(menu);
            break;
        case MENU_AUDIO_DEVICE_LIST:
            menu_display_audio_device_list(menu);
            break;
        case MENU_BLUETOOTH:
            menu_display_bluetooth(menu);
            break;
        case MENU_BT_DEVICE_LIST:
            menu_display_bt_device_list(menu);
            break;
        case MENU_CD_INFO:
            menu_display_cd_info(menu);
            break;
    }
}

static void menu_handle_main_menu(menu_system_t *menu, button_event_t event) {
    switch (event) {
        case BUTTON_PREV:
            menu->menu_selection = (menu->menu_selection - 1 + menu->max_selections) % menu->max_selections;
            menu_update_display(menu);
            break;
            
        case BUTTON_NEXT:
            menu->menu_selection = (menu->menu_selection + 1) % menu->max_selections;
            menu_update_display(menu);
            break;
            
        case BUTTON_PLAY_PAUSE:
            switch (menu->menu_selection) {
                case 0: // Play CD
                    if (menu->cd_player->disc_present) {
                        menu->current_menu = MENU_PLAYBACK;
                        menu->playback_state = PLAYBACK_PLAYING;
                        audio_play_track(menu->audio_player, menu->current_track);
                        menu_update_display(menu);
                    }
                    break;
                case 1: // Audio Output
                    menu->current_menu = MENU_AUDIO_OUTPUT;
                    menu->menu_selection = 0;
                    menu->max_selections = 3;
                    menu_update_display(menu);
                    break;
                case 2: // Bluetooth
                    menu->current_menu = MENU_BLUETOOTH;
                    menu->menu_selection = 0;
                    menu->max_selections = 4;
                    
                    // Automatically load paired and connected devices
                    menu_load_known_bt_devices(menu);
                    
                    menu_update_display(menu);
                    break;
                case 3: // CD Info
                    menu->current_menu = MENU_CD_INFO;
                    menu_update_display(menu);
                    break;
                case 4: // Eject CD
                    cd_eject(menu->cd_player);
                    lcd_print(menu->lcd, 1, 0, "Ejecting...");
                    break;
            }
            break;
        default:
            break;
    }
}

static void menu_handle_audio_output(menu_system_t *menu, button_event_t event) {
    switch (event) {
        case BUTTON_PREV:
            menu->menu_selection = (menu->menu_selection - 1 + menu->max_selections) % menu->max_selections;
            menu_update_display(menu);
            break;
            
        case BUTTON_NEXT:
            menu->menu_selection = (menu->menu_selection + 1) % menu->max_selections;
            menu_update_display(menu);
            break;
            
        case BUTTON_PLAY_PAUSE:
            switch (menu->menu_selection) {
                case 0: // Select Device
                    if (menu->num_audio_devices > 0) {
                        menu->current_menu = MENU_AUDIO_DEVICE_LIST;
                        menu->menu_selection = 0;
                        menu->max_selections = menu->num_audio_devices;
                        menu_update_display(menu);
                    }
                    break;
                case 1: // Refresh List
                    lcd_print(menu->lcd, 1, 0, "Refreshing...");
                    menu_scan_audio_devices(menu);
                    lcd_print(menu->lcd, 1, 0, "List Updated");
                    break;
                case 2: // Back
                    menu->current_menu = MENU_MAIN;
                    menu->menu_selection = 0;
                    menu->max_selections = 5;
                    menu_update_display(menu);
                    break;
            }
            break;
            
        default:
            break;
    }
}

static void menu_handle_audio_device_list(menu_system_t *menu, button_event_t event) {
    switch (event) {
        case BUTTON_PREV:
            menu->menu_selection = (menu->menu_selection - 1 + menu->max_selections) % menu->max_selections;
            menu_update_display(menu);
            break;
            
        case BUTTON_NEXT:
            menu->menu_selection = (menu->menu_selection + 1) % menu->max_selections;
            menu_update_display(menu);
            break;
            
        case BUTTON_PLAY_PAUSE:
            if (menu->num_audio_devices > 0) {
                // Select the current audio device
                audio_device_info_t *device = &menu->audio_devices[menu->menu_selection];
                if (device->is_available) {
                    audio_set_device(menu->audio_player, device->device_id);
                    strcpy(menu->current_audio_device, device->device_id);
                    menu->use_bluetooth = device->is_bluetooth;
                    
                    lcd_print(menu->lcd, 1, 0, "Device Selected");
                    usleep(1000000); // Show message for 1 second
                    
                    // Go back to audio output menu
                    menu->current_menu = MENU_AUDIO_OUTPUT;
                    menu->menu_selection = 0;
                    menu->max_selections = 3;
                    menu_update_display(menu);
                }
            }
            break;
            
        default:
            break;
    }
}

static void menu_handle_bluetooth(menu_system_t *menu, button_event_t event) {
    switch (event) {
        case BUTTON_PREV:
            if (!menu->bt_scanning) {
                menu->menu_selection = (menu->menu_selection - 1 + menu->max_selections) % menu->max_selections;
                menu_update_display(menu);
            }
            break;
            
        case BUTTON_NEXT:
            if (!menu->bt_scanning) {
                menu->menu_selection = (menu->menu_selection + 1) % menu->max_selections;
                menu_update_display(menu);
            }
            break;
            
        case BUTTON_PLAY_PAUSE:
            if (!menu->bt_scanning) {
                switch (menu->menu_selection) {
                    case 0: // Scan New Devices
                        lcd_print(menu->lcd, 1, 0, "Scanning...");
                        bluetooth_scan_devices(menu->bluetooth_manager);
                        menu_scan_bt_devices(menu);
                        lcd_print(menu->lcd, 1, 0, "Scan Complete");
                        break;
                    case 1: // Device List
                        // Refresh device status before showing list
                        bluetooth_refresh_device_status(menu->bluetooth_manager);
                        menu_load_known_bt_devices(menu);
                        
                        if (menu->num_bt_devices > 0) {
                            menu->current_menu = MENU_BT_DEVICE_LIST;
                            menu->menu_selection = 0;
                            menu->max_selections = menu->num_bt_devices + 1;
                            menu_update_display(menu);
                        } else {
                            lcd_print(menu->lcd, 1, 0, "No devices");
                        }
                        break;
                    case 2: // Disconnect
                        bluetooth_disconnect_device(menu->bluetooth_manager);
                        menu_scan_audio_devices(menu); // Refresh audio devices
                        menu_load_known_bt_devices(menu); // Refresh BT device list
                        lcd_print(menu->lcd, 1, 0, "Disconnected");
                        break;
                    case 3: // Back
                        menu->current_menu = MENU_MAIN;
                        menu->menu_selection = 0;
                        menu->max_selections = 5;
                        menu_update_display(menu);
                        break;
                }
            }
            break;
            
        default:
            break;
    }
}



static void menu_handle_playback(menu_system_t *menu, button_event_t event) {
    switch (event) {
        case BUTTON_PLAY_PAUSE:
            if (menu->playback_state == PLAYBACK_PLAYING) {
                audio_pause(menu->audio_player);
                menu->playback_state = PLAYBACK_PAUSED;
            } else if (menu->playback_state == PLAYBACK_PAUSED) {
                audio_resume(menu->audio_player);
                menu->playback_state = PLAYBACK_PLAYING;
            } else {
                audio_play_track(menu->audio_player, menu->current_track);
                menu->playback_state = PLAYBACK_PLAYING;
            }
            menu_update_display(menu);
            break;
            
        case BUTTON_PREV:
            if (menu->current_track > 1) {
                menu->current_track--;
                audio_play_track(menu->audio_player, menu->current_track);
                menu->elapsed_time = 0;
                menu_update_display(menu);
            }
            break;
            
        case BUTTON_NEXT:
            if (menu->current_track < menu->cd_player->num_tracks) {
                menu->current_track++;
                audio_play_track(menu->audio_player, menu->current_track);
                menu->elapsed_time = 0;
                menu_update_display(menu);
            }
            break;
            
        default:
            break;
    }
}

void menu_handle_button(menu_system_t *menu, button_event_t event) {
    if (event == BUTTON_NONE) {
        return;
    }
    
    switch (menu->current_menu) {
        case MENU_MAIN:
            menu_handle_main_menu(menu, event);
            break;
        case MENU_PLAYBACK:
            menu_handle_playback(menu, event);
            break;
        case MENU_AUDIO_OUTPUT:
            menu_handle_audio_output(menu, event);
            break;
        case MENU_AUDIO_DEVICE_LIST:
            menu_handle_audio_device_list(menu, event);
            break;
        case MENU_BLUETOOTH:
            menu_handle_bluetooth(menu, event);
            break;
        case MENU_BT_DEVICE_LIST:
            menu_handle_bt_device_list(menu, event);
            break;
        case MENU_CD_INFO:
            // CD Info is read-only, any button goes back
            menu->current_menu = MENU_MAIN;
            menu->menu_selection = 0;
            menu->max_selections = 5;
            menu_update_display(menu);
            break;
    }
}

void menu_update_playback_info(menu_system_t *menu) {
    if (menu->playback_state == PLAYBACK_PLAYING) {
        menu->elapsed_time++;
        
        if (menu->track_length == 0) {
            cd_get_track_info(menu->cd_player, menu->current_track, &menu->track_length);
        }
        
        if (menu->elapsed_time >= menu->track_length && menu->track_length > 0) {
            if (menu->current_track < menu->cd_player->num_tracks) {
                menu->current_track++;
                menu->elapsed_time = 0;
                menu->track_length = 0;
                audio_play_track(menu->audio_player, menu->current_track);
            } else {
                menu->playback_state = PLAYBACK_STOPPED;
                audio_stop(menu->audio_player);
            }
        }
        
        if (menu->current_menu == MENU_PLAYBACK) {
            menu_update_display(menu);
        }
    }
}

void menu_cleanup(menu_system_t *menu) {
    if (menu->playback_state != PLAYBACK_STOPPED) {
        audio_stop(menu->audio_player);
    }
    
    memset(menu, 0, sizeof(menu_system_t));
}
