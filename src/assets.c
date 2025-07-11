#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

int assets_init(void) {
    struct stat st = {0};
    
    // Check if assets directory exists
    if (stat(ASSETS_BASE_DIR, &st) == -1) {
        printf("❌ Assets directory not found: %s\n", ASSETS_BASE_DIR);
        return -1;
    }
    
    // Check if sounds directory exists
    if (stat(SOUNDS_DIR, &st) == -1) {
        printf("❌ Sounds directory not found: %s\n", SOUNDS_DIR);
        return -1;
    }
    
    printf("✅ Assets directory initialized successfully\n");
    return 0;
}

bool assets_file_exists(const char *asset_path) {
    return access(asset_path, F_OK) == 0;
}

int assets_get_full_path(const char *asset_path, char *full_path, size_t path_size) {
    char cwd[256];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        return -1;
    }
    
    snprintf(full_path, path_size, "%s/%s", cwd, asset_path);
    return assets_file_exists(full_path) ? 0 : -1;
}

void assets_list_available(void) {
    printf("📁 Available audio assets:\n");
    
    DIR *dir = opendir(SOUNDS_DIR);
    if (dir == NULL) {
        printf("❌ Cannot open sounds directory\n");
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".wav") != NULL) {
            printf("  🔊 %s\n", entry->d_name);
        }
    }
    
    closedir(dir);
}
