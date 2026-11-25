// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) SoundPhysics.java, ReflectedAudio.java, ported to C++ and modified.
#pragma once

#include <array>
#include <string_view>

class BlockSource;

namespace sound_physics {

struct D3 {
    double x = 0, y = 0, z = 0;
};

struct EvalInput {
    D3               soundPos;
    D3               listenerPos;
    std::string_view name;
    std::string_view category;
    float            maxDistance = 16.0F;
};

struct EvalResult {
    std::array<float, 4> sendGain{0, 0, 0, 0};
    std::array<float, 4> sendCutoff{1, 1, 1, 1};
    float                directCutoff = 1.0F;
    float                directGain   = 1.0F;
    bool                 hasNewPos    = false;
    D3                   newPos;
    double               occlusion      = 0.0;
    int                  sharedAirspace = 0;
};

EvalResult evaluateEnvironment(BlockSource& region, EvalInput const& input);

}
