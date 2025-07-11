#include "bluetooth_manager.h"
#include "audio_playback.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include <gio/gio.h>
#include <time.h>


int bluetooth_init(bluetooth_manager_t *manager) {
    GError *error = NULL;
    
    // Try to connect to D-Bus
    manager->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error) {
        printf("❌ Failed to connect to D-Bus, attempting Bluetooth service reset...\n");
        g_error_free(error);
        
        // Reset service and retry
        if (bluetooth_reset_service() == 0) {
            printf("🔄 Retrying D-Bus connection after service reset...\n");
            manager->connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
            if (error) {
                g_error_free(error);
                return -1;
            }
        } else {
            return -1;
        }
    }
    
    // Create object manager for BlueZ
    manager->object_manager = g_dbus_object_manager_client_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
        "org.bluez",
        "/",
        NULL, NULL, NULL, NULL,
        &error
    );
    
    if (error) {
        g_error_free(error);
        return -1;
    }
    
    return 0;
}


int bluetooth_scan_devices(bluetooth_manager_t *manager) {
    manager->num_devices = 0;
    manager->is_scanning = true;
    
    printf("🔍 Scanning for Bluetooth devices...\n");
    
    GList *objects = g_dbus_object_manager_get_objects(manager->object_manager);
    
    for (GList *l = objects; l != NULL; l = l->next) {
        GDBusObject *object = G_DBUS_OBJECT(l->data);
        const char *object_path = g_dbus_object_get_object_path(object);
        
        // Check if this is a device object
        if (strstr(object_path, "/dev_")) {
            GDBusInterface *device_interface = 
                g_dbus_object_get_interface(object, "org.bluez.Device1");
            
            if (device_interface && manager->num_devices < MAX_DEVICES) {
                GVariant *address_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Address");
                GVariant *name_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Name");
                GVariant *paired_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Paired");
                GVariant *connected_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Connected");
                
                if (address_variant) {
                    const char *address = g_variant_get_string(address_variant, NULL);
                    const char *name = name_variant ? 
                        g_variant_get_string(name_variant, NULL) : "Unknown Device";
                    gboolean is_paired = paired_variant ? 
                        g_variant_get_boolean(paired_variant) : FALSE;
                    gboolean is_connected = connected_variant ? 
                        g_variant_get_boolean(connected_variant) : FALSE;
                    
                    strcpy(manager->devices[manager->num_devices].address, address);
                    strcpy(manager->devices[manager->num_devices].name, name);
                    manager->devices[manager->num_devices].is_paired = is_paired;
                    manager->devices[manager->num_devices].is_connected = is_connected;
                    manager->devices[manager->num_devices].is_audio_device = true;
                    
                    printf("Found: %s %s (%s%s)\n", 
                           name, address,
                           is_connected ? "●" : (is_paired ? "○" : "◦"),
                           is_connected ? " Connected" : (is_paired ? " Paired" : " New"));
                    
                    manager->num_devices++;
                }
                
                g_object_unref(device_interface);
            }
        }
    }
    
    g_list_free_full(objects, g_object_unref);
    manager->is_scanning = false;
    
    printf("📱 Found %d Bluetooth devices\n", manager->num_devices);
    return manager->num_devices;
}

int bluetooth_refresh_device_status(bluetooth_manager_t *manager) {
    // Update connection status for all known devices
    for (int i = 0; i < manager->num_devices; i++) {
        char object_path[256];
        snprintf(object_path, sizeof(object_path), "/org/bluez/hci0/dev_%s", 
                 manager->devices[i].address);
        
        // Replace colons with underscores in path
        for (int j = 0; object_path[j]; j++) {
            if (object_path[j] == ':') object_path[j] = '_';
        }
        
        GError *error = NULL;
        GDBusProxy *device_proxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_NONE,
            NULL,
            "org.bluez",
            object_path,
            "org.bluez.Device1",
            NULL,
            &error
        );
        
        if (!error && device_proxy) {
            GVariant *connected_variant = g_dbus_proxy_get_cached_property(
                device_proxy, "Connected");
            if (connected_variant) {
                manager->devices[i].is_connected = g_variant_get_boolean(connected_variant);
                g_variant_unref(connected_variant);
            }
            g_object_unref(device_proxy);
        } else if (error) {
            g_error_free(error);
        }
    }
    
    return 0;
}

