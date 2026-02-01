/*
 * configfile.h - Generic LADSPA plugin configuration file support
 * 
 * Provides utilities to load plugin configuration from INI files
 * located in ~/.state/ladspa/<pluginname>.ini
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <ladspa.h>

#define MAX_CONFIG_ENTRIES 64
#define MAX_KEY_LENGTH 64
#define MAX_VALUE_LENGTH 64

// Configuration entry
typedef struct {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
} ConfigEntry;

// Configuration storage
typedef struct {
    ConfigEntry entries[MAX_CONFIG_ENTRIES];
    int count;
} PluginConfig;

// Build config file path: ~/.state/ladspa/<pluginname>.ini
// Returns allocated string that must be freed, or NULL on error
char* config_build_path(const char *plugin_name);

// Load configuration from INI file
// Returns 0 on success, non-zero on error
int config_load(const char *filepath, PluginConfig *config);

// Get a configuration value by key (control port name)
// Returns the value string, or NULL if not found
const char* config_get(const PluginConfig *config, const char *key);

// Get a configuration value as float
// Returns the parsed float value, or default_value if not found or invalid
LADSPA_Data config_get_float(const PluginConfig *config, const char *key, LADSPA_Data default_value);

// Set or update a configuration value
// Returns 0 on success, -1 on error
int config_set(PluginConfig *config, const char *key, const char *value);

// Save configuration to INI file
// Returns 0 on success, non-zero on error
int config_save(const char *filepath, const PluginConfig *config);

// Initialize empty config
void config_init(PluginConfig *config);

#endif // CONFIGFILE_H
