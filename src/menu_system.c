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
    printf("🔍 Scanning for BlueALSA audio devices...\n");
    
    // Check if BlueALSA service is running
    if (system("pgrep bluealsa > /dev/null 2>&1") != 0) {
        printf("❌ BlueALSA service not running\n");
        return;
    }
    
    // Use bluealsa-aplay to list available devices
    FILE *fp = popen("bluealsa-aplay -l 2>/dev/null", "r");
    if (!fp) {
        printf("❌ Failed to run bluealsa-aplay\n");
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
            
            if (sscanf(line, "%15[^:]: %17s [%31[^]]], %127[^\n]", 
                      hci, mac, mac_brackets, device_name) == 4) {
                
                printf("✅ Found BlueALSA device: %s (%s)\n", device_name, mac);
                
                // Add to audio device list with proper BlueALSA device string
                snprintf(menu->audio_devices[menu->num_audio_devices].name, 
                        sizeof(menu->audio_devices[menu->num_audio_devices].name),
                        "BT: %s", device_name);
                
                // Create proper BlueALSA device string
                snprintf(menu->audio_devices[menu->num_audio_devices].device_id,
                        sizeof(menu->audio_devices[menu->num_audio_devices].device_id),
                        "bluealsa:DEV=%s,PROFILE=a2dp", mac);
                
                menu->audio_devices[menu->num_audio_devices].is_bluetooth = true;
                menu->audio_devices[menu->num_audio_devices].is_available = true;
                menu->num_audio_devices++;
                
                printf("📱 Added BlueALSA device: %s\n", 
                       menu->audio_devices[menu->num_audio_devices-1].device_id);
            }
        }
    }
    pclose(fp);
    
    printf("🔍 BlueALSA scan complete. Found %d devices\n", 
           menu->num_audio_devices - 2); // Subtract wired devices
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