int bluetooth_disconnect_device(bluetooth_manager_t *manager) {
    printf("🔌 Attempting to disconnect Bluetooth device...\n");
    
    if (!manager->is_connected) {
        printf("⚠️  No device currently connected\n");
        return 0;
    }
    
    printf("📱 Disconnecting from device: %s\n", manager->connected_device);
    
    // First, try to check if device is actually connected
    char object_path[256];
    snprintf(object_path, sizeof(object_path), "/org/bluez/hci0/dev_%s", 
             manager->connected_device);
    
    // Replace colons with underscores in path
    for (int i = 0; object_path[i]; i++) {
        if (object_path[i] == ':') object_path[i] = '_';
    }
    
    printf("🔗 Using D-Bus object path: %s\n", object_path);
    
    GError *error = NULL;
    GDBusProxy *device_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.bluez",
        object_path,
        "org.bluez.Device1",
        NULL,
        &error
    );
    
    if (error) {
        printf("❌ Failed to create D-Bus proxy: %s\n", error->message);
        g_error_free(error);
        return -1;
    }
    
    if (!device_proxy) {
        printf("❌ Failed to create device proxy (NULL returned)\n");
        return -1;
    }
    
    // Check current connection status before attempting disconnect
    GVariant *connected_variant = g_dbus_proxy_get_cached_property(device_proxy, "Connected");
    if (connected_variant) {
        gboolean is_connected = g_variant_get_boolean(connected_variant);
        printf("📊 Device connection status: %s\n", is_connected ? "Connected" : "Disconnected");
        
        if (!is_connected) {
            printf("✅ Device already disconnected\n");
            g_variant_unref(connected_variant);
            g_object_unref(device_proxy);
            
            // Update manager state
            manager->is_connected = false;
            memset(manager->connected_device, 0, sizeof(manager->connected_device));
            return 0;
        }
        g_variant_unref(connected_variant);
    }
    
    printf("🔄 Calling Disconnect method via D-Bus...\n");
    
    // Use shorter timeout and add cancellation support
    GVariant *result = g_dbus_proxy_call_sync(
        device_proxy,
        "Disconnect",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        2000,  // Reduced timeout to 2 seconds
        NULL,
        &error
    );
    
    if (error) {
        printf("❌ D-Bus Disconnect method failed: %s\n", error->message);
        
        if (strstr(error->message, "Timeout") || strstr(error->message, "timeout")) {
            printf("⏰ Timeout detected, trying fallback methods...\n");
            g_error_free(error);
            g_object_unref(device_proxy);
            
            // Step 1: Try bluetoothctl fallback
            printf("🔄 Attempting bluetoothctl fallback...\n");
            int fallback_result = bluetooth_disconnect_fallback(manager);
            
            if (fallback_result == 0) {
                printf("✅ Fallback disconnect succeeded\n");
                return 0;
            }
            
            // Step 2: If fallback also fails, reset Bluetooth service
            printf("❌ Fallback disconnect failed, resetting Bluetooth service...\n");
            if (bluetooth_reset_service() == 0) {
                printf("✅ Bluetooth service reset completed\n");
                
                // Force update manager state after reset
                manager->is_connected = false;
                memset(manager->connected_device, 0, sizeof(manager->connected_device));
                
                return 0;
            } else {
                printf("❌ Bluetooth service reset failed\n");
                return -1;
            }
        }
        
        g_error_free(error);
        g_object_unref(device_proxy);
        return -1;
    }
    
    if (result) {
        printf("✅ D-Bus Disconnect method succeeded\n");
        g_variant_unref(result);
        
        // Update manager state
        manager->is_connected = false;
        printf("📝 Updated manager state: is_connected = false\n");
        
        printf("🧹 Clearing connected device: %s\n", manager->connected_device);
        memset(manager->connected_device, 0, sizeof(manager->connected_device));
        
        printf("✅ Successfully disconnected from Bluetooth device\n");
    } else {
        printf("⚠️  Disconnect method returned NULL result\n");
    }
    
    g_object_unref(device_proxy);
    printf("🧹 Cleaned up D-Bus proxy\n");
    
    return 0;
}

