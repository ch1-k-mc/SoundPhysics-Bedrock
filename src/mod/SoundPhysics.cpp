// SPDX-License-Identifier: GPL-3.0-only
#include "mod/SoundPhysics.h"

#include "ll/api/mod/RegisterHelper.h"

#include <windows.h>

#include "audio/Engine.h"
#include "config/Config.h"

namespace sound_physics {

namespace {
bool gInstalled = false;
}

SoundPhysics& SoundPhysics::getInstance() {
    static SoundPhysics instance;
    return instance;
}

bool SoundPhysics::load() {
    loadConfig(getSelf().getConfigDir());
    if (!config().enabled) return true;
#ifndef SOUNDPHYSICS_RELEASE
    if (GetEnvironmentVariableW(L"SOUNDPHYSICS_ENABLE", nullptr, 0) == 0) return true;
    if (GetEnvironmentVariableW(L"SOUNDPHYSICS_LOG", nullptr, 0) > 0) config().environmentLogging = true;
#endif
    gInstalled = audio::install();
    getSelf().getLogger().info("Sound processing installed: {}", gInstalled);
    return true;
}

bool SoundPhysics::enable() { return true; }

bool SoundPhysics::disable() {
    if (gInstalled) {
        audio::logSummary();
        audio::uninstall();
        gInstalled = false;
    }
    return true;
}

}

LL_REGISTER_MOD(sound_physics::SoundPhysics, sound_physics::SoundPhysics::getInstance());