// KEEP THIS VERSION (around line 663):
static void menu_display_playback(menu_system_t *menu) {
    lcd_clear(menu->lcd);
    
    char line1[32];
    if (menu->cd_player->disc_present && menu->cd_player->is_audio_cd) {
        char device_indicator = menu->use_bluetooth ? 'B' : 'W';
        snprintf(line1, sizeof(line1), "%c Track %02d/%02d", device_indicator,
                menu->current_track, menu->cd_player->num_tracks);
    } else {
        strcpy(line1, "No Disc");
    }
    lcd_print(menu->lcd, 0, 0, line1);
    
    // Enhanced timing display
    char line2[32];
    if (menu->playback_state == PLAYBACK_PLAYING || menu->playback_state == PLAYBACK_PAUSED) {
        int elapsed, total;
        if (audio_get_position(menu->audio_player, &elapsed, &total) == 0) {
            int elapsed_min = elapsed / 60;
            int elapsed_sec = elapsed % 60;
            int total_min = total / 60;
            int total_sec = total % 60;
            
            snprintf(line2, sizeof(line2), "%02d:%02d/%02d:%02d", 
                    elapsed_min, elapsed_sec, total_min, total_sec);
            
            if (menu->playback_state == PLAYBACK_PAUSED) {
                strcat(line2, " ||");
            }
        } else {
            strcpy(line2, "00:00/00:00");
        }
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
                // Handle device selection
                bt_device_info_t *device = &menu->bt_devices[menu->menu_selection];
                
                if (device->is_connected) {
                    // Disconnect using D-Bus API
                    if (bluetooth_disconnect_device(menu->bluetooth_manager) == 0) {
                        device->is_connected = false;
                        menu_scan_audio_devices(menu);
                        lcd_print(menu->lcd, 1, 0, "Disconnected");
                    } else {
                        lcd_print(menu->lcd, 1, 0, "Disconnect Failed");
                    }
                } else if (device->is_paired) {
                    // Connect using D-Bus API
                    lcd_print(menu->lcd, 1, 0, "Connecting...");
                    if (bluetooth_connect_device(menu->bluetooth_manager, device->address) == 0) {
                        device->is_connected = true;
                        menu_scan_audio_devices(menu);
                        lcd_print(menu->lcd, 1, 0, "Connected!");
                    } else {
                        lcd_print(menu->lcd, 1, 0, "Connect Failed");
                    }
                } else {
                    // Pair using D-Bus API
                    lcd_print(menu->lcd, 1, 0, "Pairing...");
                    if (bluetooth_pair_device(menu->bluetooth_manager, device->address) == 0) {
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
            
        case BUTTON_NONE:
            // Do nothing for no button press
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
        if (menu->cd_player->is_audio_cd) {
            snprintf(line2, sizeof(line2), "%d audio tracks", menu->cd_player->num_tracks);
        } else {
            strcpy(line2, "Not audio CD");
        }
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
                    cd_detect_disc(menu->cd_player);
                    if (menu->cd_player->disc_present && menu->cd_player->is_audio_cd) {
                        cd_get_disc_info(menu->cd_player);

                        // Validate current audio device
                        if (audio_validate_device(menu->audio_player) != 0) {
                            printf("⚠️  Current audio device not ready, using default...\n");
                            audio_set_device(menu->audio_player, "hw:0,0");
                            strcpy(menu->current_audio_device, "hw:0,0");
                            menu->use_bluetooth = false;
                        }

                        // Ensure CD player reference is set
                        audio_set_cd_player(menu->audio_player, menu->cd_player);

                        printf("🎵 Starting CD playback on device: %s\n", menu->current_audio_device);

                        menu->current_menu = MENU_PLAYBACK;
                        menu->playback_state = PLAYBACK_PLAYING;

                        if (audio_play_track(menu->audio_player, menu->current_track) == 0) {
                            printf("✅ CD playback started on selected device\n");
                        } else {
                            printf("❌ Failed to start CD playback\n");
                            menu->playback_state = PLAYBACK_STOPPED;
                        }

                        menu_update_display(menu);
                    } else {
                        lcd_print(menu->lcd, 1, 0, menu->cd_player->disc_present ? "Not audio CD" : "No disc");
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
            printf("🔼 Audio menu: selected item %d\n", menu->menu_selection);
            menu_update_display(menu);
            break;
            
        case BUTTON_NEXT:
            menu->menu_selection = (menu->menu_selection + 1) % menu->max_selections;
            printf("🔽 Audio menu: selected item %d\n", menu->menu_selection);
            menu_update_display(menu);
            break;
            
        case BUTTON_PLAY_PAUSE:
            printf("▶️ Audio menu: action on item %d\n", menu->menu_selection);
            switch (menu->menu_selection) {
                case 0: // Select Device
                    if (menu->num_audio_devices > 0) {
                        printf("📱 Entering device list (%d devices available)\n", menu->num_audio_devices);
                        menu->current_menu = MENU_AUDIO_DEVICE_LIST;
                        menu->menu_selection = 0;
                        menu->max_selections = menu->num_audio_devices;
                        menu_update_display(menu);
                    } else {
                        lcd_print(menu->lcd, 1, 0, "No devices");
                        printf("❌ No audio devices available\n");
                    }
                    break;
                case 1: // Refresh List
                    lcd_print(menu->lcd, 1, 0, "Refreshing...");
                    printf("🔄 Refreshing audio device list...\n");
                    menu_scan_audio_devices(menu);
                    printf("✅ Found %d audio devices after refresh\n", menu->num_audio_devices);
                    lcd_print(menu->lcd, 1, 0, "List Updated");
                    usleep(1000000); // Show message for 1 second
                    menu_update_display(menu);
                    break;
                case 2: // Back
                    printf("🔙 Returning to main menu\n");
                    menu->current_menu = MENU_MAIN;
                    menu->menu_selection = 0;
                    menu->max_selections = 5;
                    menu_update_display(menu);
                    break;
            }
            break;
            
        case BUTTON_NONE:
            break;
            
        default:
            break;
    }
}


static void menu_handle_audio_device_list(menu_system_t *menu, button_event_t event) {
    switch (event) {
        case BUTTON_PREV:
            menu->menu_selection = (menu->menu_selection - 1 + menu->max_selections) % menu->max_selections;
            printf("🔼 Device list: selected device %d\n", menu->menu_selection);
            menu_update_display(menu);
            break;
            
        case BUTTON_NEXT:
            menu->menu_selection = (menu->menu_selection + 1) % menu->max_selections;
            printf("🔽 Device list: selected device %d\n", menu->menu_selection);
            menu_update_display(menu);
            break;
            
        case BUTTON_PLAY_PAUSE:
            if (menu->num_audio_devices > 0 && menu->menu_selection < menu->num_audio_devices) {
                audio_device_info_t *device = &menu->audio_devices[menu->menu_selection];
                
                if (device->is_available) {
                    printf("🔄 Switching to audio device: %s (%s)\n", device->name, device->device_id);
                    
                    // Check if it's a Bluetooth device and validate BlueALSA health
                    if (device->is_bluetooth) {
                        if (bluetooth_check_bluealsa_health() != 0) {
                            lcd_print(menu->lcd, 1, 0, "BT Service Error");
                            printf("❌ BlueALSA service unhealthy, cannot use Bluetooth audio\n");
                            usleep(2000000);
                            menu_update_display(menu);
                            break;
                        }
                    }
                    
                    // Stop current playback if active
                    bool was_playing = (menu->playback_state == PLAYBACK_PLAYING);
                    int current_track = menu->current_track;
                    
                    if (was_playing) {
                        printf("⏹️ Stopping current playback for device switch\n");
                        audio_stop(menu->audio_player);
                        menu->playback_state = PLAYBACK_STOPPED;
                    }
                    
                    // Switch audio device
                    if (audio_set_device(menu->audio_player, device->device_id) == 0) {
                        strcpy(menu->current_audio_device, device->device_id);
                        menu->use_bluetooth = device->is_bluetooth;
                        
                        // Ensure CD player reference is maintained
                        audio_set_cd_player(menu->audio_player, menu->cd_player);
                        
                        printf("✅ Audio device switched to: %s\n", device->device_id);
                        lcd_print(menu->lcd, 1, 0, "Device Selected");
                        
                        // If we were playing, restart on new device
                        if (was_playing) {
                            printf("🔄 Restarting playback on new device...\n");
                            usleep(2000000); // 2 second delay for device stabilization
                            
                            if (audio_play_track(menu->audio_player, current_track) == 0) {
                                menu->playback_state = PLAYBACK_PLAYING;
                                printf("✅ Playback resumed on %s\n", device->name);
                            } else {
                                printf("❌ Failed to restart playback on new device\n");
                            }
                        }
                    } else {
                        lcd_print(menu->lcd, 1, 0, "Switch Failed");
                        printf("❌ Failed to switch to audio device: %s\n", device->device_id);
                    }
                    
                    usleep(2000000); // Show message for 2 seconds
                    
                    // Return to audio output menu
                    menu->current_menu = MENU_AUDIO_OUTPUT;
                    menu->menu_selection = 0;
                    menu->max_selections = 3;
                    menu_update_display(menu);
                }
            }
            break;
            
        case BUTTON_NONE:
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
                        printf("🎛️  User selected Disconnect option\n");
                        
                        if (menu->bluetooth_manager->is_connected) {
                            lcd_print(menu->lcd, 1, 0, "Disconnecting...");
                            
                            int disconnect_attempts = 0;
                            int result = -1;
                            
                            // Try disconnect up to 2 times before service reset
                            while (disconnect_attempts < 2 && result != 0) {
                                result = bluetooth_disconnect_device(menu->bluetooth_manager);
                                disconnect_attempts++;
                                
                                if (result != 0 && disconnect_attempts < 2) {
                                    printf("⚠️  Disconnect attempt %d failed, retrying...\n", disconnect_attempts);
                                    sleep(1);
                                }
                            }
                            
                            if (result == 0) {
                                menu_scan_audio_devices(menu);
                                lcd_print(menu->lcd, 1, 0, "Disconnected");
                            } else {
                                // Last resort: manual service reset
                                lcd_print(menu->lcd, 1, 0, "Resetting BT...");
                                printf("🔄 Manual Bluetooth service reset triggered\n");
                                
                                if (bluetooth_reset_service() == 0) {
                                    // Force state update after reset
                                    menu->bluetooth_manager->is_connected = false;
                                    memset(menu->bluetooth_manager->connected_device, 0, 
                                           sizeof(menu->bluetooth_manager->connected_device));
                                           menu_load_known_bt_devices(menu);
                                    menu_scan_audio_devices(menu);
                                    lcd_print(menu->lcd, 1, 0, "BT Reset Done");
                                } else {
                                    lcd_print(menu->lcd, 1, 0, "Reset Failed");
                                }
                            }
                        }
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
        // Force display update even if audio thread isn't updating properly
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