int bluetooth_disconnect_fallback(bluetooth_manager_t *manager) {
    printf("🔄 Using fallback disconnect method with timeout...\n");
    
    char command[256];
    snprintf(command, sizeof(command), "timeout 5 bluetoothctl disconnect %s", manager->connected_device);
    
    printf("🖥️  Executing: %s\n", command);
    
    int result = system(command);
    
    // Check if timeout occurred (exit code 124)
    if (WEXITSTATUS(result) == 124) {
        printf("⏰ Bluetoothctl fallback timed out after 5 seconds\n");
        
        // Force disconnect using hciconfig
        printf("🔧 Attempting hardware-level disconnect...\n");
        char hci_command[256];
        snprintf(hci_command, sizeof(hci_command), "sudo hciconfig hci0 reset");
        
        if (system(hci_command) == 0) {
            printf("✅ Hardware reset completed\n");
            
            // Update manager state after hardware reset
            manager->is_connected = false;
            memset(manager->connected_device, 0, sizeof(manager->connected_device));
            
            return 0;
        } else {
            printf("❌ Hardware reset failed\n");
            return -1;
        }
    } else if (result == 0) {
        printf("✅ Fallback disconnect succeeded\n");
        
        // Update manager state
        manager->is_connected = false;
        memset(manager->connected_device, 0, sizeof(manager->connected_device));
        
        return 0;
    } else {
        printf("❌ Fallback disconnect failed with exit code: %d\n", WEXITSTATUS(result));
        return -1;
    }
}




int bluetooth_reset_service(void) {
    printf("🔄 Resetting Bluetooth service...\n");
    
    // Stop BlueALSA
    if (system("sudo systemctl stop bluealsa") != 0) {
        printf("⚠️  Warning: Failed to stop BlueALSA service\n");
    }
    sleep(1);
    
    // Restart Bluetooth service
    if (system("sudo systemctl restart bluetooth") != 0) {
        printf("❌ Failed to restart Bluetooth service\n");
        return -1;
    }
    
    sleep(2); // Wait for service to start
    
    // Restart BlueALSA
    if (system("sudo systemctl start bluealsa") != 0) {
        printf("⚠️  Warning: Failed to start BlueALSA service\n");
    }
    sleep(1);
    
    printf("✅ Bluetooth service reset completed\n");
    return 0;
}

int bluetooth_pair_device(bluetooth_manager_t *manager, const char *device_address) {
    (void)manager;
    printf("📱 Pairing with device: %s\n", device_address);
    
    GError *error = NULL;
    char object_path[256];
    
    // Convert MAC address to D-Bus object path
    snprintf(object_path, sizeof(object_path), "/org/bluez/hci0/dev_%s", device_address);
    
    // Replace colons with underscores in path
    for (int i = 0; object_path[i]; i++) {
        if (object_path[i] == ':') object_path[i] = '_';
    }
    
    printf("🔗 Using D-Bus object path: %s\n", object_path);
    
    // Create D-Bus proxy for the device
    GDBusProxy *device_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.bluez",
        object_path,
        "org.bluez.Device1",
        NULL,
        &error
    );
    
    if (error) {
        printf("❌ Failed to create D-Bus proxy: %s\n", error->message);
        g_error_free(error);
        return -1;
    }
    
    if (!device_proxy) {
        printf("❌ Failed to create device proxy (NULL returned)\n");
        return -1;
    }
    
    printf("🔄 Calling Pair method via D-Bus...\n");
    
    // Call the Pair method
    GVariant *result = g_dbus_proxy_call_sync(
        device_proxy,
        "Pair",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        30000,  // 30 second timeout for pairing
        NULL,
        &error
    );
    
    if (error) {
        printf("❌ D-Bus Pair method failed: %s\n", error->message);
        g_error_free(error);
        g_object_unref(device_proxy);
        return -1;
    }
    
    if (result) {
        printf("✅ D-Bus Pair method succeeded\n");
        g_variant_unref(result);
        
        printf("✅ Successfully paired with device: %s\n", device_address);
    } else {
        printf("⚠️  Pair method returned NULL result\n");
    }
    
    g_object_unref(device_proxy);
    printf("🧹 Cleaned up D-Bus proxy\n");
    
    return 0;
}


