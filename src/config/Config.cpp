// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) config/SoundPhysicsConfig.java, config/*Config.java, ported to C++ and modified.
#include "config/Config.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <utility>
#include <variant>
#include <vector>

namespace sound_physics {

namespace {

Config                                 gConfig;
std::unordered_map<std::string, float> gReflectivity;
std::unordered_map<std::string, float> gOcclusion;
std::unordered_map<std::string, int>   gSoundRates;

struct Entry {
    char const*                       key;
    std::variant<bool*, int*, double*> value;
    double                            min;
    double                            max;
};

std::vector<Entry> entries() {
    Config&      c    = gConfig;
    double const dMax = (std::numeric_limits<double>::max)();
    double const iMax = (std::numeric_limits<int>::max)();
    return {
        {"enabled", &c.enabled, 0, 1},
        {"attenuation_factor", &c.attenuationFactor, 0.1, 1},
        {"reverb_attenuation_distance", &c.reverbAttenuationDistance, 0, 512},
        {"reverb_gain", &c.reverbGain, 0.1, 2},
        {"reverb_brightness", &c.reverbBrightness, 0.1, 2},
        {"reverb_distance", &c.reverbDistance, 0.1, 16},
        {"block_absorption", &c.blockAbsorption, 0.1, 4},
        {"occlusion_variation", &c.occlusionVariation, 0, 16},
        {"default_block_reflectivity", &c.defaultBlockReflectivity, 0.1, 4},
        {"default_block_occlusion_factor", &c.defaultBlockOcclusionFactor, 0, 10},
        {"sound_distance_allowance", &c.soundDistanceAllowance, 1, 6},
        {"air_absorption", &c.airAbsorption, 0, 5},
        {"underwater_filter", &c.underwaterFilter, 0, 1},
        {"evaluate_ambient_sounds", &c.evaluateAmbientSounds, 0, 1},
        {"environment_evaluation_ray_count", &c.environmentEvaluationRayCount, 8, 64},
        {"environment_evaluation_ray_bounces", &c.environmentEvaluationRayBounces, 2, 64},
        {"non_full_block_occlusion_factor", &c.nonFullBlockOcclusionFactor, 0, 1},
        {"max_occlusion_rays", &c.maxOcclusionRays, 1, 128},
        {"max_occlusion", &c.maxOcclusion, 0, 1024},
        {"strict_occlusion", &c.strictOcclusion, 0, 1},
        {"sound_direction_evaluation", &c.soundDirectionEvaluation, 0, 1},
        {"redirect_non_occluded_sounds", &c.redirectNonOccludedSounds, 0, 1},
        {"update_moving_sounds", &c.updateMovingSounds, 0, 1},
        {"sound_update_interval", &c.soundUpdateInterval, 1, iMax},
        {"max_sound_processing_distance", &c.maxSoundProcessingDistance, 0, dMax},
        {"unsafe_level_access", &c.unsafeLevelAccess, 0, 1},
        {"level_clone_range", &c.levelCloneRange, 2, 16},
        {"level_clone_max_retain_ticks", &c.levelCloneMaxRetainTicks, 1, iMax},
        {"level_clone_max_retain_block_distance", &c.levelCloneMaxRetainBlockDistance, 1, iMax},
        {"debug_logging", &c.debugLogging, 0, 1},
        {"occlusion_logging", &c.occlusionLogging, 0, 1},
        {"environment_logging", &c.environmentLogging, 0, 1},
        {"performance_logging", &c.performanceLogging, 0, 1},
        {"render_sound_bounces", &c.renderSoundBounces, 0, 1},
        {"render_occlusion", &c.renderOcclusion, 0, 1},
        {"simple_voice_chat_integration", &c.simpleVoiceChatIntegration, 0, 1},
        {"simple_voice_chat_hear_self", &c.simpleVoiceChatHearSelf, 0, 1},
    };
}

std::string trim(std::string s) {
    auto const notSpace = [](unsigned char ch) { return ch != ' ' && ch != '\t' && ch != '\r'; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    return s;
}

std::map<std::string, std::string> readProperties(std::filesystem::path const& path) {
    std::map<std::string, std::string> out;
    std::ifstream                      in(path);
    std::string                        line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '#' && line.find('=') == std::string::npos) continue;
        auto const eq = line.find('=');
        if (eq == std::string::npos) continue;
        out[trim(line.substr(0, eq))] = trim(line.substr(eq + 1));
    }
    return out;
}

bool parseDouble(std::string const& s, double& out) {
    auto const* end = s.data() + s.size();
    auto const  r   = std::from_chars(s.data(), end, out);
    return r.ec == std::errc{} && r.ptr == end;
}

std::vector<std::pair<char const*, float>> const kDefaultReflectivity = {
    {"STONE", 1.5F},          {"NETHERITE_BLOCK", 1.5F},    {"TUFF", 1.5F},
    {"AMETHYST", 1.5F},       {"BASALT", 1.5F},             {"CALCITE", 1.5F},
    {"BONE_BLOCK", 1.5F},     {"DEEPSLATE", 1.5F},          {"DEEPSLATE_BRICKS", 1.5F},
    {"DEEPSLATE_TILES", 1.5F},{"POLISHED_DEEPSLATE", 1.5F}, {"NETHER_BRICKS", 1.5F},
    {"COPPER", 1.25F},        {"METAL", 1.25F},             {"NETHERRACK", 1.1F},
    {"NETHER_GOLD_ORE", 1.1F},{"NETHER_ORE", 1.1F},         {"GLASS", 0.75F},
    {"STEM", 0.4F},           {"WOOD", 0.4F},               {"GRAVEL", 0.3F},
    {"GRASS", 0.3F},          {"SOUL_SAND", 0.2F},          {"SOUL_SOIL", 0.2F},
    {"CORAL_BLOCK", 0.2F},    {"SAND", 0.2F},               {"SNOW", 0.15F},
    {"WOOL", 0.1F},           {"HONEY_BLOCK", 0.1F},        {"MOSS", 0.1F},
};

std::vector<std::pair<char const*, float>> const kDefaultOcclusion = {
    {"WOOL", 1.5F},           {"MOSS", 0.75F},              {"minecraft:lava", 0.75F},
    {"HONEY_BLOCK", 0.5F},    {"minecraft:water", 0.25F},   {"GLASS", 0.1F},
    {"SNOW", 0.1F},           {"POWDER_SNOW", 0.1F},        {"BAMBOO", 0.1F},
    {"BAMBOO_SAPLING", 0.1F}, {"WET_GRASS", 0.1F},          {"MOSS_CARPET", 0.1F},
    {"WEEPING_VINES", 0.0F},  {"TWISTING_VINES", 0.0F},     {"VINE", 0.0F},
    {"SWEET_BERRY_BUSH", 0.0F},{"SPORE_BLOSSOM", 0.0F},     {"SMALL_DRIPLEAF", 0.0F},
    {"ROOTS", 0.0F},          {"POINTED_DRIPSTONE", 0.0F},  {"SCAFFOLDING", 0.0F},
    {"GLOW_LICHEN", 0.0F},    {"CROP", 0.0F},               {"FUNGUS", 0.0F},
    {"LILY_PAD", 0.0F},       {"LARGE_AMETHYST_BUD", 0.0F}, {"MEDIUM_AMETHYST_BUD", 0.0F},
    {"SMALL_AMETHYST_BUD", 0.0F},{"LADDER", 0.0F},          {"CHAIN", 0.0F},
    {"minecraft:jukebox", 0.0F},
};

std::vector<std::pair<char const*, int>> const kDefaultSoundRates = {
    {"ambient.weather.rain", 0},
    {"ambient.weather.thunder", 0},
    {"ambient.weather.lightning.impact", 0},
    {"horn.call.0", 0},
    {"horn.call.1", 0},
    {"horn.call.2", 0},
    {"horn.call.3", 0},
    {"horn.call.4", 0},
    {"horn.call.5", 0},
    {"horn.call.6", 0},
    {"horn.call.7", 0},
};

template <class T>
void loadTable(
    std::filesystem::path const&                 path,
    std::vector<std::pair<char const*, T>> const& defaults,
    std::unordered_map<std::string, T>&          table
) {
    table.clear();
    for (auto const& [k, v] : defaults) table[k] = v;
    if (!std::filesystem::exists(path)) {
        std::ofstream out(path);
        for (auto const& [k, v] : defaults) out << k << '=' << v << '\n';
        return;
    }
    for (auto const& [k, v] : readProperties(path)) {
        double d = 0;
        if (parseDouble(v, d)) table[k] = static_cast<T>(d);
    }
}

}

