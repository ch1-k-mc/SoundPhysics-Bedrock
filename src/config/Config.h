// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) config/SoundPhysicsConfig.java, ported to C++ and modified.
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace sound_physics {

struct Config {
    bool   enabled                         = true;
    double attenuationFactor               = 1.0;
    double reverbAttenuationDistance       = 0.0;
    double reverbGain                      = 1.0;
    double reverbBrightness                = 1.0;
    double reverbDistance                  = 1.5;
    double blockAbsorption                 = 1.0;
    double occlusionVariation              = 0.35;
    double defaultBlockReflectivity        = 0.5;
    double defaultBlockOcclusionFactor     = 1.0;
    double soundDistanceAllowance          = 4.0;
    double airAbsorption                   = 1.0;
    double underwaterFilter                = 0.9;
    bool   evaluateAmbientSounds           = false;
    int    environmentEvaluationRayCount   = 32;
    int    environmentEvaluationRayBounces = 4;
    double nonFullBlockOcclusionFactor     = 0.25;
    int    maxOcclusionRays                = 16;
    double maxOcclusion                    = 64.0;
    bool   strictOcclusion                 = false;
    bool   soundDirectionEvaluation        = true;
    bool   redirectNonOccludedSounds       = true;
    bool   updateMovingSounds              = false;
    int    soundUpdateInterval             = 5;
    double maxSoundProcessingDistance      = 512.0;
    bool   unsafeLevelAccess               = false;
    int    levelCloneRange                 = 4;
    int    levelCloneMaxRetainTicks        = 20;
    int    levelCloneMaxRetainBlockDistance = 16;
    bool   debugLogging                    = false;
    bool   occlusionLogging                = false;
    bool   environmentLogging              = false;
    bool   performanceLogging              = false;
    bool   renderSoundBounces              = false;
    bool   renderOcclusion                 = false;
    bool   simpleVoiceChatIntegration      = true;
    bool   simpleVoiceChatHearSelf         = false;
};

Config& config();

std::unordered_map<std::string, float>& reflectivityTable();
std::unordered_map<std::string, float>& occlusionTable();
std::unordered_map<std::string, int>&   soundRateTable();

void loadConfig(std::filesystem::path const& dir);

}
