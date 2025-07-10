#include "bluetooth_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h> 

int bluetooth_init(bluetooth_manager_t *manager) {
    memset(manager, 0, sizeof(bluetooth_manager_t));
    manager->is_connected = false;
    manager->num_devices = 0;
    return 0;
}

static int get_device_info(const char *mac_address, char *name, size_t name_size, 
                          bool *is_paired, bool *is_connected) {
    char command[256];
    snprintf(command, sizeof(command), "bluetoothctl info %s", mac_address);
    
    FILE *fp = popen(command, "r");
    if (!fp) return -1;
    
    char line[256];
    strcpy(name, "Unknown");
    *is_paired = false;
    *is_connected = false;
    
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "\tName: ", 7) == 0) {
            char *name_start = line + 7;
            char *newline = strchr(name_start, '\n');
            if (newline) *newline = '\0';
            snprintf(name, name_size, "%s", name_start);
            name[name_size - 1] = '\0';
        } else if (strncmp(line, "\tPaired: yes", 12) == 0) {
            *is_paired = true;
        } else if (strncmp(line, "\tConnected: yes", 15) == 0) {
            *is_connected = true;
        }
    }
    
    pclose(fp);
    return 0;
}

static void generate_device_name(const char *mac_address, char *name, size_t name_size) {
    // Generate friendly name from MAC address (last 4 chars)
    const char *mac_clean = strrchr(mac_address, ':');
    if (mac_clean && strlen(mac_clean) > 3) {
        snprintf(name, name_size, "Device-%s", mac_clean + 1);
    } else {
        snprintf(name, name_size, "Device-%04X", (unsigned int)(time(NULL) & 0xFFFF));
    }
}

int bluetooth_scan_devices(bluetooth_manager_t *manager) {
    manager->num_devices = 0;
    manager->is_scanning = true;
    
    printf("🔍 Scanning for Bluetooth devices...\n");
    
    // Create pipes for bluetoothctl communication
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
        manager->is_scanning = false;
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: bluetoothctl
        close(stdin_pipe[1]);   // Close write end
        close(stdout_pipe[0]);  // Close read end
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        
        execl("/usr/bin/bluetoothctl", "bluetoothctl", NULL);
        exit(1);
    } else if (pid > 0) {
        // Parent process
        close(stdin_pipe[0]);   // Close read end
        close(stdout_pipe[1]);  // Close write end
        
        FILE *bt_stdin = fdopen(stdin_pipe[1], "w");
        FILE *bt_stdout = fdopen(stdout_pipe[0], "r");
        
        if (!bt_stdin || !bt_stdout) {
            manager->is_scanning = false;
            return -1;
        }
        
        // Start scanning
        fprintf(bt_stdin, "scan on\n");
        fflush(bt_stdin);
        
        // Extended scanning time (8 seconds like Python version)
        printf("Scanning for 8 seconds...\n");
        sleep(8);
        
        // Query devices multiple times
        for (int round = 0; round < 2; round++) {
            fprintf(bt_stdin, "devices\n");
            fflush(bt_stdin);
            sleep(1);
            
            fprintf(bt_stdin, "paired-devices\n");
            fflush(bt_stdin);
            sleep(1);
        }
        
        // Stop scanning and quit
        fprintf(bt_stdin, "scan off\n");
        fprintf(bt_stdin, "quit\n");
        fflush(bt_stdin);
        
        // Read and parse output
        char line[512];
        char all_devices[MAX_DEVICES][18]; // Store MAC addresses to avoid duplicates
        int device_count = 0;
        
        while (fgets(line, sizeof(line), bt_stdout) && device_count < MAX_DEVICES) {
            if (strncmp(line, "Device ", 7) == 0) {
                char mac[18];
                char raw_name[256];
                
                if (sscanf(line, "Device %17s %255[^\n]", mac, raw_name) >= 1) {
                    // Check for duplicates
                    bool duplicate = false;
                    for (int i = 0; i < device_count; i++) {
                        if (strcmp(all_devices[i], mac) == 0) {
                            duplicate = true;
                            break;
                        }
                    }
                    
                    if (!duplicate && manager->num_devices < MAX_DEVICES) {
                        strcpy(all_devices[device_count], mac);
                        strcpy(manager->devices[manager->num_devices].address, mac);
                        
                        // Get detailed device info
                        char device_name[MAX_DEVICE_NAME];
                        bool is_paired = false, is_connected = false;
                        
                        // Try to get proper device name and status
                        if (get_device_info(mac, device_name, sizeof(device_name), 
                                          &is_paired, &is_connected) == 0 && 
                            strcmp(device_name, "Unknown") != 0 && strlen(device_name) > 0) {
                            // Use the detailed device name
                            strcpy(manager->devices[manager->num_devices].name, device_name);
                        } else if (sscanf(line, "Device %17s %255[^\n]", mac, raw_name) == 2 && 
                                  strlen(raw_name) > 0 && strcmp(raw_name, "Unknown Device") != 0) {
                            // Use the raw name from bluetoothctl output
                            strcpy(manager->devices[manager->num_devices].name, raw_name);
                        } else {
                            // Generate a friendly name from MAC address
                            generate_device_name(mac, device_name, sizeof(device_name));
                            strcpy(manager->devices[manager->num_devices].name, device_name);
                        }
                        
                        // Store device status
                        manager->devices[manager->num_devices].is_paired = is_paired;
                        manager->devices[manager->num_devices].is_connected = is_connected;
                        manager->devices[manager->num_devices].is_audio_device = true; // Assume all are audio capable
                        
                        printf("Found: %s %s (%s%s)\n", 
                               manager->devices[manager->num_devices].name,
                               mac,
                               is_connected ? "●" : (is_paired ? "○" : "◦"),
                               is_connected ? " Connected" : (is_paired ? " Paired" : " New"));
                        
                        manager->num_devices++;
                        device_count++;
                    }
                }
            }
        }
        
        fclose(bt_stdin);
        fclose(bt_stdout);
        
        // Wait for child process
        int status;
        waitpid(pid, &status, 0);
    }
    
    manager->is_scanning = false;
    printf("📱 Found %d Bluetooth devices\n", manager->num_devices);
    return manager->num_devices;
}


