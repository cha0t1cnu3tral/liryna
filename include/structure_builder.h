#ifndef LIRYNA_STRUCTURE_BUILDER_H
#define LIRYNA_STRUCTURE_BUILDER_H

#include <stdbool.h>
#include <stddef.h>

#include "../src/world/biome_tile_sets.h"
#include "../src/world/world.h"

#define STRUCTURE_BUILDER_NAME_CAPACITY 64

typedef enum StructureSaveKind
{
    STRUCTURE_SAVE_KIND_STRUCTURE = 0,
    STRUCTURE_SAVE_KIND_VILLAGE,
    STRUCTURE_SAVE_KIND_COUNT
} StructureSaveKind;

typedef struct StructureBuilderConfig
{
    char name[STRUCTURE_BUILDER_NAME_CAPACITY];
    BiomeType builder_biome;
    StructureSaveKind save_kind;
    bool allowed_biomes[BIOME_TYPE_COUNT];
    bool allowed_support_tiles[TILE_ID_COUNT];
} StructureBuilderConfig;

void structure_builder_config_reset(StructureBuilderConfig *config);
bool structure_builder_is_support_tile(TileId tile_id);
TileId structure_builder_biome_primary_tile(BiomeType biome_type);
void structure_builder_set_allowed_supports_for_biome(StructureBuilderConfig *config,
                                                      BiomeType biome_type);
const char *structure_builder_save_kind_name(StructureSaveKind save_kind);
bool structure_builder_save(const World *world,
                            const StructureBuilderConfig *config,
                            char *out_path,
                            size_t out_path_size,
                            char *out_error,
                            size_t out_error_size);

#endif