int bluetooth_check_connection_status(bluetooth_manager_t *manager, const char *device_address) {
    (void)manager; 
    char object_path[256];
    snprintf(object_path, sizeof(object_path), "/org/bluez/hci0/dev_%s", device_address);
    
    // Replace colons with underscores in path
    for (int i = 0; object_path[i]; i++) {
        if (object_path[i] == ':') object_path[i] = '_';
    }
    
    GError *error = NULL;
    GDBusProxy *device_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.bluez",
        object_path,
        "org.bluez.Device1",
        NULL,
        &error
    );
    
    if (error || !device_proxy) {
        if (error) g_error_free(error);
        return -1;
    }
    
    GVariant *connected_variant = g_dbus_proxy_get_cached_property(device_proxy, "Connected");
    GVariant *name_variant = g_dbus_proxy_get_cached_property(device_proxy, "Name");
    
    if (connected_variant && name_variant) {
        gboolean is_connected = g_variant_get_boolean(connected_variant);
        const char *name = g_variant_get_string(name_variant, NULL);
        
        printf("📊 Device Status: %s (%s) - %s\n", 
               name, device_address, 
               is_connected ? "Connected" : "Disconnected");
        
        g_variant_unref(connected_variant);
        g_variant_unref(name_variant);
    }
    
    g_object_unref(device_proxy);
    return 0;
}



void bluetooth_cleanup(bluetooth_manager_t *manager) {
    if (manager->is_connected) {
        bluetooth_disconnect_device(manager);
    }
    
    if (manager->object_manager) {
        g_object_unref(manager->object_manager);
        manager->object_manager = NULL;
    }
    
    if (manager->connection) {
        g_object_unref(manager->connection);
        manager->connection = NULL;
    }
    
    memset(manager, 0, sizeof(bluetooth_manager_t));
}


int bluetooth_load_paired_devices(bluetooth_manager_t *manager) {
    manager->num_devices = 0;
    
    GList *objects = g_dbus_object_manager_get_objects(manager->object_manager);
    
    for (GList *l = objects; l != NULL; l = l->next) {
        GDBusObject *object = G_DBUS_OBJECT(l->data);
        const char *object_path = g_dbus_object_get_object_path(object);
        
        // Check if this is a device object
        if (strstr(object_path, "/dev_")) {
            GDBusInterface *device_interface = 
                g_dbus_object_get_interface(object, "org.bluez.Device1");
            
            if (device_interface) {
                GVariant *address_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Address");
                GVariant *name_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Name");
                GVariant *paired_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Paired");
                GVariant *connected_variant = g_dbus_proxy_get_cached_property(
                    G_DBUS_PROXY(device_interface), "Connected");
                
                if (address_variant && paired_variant) {
                    gboolean is_paired = g_variant_get_boolean(paired_variant);
                    
                    if (is_paired && manager->num_devices < MAX_DEVICES) {
                        const char *address = g_variant_get_string(address_variant, NULL);
                        const char *name = name_variant ? 
                            g_variant_get_string(name_variant, NULL) : "Unknown";
                        gboolean is_connected = connected_variant ? 
                            g_variant_get_boolean(connected_variant) : FALSE;
                        
                        strcpy(manager->devices[manager->num_devices].address, address);
                        strcpy(manager->devices[manager->num_devices].name, name);
                        manager->devices[manager->num_devices].is_paired = TRUE;
                        manager->devices[manager->num_devices].is_connected = is_connected;
                        manager->devices[manager->num_devices].is_audio_device = TRUE;
                        
                        manager->num_devices++;
                    }
                }
                
                g_object_unref(device_interface);
            }
        }
    }
    
    g_list_free_full(objects, g_object_unref);
    return manager->num_devices;
}



