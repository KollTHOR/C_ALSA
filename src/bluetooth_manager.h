#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

// With this:
#define NOTIFICATION_SOUND_PATH "./assets/sounds/bt_connect.wav"
#define DISCONNECT_SOUND_PATH "./assets/sounds/bt_disconnect.wav"
#define ERROR_SOUND_PATH "./assets/sounds/error_beep.wav"

#include <stdbool.h>
#include <glib.h>
#include <gio/gio.h>
#include <stdio.h>

#define MAX_DEVICES 20
#define MAX_DEVICE_NAME 256

typedef struct {
    char name[MAX_DEVICE_NAME];
    char address[18];
    bool is_paired;
    bool is_connected;
    bool is_audio_device;
} bluetooth_device_t;

typedef struct {
    GDBusConnection *connection;
    GDBusObjectManager *object_manager;
    bluetooth_device_t devices[MAX_DEVICES];
    int num_devices;
    bool is_connected;
    char connected_device[18];
    bool is_scanning;
} bluetooth_manager_t;

// Function declarations only (no implementations)
int bluetooth_init(bluetooth_manager_t *manager);
int bluetooth_scan_devices(bluetooth_manager_t *manager);
int bluetooth_load_paired_devices(bluetooth_manager_t *manager);
int bluetooth_connect_device(bluetooth_manager_t *manager, const char *device_address);
int bluetooth_disconnect_device(bluetooth_manager_t *manager);
int bluetooth_get_audio_device_name(bluetooth_manager_t *manager, char *device_name, size_t size);
void bluetooth_cleanup(bluetooth_manager_t *manager);
int bluetooth_refresh_device_status(bluetooth_manager_t *manager);
int bluetooth_check_connection_status(bluetooth_manager_t *manager, const char *device_address);
int bluetooth_pair_device(bluetooth_manager_t *manager, const char *device_address);
int bluetooth_reset_service(void);
int bluetooth_disconnect_fallback(bluetooth_manager_t *manager);
int bluetooth_check_bluealsa_health(void);

#endif