int bluetooth_pair_device(bluetooth_manager_t *manager, const char *device_address) {
    char command[256];
    snprintf(command, sizeof(command), "bluetoothctl pair %s", device_address);
    
    printf("📱 Pairing with %s\n", device_address);
    
    int result = system(command);
    if (result == 0) {
        printf("✅ Successfully paired with %s\n", device_address);
        
        // Auto-connect after successful pairing
        printf("🔗 Auto-connecting...\n");
        sleep(1);
        if (bluetooth_connect_device(manager, device_address) == 0) {
            printf("✅ Paired and connected successfully\n");
        }
        return 0;
    } else {
        printf("❌ Failed to pair with %s\n", device_address);
        return -1;
    }
}

int bluetooth_connect_device(bluetooth_manager_t *manager, const char *device_address) {
    char command[256];
    snprintf(command, sizeof(command), "bluetoothctl connect %s", device_address);
    
    printf("📱 Connecting to %s\n", device_address);
    
    int result = system(command);
    if (result == 0) {
        printf("✅ Successfully connected to %s\n", device_address);
        strncpy(manager->connected_device, device_address, sizeof(manager->connected_device) - 1);
        manager->is_connected = true;
        return 0;
    } else {
        printf("❌ Failed to connect to %s\n", device_address);
        return -1;
    }
}

int bluetooth_disconnect_device(bluetooth_manager_t *manager) {
    if (!manager->is_connected) {
        return 0;
    }
    
    char command[256];
    snprintf(command, sizeof(command), "bluetoothctl disconnect %s", manager->connected_device);
    
    printf("📱 Disconnecting from %s\n", manager->connected_device);
    
    int result = system(command);
    if (result == 0) {
        printf("✅ Successfully disconnected\n");
        manager->is_connected = false;
        memset(manager->connected_device, 0, sizeof(manager->connected_device));
        return 0;
    } else {
        printf("❌ Failed to disconnect\n");
        return -1;
    }
}

int bluetooth_get_audio_device_name(bluetooth_manager_t *manager, char *device_name, size_t size) {
    if (!manager->is_connected) {
        return -1;
    }
    
    snprintf(device_name, size, "bluealsa");
    return 0;
}

int bluetooth_load_paired_devices(bluetooth_manager_t *manager) {
    manager->num_devices = 0;
    
    printf("Loading paired and connected Bluetooth devices...\n");
    
    // Create pipes for bluetoothctl communication
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
        return -1;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: bluetoothctl
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        
        execl("/usr/bin/bluetoothctl", "bluetoothctl", NULL);
        exit(1);
    } else if (pid > 0) {
        // Parent process
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        
        FILE *bt_stdin = fdopen(stdin_pipe[1], "w");
        FILE *bt_stdout = fdopen(stdout_pipe[0], "r");
        
        if (!bt_stdin || !bt_stdout) {
            return -1;
        }
        
        // Send commands to get devices
        fprintf(bt_stdin, "devices\n");
        fflush(bt_stdin);
        sleep(1);
        
        fprintf(bt_stdin, "quit\n");
        fflush(bt_stdin);
        
        // Read and parse output
        char line[256];
        while (fgets(line, sizeof(line), bt_stdout) && manager->num_devices < MAX_DEVICES) {
            if (strncmp(line, "Device ", 7) == 0) {
                char mac[18], name[128];
                if (sscanf(line, "Device %17s %127[^\n]", mac, name) == 2) {
                    // Check if device is paired
                    char cmd[256];
                    snprintf(cmd, sizeof(cmd), "bluetoothctl info %s | grep -q 'Paired: yes'", mac);
                    if (system(cmd) == 0) {
                        // Store paired device
                        strcpy(manager->devices[manager->num_devices].address, mac);
                        strcpy(manager->devices[manager->num_devices].name, name);
                        manager->devices[manager->num_devices].is_paired = true;
                        
                        // Check connection status
                        snprintf(cmd, sizeof(cmd), "bluetoothctl info %s | grep -q 'Connected: yes'", mac);
                        manager->devices[manager->num_devices].is_connected = (system(cmd) == 0);
                        
                        manager->devices[manager->num_devices].is_audio_device = true;
                        manager->num_devices++;
                        
                        printf("Found paired device: %s (%s)\n", name, mac);
                    }
                }
            }
        }
        
        fclose(bt_stdin);
        fclose(bt_stdout);
        
        int status;
        waitpid(pid, &status, 0);
    }
    
    printf("Loaded %d paired/connected devices\n", manager->num_devices);
    return manager->num_devices;
}


int bluetooth_refresh_device_status(bluetooth_manager_t *manager) {
    // Update connection status for all known devices
    for (int i = 0; i < manager->num_devices; i++) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "bluetoothctl info %s | grep -q 'Connected: yes'", 
                 manager->devices[i].address);
        manager->devices[i].is_connected = (system(cmd) == 0);
    }
    return 0;
}


void bluetooth_cleanup(bluetooth_manager_t *manager) {
    if (manager->is_connected) {
        bluetooth_disconnect_device(manager);
    }
    memset(manager, 0, sizeof(bluetooth_manager_t));
}
