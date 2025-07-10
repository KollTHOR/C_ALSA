#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "cd_control.h"
#include "audio_playback.h"
#include "lcd_display.h"
#include "button_input.h"
#include "bluetooth_manager.h"
#include "menu_system.h"

static volatile bool running = true;
static menu_system_t menu;

void signal_handler(int sig) {
    (void)sig;
    running = false;
}

void* playback_timer_thread(void* arg) {
    menu_system_t* menu = (menu_system_t*)arg;
    
    while (running) {
        sleep(1); // Update every second
        menu_update_playback_info(menu);
    }
    
    return NULL;
}

void* cd_monitor_thread(void* arg) {
    cd_player_t* cd_player = (cd_player_t*)arg;
    
    while (running) {
        cd_detect_disc(cd_player);
        sleep(2); // Check every 2 seconds
    }
    
    return NULL;
}

int main() {
    printf("CD Player starting...\n");
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize components with proper error handling
    cd_player_t cd_player = {0};
    audio_player_t audio_player = {0};
    lcd_t lcd = {0};
    button_manager_t button_manager = {0};
    
    // Initialize LCD first
    if (lcd_init(&lcd, 0x27) != 0) {
        printf("Failed to initialize LCD - continuing without display\n");
        // Don't exit - LCD failure shouldn't stop the program
    } else {
        lcd_print(&lcd, 0, 0, "CD Player v1.0");
        lcd_print(&lcd, 1, 0, "Initializing...");
    }
    
    // Try different audio devices
    int audio_initialized = 0;
    const char* audio_devices[] = {"hw:0,0", "hw:1,0", "default", NULL};
    
    for (int i = 0; audio_devices[i] != NULL; i++) {
        printf("Trying audio device: %s\n", audio_devices[i]);
        if (audio_init(&audio_player, audio_devices[i]) == 0) {
            printf("Audio initialized successfully with device: %s\n", audio_devices[i]);
            audio_initialized = 1;
            break;
        }
    }
    
    if (!audio_initialized) {
        printf("Warning: No audio device available - continuing without audio\n");
    }
    
    // Initialize buttons with WiringPi pin numbers
    if (button_init(&button_manager, 2, 5, 8) != 0) {
        printf("Warning: Button initialization failed - continuing without buttons\n");
        button_manager.play_pin = -1; // Mark as invalid
    } else {
        printf("Buttons initialized successfully\n");
    }
    
    // Initialize CD player
    if (cd_init(&cd_player) != 0) {
        printf("Warning: CD-ROM not available\n");
    }
    
    // Initialize Bluetooth
    bluetooth_manager_t bluetooth_manager = {0};
    if (bluetooth_init(&bluetooth_manager) != 0) {
        printf("Warning: Bluetooth not available\n");
    }
    
    // Only initialize menu system if at least LCD is working
    if (lcd.i2c_fd >= 0) {
        if (menu_init(&menu, &lcd, &cd_player, &audio_player, &bluetooth_manager) != 0) {
            printf("Failed to initialize menu system\n");
            goto cleanup;
        }
    }
    
    printf("CD Player ready!\n");
    
    // Main event loop with null pointer checks
    while (running) {
        // Only poll buttons if they were initialized successfully
        if (button_manager.play_pin >= 0) {
            button_event_t event = button_poll(&button_manager);
            if (event != BUTTON_NONE && lcd.i2c_fd >= 0) {
                menu_handle_button(&menu, event);
            }
        }
        
        usleep(50000); // 50ms delay
    }
    
cleanup:
    // Cleanup with proper checks
    if (button_manager.play_pin >= 0) {
        button_cleanup(&button_manager);
    }
    if (bluetooth_manager.connection != NULL) {
        bluetooth_cleanup(&bluetooth_manager);
    }
    if (audio_player.pcm_handle != NULL) {
        audio_cleanup(&audio_player);
    }
    cd_cleanup(&cd_player);
    if (lcd.i2c_fd >= 0) {
        lcd_cleanup(&lcd);
    }
    
    printf("CD Player stopped\n");
    return 0;
}
