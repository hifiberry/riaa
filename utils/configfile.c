/*
 * configfile.c - Generic LADSPA plugin configuration file support
 * 
 * Provides utilities to load plugin configuration from INI files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include "configfile.h"
#include "ini.h"

// Initialize empty config
void config_init(PluginConfig *config) {
    if (config) {
        config->count = 0;
        memset(config->entries, 0, sizeof(config->entries));
    }
}

// Build config file path: ~/.state/ladspa/<pluginname>.ini
char* config_build_path(const char *plugin_name) {
    if (!plugin_name) {
        return NULL;
    }
    
    // Get home directory
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    
    if (!home) {
        return NULL;
    }
    
    // Build path: ~/.state/ladspa/<pluginname>.ini
    size_t len = strlen(home) + strlen("/.state/ladspa/") + strlen(plugin_name) + strlen(".ini") + 1;
    char *path = (char *)malloc(len);
    
    if (path) {
        snprintf(path, len, "%s/.state/ladspa/%s.ini", home, plugin_name);
    }
    
    return path;
}

// INI parser handler callback
static int config_ini_handler(void* user, const char* section, const char* name, const char* value) {
    PluginConfig *config = (PluginConfig *)user;
    
    // Ignore sections, just use flat key=value
    (void)section;
    
    if (config->count >= MAX_CONFIG_ENTRIES) {
        return 0; // Config full
    }
    
    // Store the key-value pair
    strncpy(config->entries[config->count].key, name, MAX_KEY_LENGTH - 1);
    config->entries[config->count].key[MAX_KEY_LENGTH - 1] = '\0';
    
    strncpy(config->entries[config->count].value, value, MAX_VALUE_LENGTH - 1);
    config->entries[config->count].value[MAX_VALUE_LENGTH - 1] = '\0';
    
    config->count++;
    
    return 1; // Success
}

// Load configuration from INI file
int config_load(const char *filepath, PluginConfig *config) {
    if (!filepath || !config) {
        return -1;
    }
    
    // Check if file exists
    if (access(filepath, F_OK) != 0) {
        // File doesn't exist, not an error - just use defaults
        return 0;
    }
    
    // Initialize config before loading
    config_init(config);
    
    // Parse INI file
    int result = ini_parse(filepath, config_ini_handler, config);
    
    return result;
}

// Get a configuration value by key
const char* config_get(const PluginConfig *config, const char *key) {
    if (!config || !key) {
        return NULL;
    }
    
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            return config->entries[i].value;
        }
    }
    
    return NULL;
}

// Get a configuration value as float
LADSPA_Data config_get_float(const PluginConfig *config, const char *key, LADSPA_Data default_value) {
    const char *value_str = config_get(config, key);
    
    if (!value_str) {
        return default_value;
    }
    
    // Check for boolean strings (case-insensitive)
    if (strcasecmp(value_str, "yes") == 0 || strcasecmp(value_str, "true") == 0) {
        return 1.0f;
    }
    if (strcasecmp(value_str, "no") == 0 || strcasecmp(value_str, "false") == 0) {
        return 0.0f;
    }
    
    // Try to parse as numeric value
    char *endptr;
    float value = strtof(value_str, &endptr);
    
    // Check if conversion was successful
    if (endptr == value_str || *endptr != '\0') {
        return default_value;
    }
    
    return (LADSPA_Data)value;
}

// Set or update a configuration value
int config_set(PluginConfig *config, const char *key, const char *value) {
    if (!config || !key || !value) {
        return -1;
    }
    
    // Check if key already exists - update it
    for (int i = 0; i < config->count; i++) {
        if (strcmp(config->entries[i].key, key) == 0) {
            strncpy(config->entries[i].value, value, MAX_VALUE_LENGTH - 1);
            config->entries[i].value[MAX_VALUE_LENGTH - 1] = '\0';
            return 0;
        }
    }
    
    // Key doesn't exist - add new entry
    if (config->count >= MAX_CONFIG_ENTRIES) {
        return -1; // Config full
    }
    
    strncpy(config->entries[config->count].key, key, MAX_KEY_LENGTH - 1);
    config->entries[config->count].key[MAX_KEY_LENGTH - 1] = '\0';
    
    strncpy(config->entries[config->count].value, value, MAX_VALUE_LENGTH - 1);
    config->entries[config->count].value[MAX_VALUE_LENGTH - 1] = '\0';
    
    config->count++;
    return 0;
}

// Save configuration to INI file
int config_save(const char *filepath, const PluginConfig *config) {
    if (!filepath || !config) {
        return -1;
    }
    
    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        return -1;
    }
    
    // Write header comment
    fprintf(fp, "# LADSPA Plugin Configuration\n");
    fprintf(fp, "# Automatically saved settings\n\n");
    
    // Write all key-value pairs
    for (int i = 0; i < config->count; i++) {
        fprintf(fp, "%s = %s\n", config->entries[i].key, config->entries[i].value);
    }
    
    fclose(fp);
    return 0;
}