int bluetooth_check_bluealsa_health(void) {
    printf("🔍 Checking BlueALSA service health...\n");
    
    // Check if BlueALSA process is running
    if (system("pgrep bluealsa > /dev/null 2>&1") != 0) {
        printf("❌ BlueALSA service not running\n");
        return -1;
    }
    
    // Check if BlueALSA is responsive
    if (system("timeout 3 bluealsa-aplay -l > /dev/null 2>&1") != 0) {
        printf("❌ BlueALSA service not responsive\n");
        
        // Try to restart BlueALSA
        printf("🔄 Restarting BlueALSA service...\n");
        if (system("sudo systemctl restart bluealsa") != 0) {
            printf("⚠️  Warning: Failed to restart BlueALSA service\n");
        }
        sleep(2);
        
        if (system("pgrep bluealsa > /dev/null 2>&1") == 0) {
            printf("✅ BlueALSA service restarted\n");
            return 0;
        } else {
            printf("❌ Failed to restart BlueALSA service\n");
            return -1;
        }
    }
    
    printf("✅ BlueALSA service is healthy\n");
    return 0;
}

int bluetooth_connect_device(bluetooth_manager_t *manager, const char *device_address) {
    printf("📱 Connecting to %s\n", device_address);
    
    char object_path[256];
    snprintf(object_path, sizeof(object_path), "/org/bluez/hci0/dev_%s", device_address);
    
    // Replace colons with underscores in path
    for (int i = 0; object_path[i]; i++) {
        if (object_path[i] == ':') object_path[i] = '_';
    }
    
    printf("🔗 Using D-Bus object path: %s\n", object_path);
    
    GError *error = NULL;
    GDBusProxy *device_proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL,
        "org.bluez",
        object_path,
        "org.bluez.Device1",
        NULL,
        &error
    );
    
    if (error) {
        printf("❌ Failed to create D-Bus proxy: %s\n", error->message);
        g_error_free(error);
        return -1;
    }
    
    printf("🔄 Calling Connect method via D-Bus...\n");
    
    GVariant *result = g_dbus_proxy_call_sync(
        device_proxy,
        "Connect",
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        10000,  // 10 second timeout for connection
        NULL,
        &error
    );
    
    if (error) {
        printf("❌ D-Bus Connect method failed: %s\n", error->message);
        g_error_free(error);
        g_object_unref(device_proxy);
        return -1;
    }
    
    if (result) {
        printf("✅ D-Bus Connect method succeeded\n");
        g_variant_unref(result);
        
        // Update manager state
        strncpy(manager->connected_device, device_address, sizeof(manager->connected_device) - 1);
        manager->is_connected = true;
        
        // Play connection notification
        char bt_device_id[128];
        snprintf(bt_device_id, sizeof(bt_device_id), "bluealsa:DEV=%s,PROFILE=a2dp", device_address);
        
        // Test and potentially switch to Bluetooth audio
        printf("🔄 Testing Bluetooth audio device...\n");
        if (audio_test_device_with_notification(bt_device_id, "./assets/sounds/bt_connect.wav") == 0) {
            printf("🎉 Bluetooth audio working! Device ready for CD playback\n");
        }
        
        printf("✅ Successfully connected to Bluetooth device\n");
    }
    
    g_object_unref(device_proxy);
    return 0;
}
