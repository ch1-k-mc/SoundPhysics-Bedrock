// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) config/blocksound/*.java, config/SoundTypes.java, ported to C++ and modified.
#include "physics/Materials.h"

#include <array>
#include <string>
#include <unordered_map>
#include <utility>

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"

#include "config/Config.h"

namespace sound_physics {

namespace {

std::unordered_map<BlockType const*, BlockAcoustics> gCache;

constexpr std::array<std::pair<std::string_view, std::string_view>, 71> kRules{{
    {"weeping_vines", "WEEPING_VINES"},
    {"twisting_vines", "TWISTING_VINES"},
    {"cave_vines", "VINE"},
    {"vine", "VINE"},
    {"sweet_berry", "SWEET_BERRY_BUSH"},
    {"spore_blossom", "SPORE_BLOSSOM"},
    {"small_dripleaf", "SMALL_DRIPLEAF"},
    {"roots", "ROOTS"},
    {"pointed_dripstone", "POINTED_DRIPSTONE"},
    {"scaffolding", "SCAFFOLDING"},
    {"glow_lichen", "GLOW_LICHEN"},
    {"wheat", "CROP"},
    {"carrots", "CROP"},
    {"potatoes", "CROP"},
    {"beetroot", "CROP"},
    {"fungus", "FUNGUS"},
    {"waterlily", "LILY_PAD"},
    {"lily_pad", "LILY_PAD"},
    {"large_amethyst_bud", "LARGE_AMETHYST_BUD"},
    {"medium_amethyst_bud", "MEDIUM_AMETHYST_BUD"},
    {"small_amethyst_bud", "SMALL_AMETHYST_BUD"},
    {"ladder", "LADDER"},
    {"chain", "CHAIN"},
    {"moss_carpet", "MOSS_CARPET"},
    {"moss", "MOSS"},
    {"powder_snow", "POWDER_SNOW"},
    {"snow", "SNOW"},
    {"bamboo_sapling", "BAMBOO_SAPLING"},
    {"bamboo", "BAMBOO"},
    {"wool", "WOOL"},
    {"carpet", "WOOL"},
    {"honey_block", "HONEY_BLOCK"},
    {"glass", "GLASS"},
    {"ice", "GLASS"},
    {"coral_block", "CORAL_BLOCK"},
    {"soul_sand", "SOUL_SAND"},
    {"soul_soil", "SOUL_SOIL"},
    {"nether_gold_ore", "NETHER_GOLD_ORE"},
    {"quartz_ore", "NETHER_ORE"},
    {"netherrack", "NETHERRACK"},
    {"nether_brick", "NETHER_BRICKS"},
    {"netherite", "NETHERITE_BLOCK"},
    {"copper", "COPPER"},
    {"iron", "METAL"},
    {"gold_block", "METAL"},
    {"diamond_block", "METAL"},
    {"emerald_block", "METAL"},
    {"amethyst", "AMETHYST"},
    {"tuff", "TUFF"},
    {"basalt", "BASALT"},
    {"calcite", "CALCITE"},
    {"bone_block", "BONE_BLOCK"},
    {"deepslate_brick", "DEEPSLATE_BRICKS"},
    {"deepslate_tile", "DEEPSLATE_TILES"},
    {"polished_deepslate", "POLISHED_DEEPSLATE"},
    {"deepslate", "DEEPSLATE"},
    {"crimson_stem", "STEM"},
    {"warped_stem", "STEM"},
    {"hyphae", "STEM"},
    {"log", "WOOD"},
    {"wood", "WOOD"},
    {"planks", "WOOD"},
    {"gravel", "GRAVEL"},
    {"grass", "GRASS"},
    {"leaves", "GRASS"},
    {"dirt", "GRAVEL"},
    {"sand", "SAND"},
    {"stone", "STONE"},
    {"brick", "STONE"},
    {"ore", "STONE"},
    {"obsidian", "STONE"},
}};

float resolve(
    std::string const&                             id,
    Block const&                                   block,
    std::unordered_map<std::string, float> const&  table,
    float                                          fallback
) {
    if (auto it = table.find(id); it != table.end()) return it->second;
    std::string const* best = nullptr;
    float              value = fallback;
    for (auto const& [key, v] : table) {
        if (key.empty() || key[0] != '#') continue;
        if (best && *best <= key) continue;
        if (!block.hasTag(HashedString(key.substr(1)))) continue;
        best  = &key;
        value = v;
    }
    if (best) return value;
    if (auto it = table.find(std::string(classifySoundType(id))); it != table.end()) return it->second;
    return fallback;
}

}

std::string_view classifySoundType(std::string_view blockName) {
    for (auto const& [word, type] : kRules) {
        if (blockName.find(word) != std::string_view::npos) return type;
    }
    return "STONE";
}

BlockAcoustics const& acousticsOf(Block const& block) {
    BlockType const* type = &block.getBlockType();
    if (auto it = gCache.find(type); it != gCache.end()) return it->second;
    std::string const& id     = block.getTypeName();
    bool const         water  = id == "minecraft:water" || id == "minecraft:flowing_water";
    bool const         lava   = id == "minecraft:lava" || id == "minecraft:flowing_lava";
    std::string const  lookup = water ? "minecraft:water" : (lava ? "minecraft:lava" : id);
    auto const&        cfg    = config();
    BlockAcoustics     a{
        resolve(lookup, block, reflectivityTable(), static_cast<float>(cfg.defaultBlockReflectivity)),
        resolve(lookup, block, occlusionTable(), static_cast<float>(cfg.defaultBlockOcclusionFactor)),
        water || lava,
        water,
    };
    return gCache.emplace(type, a).first->second;
}

void clearAcousticsCache() { gCache.clear(); }

}
