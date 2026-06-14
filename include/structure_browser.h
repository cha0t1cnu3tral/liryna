#ifndef LIRYNA_STRUCTURE_BROWSER_H
#define LIRYNA_STRUCTURE_BROWSER_H

#include <stdbool.h>
#include <stddef.h>

#include "structure_builder.h"

#define STRUCTURE_BROWSER_MAX_ENTRIES 64
#define STRUCTURE_BROWSER_PATH_CAPACITY 256
#define STRUCTURE_BROWSER_NAME_CAPACITY 128

typedef struct StructureBrowserEntry
{
    char path[STRUCTURE_BROWSER_PATH_CAPACITY];
    char filename[STRUCTURE_BROWSER_NAME_CAPACITY];
    char name[STRUCTURE_BROWSER_NAME_CAPACITY];
    StructureSaveKind save_kind;
    int width;
    int height;
} StructureBrowserEntry;

typedef struct StructureBrowserSettings
{
    char path[STRUCTURE_BROWSER_PATH_CAPACITY];
    char name[STRUCTURE_BROWSER_NAME_CAPACITY];
    StructureSaveKind save_kind;
    int width;
    int height;
    bool allowed_biomes[BIOME_TYPE_COUNT];
    bool allowed_support_tiles[TILE_ID_COUNT];
} StructureBrowserSettings;

int structure_browser_list(StructureBrowserEntry *entries,
                           int entry_capacity,
                           char *out_error,
                           size_t out_error_size);
bool structure_browser_load_settings(const char *path,
                                     StructureBrowserSettings *out_settings,
                                     char *out_error,
                                     size_t out_error_size);
bool structure_browser_save_settings(const StructureBrowserSettings *settings,
                                     char *out_path,
                                     size_t out_path_size,
                                     char *out_error,
                                     size_t out_error_size);
bool structure_browser_load_into_world(const StructureBrowserSettings *settings,
                                       World *world,
                                       StructureBuilderConfig *config,
                                       char *out_error,
                                       size_t out_error_size);

#endif
