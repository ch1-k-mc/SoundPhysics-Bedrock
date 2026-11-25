// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) config/blocksound/*.java, ported to C++ and modified.
#pragma once

#include <string_view>

class Block;

namespace sound_physics {

struct BlockAcoustics {
    float reflectivity;
    float occlusion;
    bool  liquid;
    bool  water;
};

std::string_view classifySoundType(std::string_view blockName);

BlockAcoustics const& acousticsOf(Block const& block);
void                  clearAcousticsCache();

}