Config&                                 config() { return gConfig; }
std::unordered_map<std::string, float>& reflectivityTable() { return gReflectivity; }
std::unordered_map<std::string, float>& occlusionTable() { return gOcclusion; }
std::unordered_map<std::string, int>&   soundRateTable() { return gSoundRates; }

void loadConfig(std::filesystem::path const& dir) {
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto const path   = dir / "soundphysics.properties";
    auto const values = readProperties(path);
    auto const list   = entries();
    bool       missing = false;
    for (auto const& e : list) {
        auto const it = values.find(e.key);
        if (it == values.end()) {
            missing = true;
            continue;
        }
        if (auto* b = std::get_if<bool*>(&e.value)) {
            **b = it->second == "true";
            continue;
        }
        double d = 0;
        if (!parseDouble(it->second, d)) continue;
        d = std::clamp(d, e.min, e.max);
        if (auto* i = std::get_if<int*>(&e.value)) **i = static_cast<int>(d);
        else **std::get_if<double*>(&e.value) = d;
    }
    if (missing) {
        std::ofstream out(path);
        for (auto const& e : list) {
            out << e.key << '=';
            if (auto* b = std::get_if<bool*>(&e.value)) out << (**b ? "true" : "false");
            else if (auto* i = std::get_if<int*>(&e.value)) out << **i;
            else out << **std::get_if<double*>(&e.value);
            out << '\n';
        }
    }
    loadTable(dir / "reflectivity.properties", kDefaultReflectivity, gReflectivity);
    loadTable(dir / "occlusion.properties", kDefaultOcclusion, gOcclusion);
    loadTable(dir / "sound_rates.properties", kDefaultSoundRates, gSoundRates);
}

}
