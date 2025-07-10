#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <stdbool.h>
#include <dbus/dbus.h>

#define MAX_DEVICE_NAME 256
#define MAX_DEVICE_ADDRESS 18
#define MAX_DEVICES 10

typedef struct
{
    char name[MAX_DEVICE_NAME];
    char address[MAX_DEVICE_ADDRESS];
    bool is_paired;
    bool is_connected;
    bool is_audio_device;
} bluetooth_device_t;

typedef struct
{
    DBusConnection *connection;
    DBusError error;
    char adapter_path[256];

    bluetooth_device_t devices[MAX_DEVICES];
    int num_devices;

    char connected_device[MAX_DEVICE_ADDRESS];
    bool is_connected;
    bool is_scanning;
} bluetooth_manager_t;

// Function declarations
int bluetooth_init(bluetooth_manager_t *manager);
int bluetooth_scan_devices(bluetooth_manager_t *manager);
int bluetooth_pair_device(bluetooth_manager_t *manager, const char *device_address);
int bluetooth_connect_device(bluetooth_manager_t *manager, const char *device_address);
int bluetooth_disconnect_device(bluetooth_manager_t *manager);
int bluetooth_get_audio_device_name(bluetooth_manager_t *manager, char *device_name, size_t size);
void bluetooth_cleanup(bluetooth_manager_t *manager);
int bluetooth_load_paired_devices(bluetooth_manager_t *manager);
int bluetooth_refresh_device_status(bluetooth_manager_t *manager);

// Additional helper functions
int bluetooth_list_paired_devices(bluetooth_manager_t *manager);
int bluetooth_remove_device(bluetooth_manager_t *manager, const char *device_address);
bool bluetooth_is_device_connected(bluetooth_manager_t *manager, const char *device_address);
int bluetooth_get_device_info(bluetooth_manager_t *manager, const char *device_address, bluetooth_device_t *device);

#endif
