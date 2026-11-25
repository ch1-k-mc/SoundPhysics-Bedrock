// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "ll/api/mod/NativeMod.h"

namespace sound_physics {

class SoundPhysics {

public:
    static SoundPhysics& getInstance();

    SoundPhysics() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();

    bool enable();

    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

}
