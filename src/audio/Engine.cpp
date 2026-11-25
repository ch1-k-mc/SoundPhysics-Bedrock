// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) SoundPhysics.java, mixin/*.java, ported to C++ and modified.
#include "audio/Engine.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <mutex>
#include <numbers>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

#include "ll/api/memory/Hook.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/MinecraftGame.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/sound/SoundEngine.h"
#include "mc/deps/audio/PlaySoundOptions.h"
#include "mc/world/level/BlockSource.h"

#include "audio/Reverb.h"
#include "config/Config.h"
#include "mod/SoundPhysics.h"
#include "physics/Evaluator.h"

namespace sound_physics::audio {

namespace {

struct DspDescription {
    unsigned int pluginSdkVersion;
    char         name[32];
    unsigned int version;
    int          numInputBuffers;
    int          numOutputBuffers;
    void*        create;
    void*        release;
    void*        reset;
    void*        read;
    void*        process;
    void*        setPosition;
    int          numParameters;
    void**       paramDesc;
    void*        setParameterFloat;
    void*        setParameterInt;
    void*        setParameterBool;
    void*        setParameterData;
    void*        getParameterFloat;
    void*        getParameterInt;
    void*        getParameterBool;
    void*        getParameterData;
    void*        shouldIProcess;
    void*        userData;
    void*        sysRegister;
    void*        sysDeregister;
    void*        sysMix;
};

struct DspState {
    void* instance;
    void* pluginData;
};

struct FmodVector {
    float x, y, z;
};

constexpr unsigned int kPluginSdkVersion = 110;
constexpr int          kDspHead          = -1;
constexpr int          kDspTail          = -2;
constexpr int          kCallbackEnd      = 0;
constexpr unsigned int kMode3D           = 0x00000010;
constexpr unsigned int kHeadRelative     = 0x00040000;
constexpr unsigned int kMaxFrames        = 8192;
constexpr int          kMaxChannels      = 8;
constexpr float        kAirAbsorbGainHF  = 0.99426F;
constexpr float        kReferenceHz      = 5000.0F;
constexpr auto         kDeferTimeout     = std::chrono::milliseconds(150);

using FnPlaySound  = int(__fastcall*)(void*, void*, void*, bool, void**);
using FnUpdate     = int(__fastcall*)(void*);
using FnSetPaused  = int(__fastcall*)(void*, bool);
using FnSet3D      = int(__fastcall*)(void*, FmodVector const*, FmodVector const*);
using FnCreateDsp  = int(__cdecl*)(void*, DspDescription const*, void**);
using FnAddDsp     = int(__cdecl*)(void*, int, void*);
using FnRemoveDsp  = int(__cdecl*)(void*, void*);
using FnReleaseDsp = int(__cdecl*)(void*);
using FnChannelCb  = int(__cdecl*)(void*, int, int, void*, void*);
using FnSetCb      = int(__cdecl*)(void*, FnChannelCb);
using FnGetMode    = int(__cdecl*)(void*, unsigned int*);
using FnGroupName  = int(__cdecl*)(void*, char*, int);
using FnGroupParent = int(__cdecl*)(void*, void**);
using FnGroupVolume = int(__cdecl*)(void*, float*);
using FnGetMinMax  = int(__cdecl*)(void*, float*, float*);
using FnSetMinMax  = int(__cdecl*)(void*, float, float);
using FnGetFormat  = int(__cdecl*)(void*, int*, int*, int*);
using FnListener   = int(__cdecl*)(void*, int, FmodVector*, FmodVector*, FmodVector*, FmodVector*);
using FnIsPlaying  = int(__cdecl*)(void*, int*);

FnPlaySound  gPlayOrigin = nullptr;
FnUpdate     gUpdateOrigin = nullptr;
FnSetPaused  gPausedOrigin = nullptr;
FnSet3D      gSet3DOrigin = nullptr;
void*        gPlayTarget = nullptr;
void*        gUpdateTarget = nullptr;
void*        gPausedTarget = nullptr;
void*        gSet3DTarget = nullptr;
FnCreateDsp  pCreateDsp = nullptr;
FnAddDsp     pAddDsp = nullptr, pGroupAddDsp = nullptr;
FnRemoveDsp  pRemoveDsp = nullptr;
FnReleaseDsp pReleaseDsp = nullptr;
FnSetCb      pSetCb = nullptr;
FnGetMode    pGetMode = nullptr;
FnGroupName  pGroupName = nullptr;
FnGroupParent pGroupParent = nullptr;
FnGroupVolume pGroupVolume = nullptr;
FnGetMinMax  pGetMinMax = nullptr;
FnSetMinMax  pSetMinMax = nullptr;
FnGetFormat  pGetFormat = nullptr;
FnListener   pListener = nullptr;
FnIsPlaying  pIsPlaying = nullptr;

struct Shelf {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    void  set(float gainHF, float sampleRate) {
        float const g     = (std::max)(gainHF, 0.001F);
        float const A     = std::sqrt(g);
        float const w     = 2.0F * std::numbers::pi_v<float> * kReferenceHz / sampleRate;
        float const c     = std::cos(w);
        float const alpha = std::sin(w) * 0.5F * std::numbers::sqrt2_v<float>;
        float const beta  = 2.0F * std::sqrt(A) * alpha;
        float const a0    = (A + 1) - (A - 1) * c + beta;
        b0 = A * ((A + 1) + (A - 1) * c + beta) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * c) / a0;
        b2 = A * ((A + 1) + (A - 1) * c - beta) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * c) / a0;
        a2 = ((A + 1) - (A - 1) * c - beta) / a0;
    }
};

struct ShelfState {
    float z1 = 0, z2 = 0;
    float run(Shelf const& s, float x) {
        float const y = s.b0 * x + z1;
        z1            = s.b1 * x - s.a1 * y + z2;
        z2            = s.b2 * x - s.a2 * y;
        return y;
    }
};

struct Voice {
    std::atomic<float>                directGain{1.0F};
    std::atomic<float>                directHF{1.0F};
    std::array<std::atomic<float>, 4> sendGain{};
    std::array<std::atomic<float>, 4> sendHF{};
    std::atomic<bool>                 dirty{true};
    Shelf                                  directShelf;
    std::array<Shelf, 4>                   sendShelf;
    std::array<ShelfState, kMaxChannels>   directState;
    std::array<ShelfState, 4>              sendState;
    float                                  curDirect = 1.0F;
    std::array<float, 4>                   curSend{};
    bool                                   first = true;
    Voice() {
        for (auto& v : sendHF) v.store(1.0F);
    }
};

struct Entry {
    Voice*                                voice = nullptr;
    void*                                 dsp   = nullptr;
    void*                                 group = nullptr;
    std::string                           name;
    std::string                           category;
    D3                                    pos;
    bool                                  hasPos = false;
    bool                                  relative = false;
    bool                                  waitingStart = true;
    bool                                  deferred = false;
    std::chrono::steady_clock::time_point deferredAt;
    float                                 baseMax = 0.0F;
    bool                                  rangeApplied = false;
};

std::mutex                              gMutex;
std::unordered_map<void*, Entry>        gEntries;
std::vector<std::pair<void*, void*>>    gPendingRelease;
std::atomic<void*>                      gSystem{nullptr};
std::atomic<bool>                       gActive{false};
std::atomic<DWORD>                      gMainThread{0};
thread_local std::string const*         tCurrentName = nullptr;
thread_local Voice*                     tCreatingVoice = nullptr;
std::unordered_map<std::string, int>    gRateCounts;
std::chrono::steady_clock::time_point   gRateReset;
std::chrono::steady_clock::time_point   gLastMoving;

struct RecentSound {
    std::string                           name;
    D3                                    pos;
    std::chrono::steady_clock::time_point at;
};
std::mutex              gRecentMutex;
std::deque<RecentSound> gRecent;

std::array<std::array<float, kMaxFrames>, 4> gBus{};
std::array<Reverb, 4>                        gReverbs;
std::array<float, kMaxFrames>                gWetL{}, gWetR{};
bool                                         gReverbReady = false;
void*                                        gReverbDsp   = nullptr;
float                                        gSampleRate  = 48000.0F;

std::atomic<long long> gStatVoices{0}, gStatEvaluated{0}, gStatDefault{0}, gStatDeferred{0}, gStatTimeout{0},
    gStatNamed{0}, gStatReverbReads{0}, gStatRedirected{0};
std::atomic<long long> gStatEvalMicros{0};
std::atomic<float>     gStatPeakSend{0.0F}, gStatPeakWet{0.0F};

auto& logger() { return SoundPhysics::getInstance().getSelf().getLogger(); }

int __cdecl voiceCreate(DspState* state) {
    state->pluginData = tCreatingVoice;
    return 0;
}

int __cdecl voiceRelease(DspState* state) {
    delete static_cast<Voice*>(state->pluginData);
    state->pluginData = nullptr;
    return 0;
}

int __cdecl voiceRead(DspState* state, float const* in, float* out, unsigned int length, int inChannels, int* outChannels) {
    int const outCount = outChannels && *outChannels > 0 ? *outChannels : inChannels;
    if (!in || !out || inChannels <= 0 || outCount <= 0) return 0;
    auto* v = static_cast<Voice*>(state->pluginData);
    int const common = (std::min)(inChannels, outCount);
    if (!v || !gActive.load(std::memory_order_relaxed)) {
        for (unsigned int f = 0; f < length; ++f) {
            for (int c = 0; c < outCount; ++c) out[f * outCount + c] = c < common ? in[f * inChannels + c] : 0.0F;
        }
        return 0;
    }
    if (v->dirty.exchange(false, std::memory_order_acq_rel)) {
        v->directShelf.set(v->directHF.load(std::memory_order_relaxed), gSampleRate);
        for (size_t b = 0; b < 4; ++b) v->sendShelf[b].set(v->sendHF[b].load(std::memory_order_relaxed), gSampleRate);
    }
    float const targetDirect = v->directGain.load(std::memory_order_relaxed);
    std::array<float, 4> targetSend{};
    for (size_t b = 0; b < 4; ++b) targetSend[b] = v->sendGain[b].load(std::memory_order_relaxed);
    if (v->first) {
        v->curDirect = targetDirect;
        v->curSend   = targetSend;
        v->first     = false;
    }
    float const rcp = length ? 1.0F / static_cast<float>(length) : 0.0F;
    float const stepDirect = (targetDirect - v->curDirect) * rcp;
    std::array<float, 4> stepSend{};
    for (size_t b = 0; b < 4; ++b) stepSend[b] = (targetSend[b] - v->curSend[b]) * rcp;
    int const   filtered = (std::min)(outCount, kMaxChannels);
    float const rcpIn    = 1.0F / static_cast<float>(inChannels);
    unsigned int const busFrames = (std::min)(length, kMaxFrames);
    for (unsigned int f = 0; f < length; ++f) {
        float mono = 0.0F;
        for (int c = 0; c < inChannels; ++c) mono += in[f * inChannels + c];
        mono *= rcpIn;
        v->curDirect += stepDirect;
        for (int c = 0; c < outCount; ++c) {
            float x = c < common ? in[f * inChannels + c] : 0.0F;
            if (c < filtered) x = v->directState[c].run(v->directShelf, x);
            out[f * outCount + c] = x * v->curDirect;
        }
        for (size_t b = 0; b < 4; ++b) {
            v->curSend[b] += stepSend[b];
            float const s = v->sendState[b].run(v->sendShelf[b], mono) * v->curSend[b];
            if (f < busFrames) gBus[b][f] += s;
        }
    }
    v->curDirect = targetDirect;
    v->curSend   = targetSend;
    return 0;
}

int __cdecl reverbRead(DspState*, float const* in, float* out, unsigned int length, int inChannels, int* outChannels) {
    int const outCount = outChannels && *outChannels > 0 ? *outChannels : inChannels;
    if (!in || !out || inChannels <= 0 || outCount <= 0) return 0;
    int const common = (std::min)(inChannels, outCount);
    for (unsigned int f = 0; f < length; ++f) {
        for (int c = 0; c < outCount; ++c) out[f * outCount + c] = c < common ? in[f * inChannels + c] : 0.0F;
    }
    if (!gReverbReady || !gActive.load(std::memory_order_relaxed)) return 0;
    unsigned int const frames = (std::min)(length, kMaxFrames);
    std::fill_n(gWetL.begin(), frames, 0.0F);
    std::fill_n(gWetR.begin(), frames, 0.0F);
    float peakSend = 0.0F;
    for (size_t b = 0; b < 4; ++b) {
        for (unsigned int f = 0; f < frames; ++f) peakSend = (std::max)(peakSend, std::fabs(gBus[b][f]));
        gReverbs[b].process(gBus[b].data(), gWetL.data(), gWetR.data(), frames);
        std::fill_n(gBus[b].begin(), frames, 0.0F);
    }
    float peakWet = 0.0F;
    for (unsigned int f = 0; f < frames; ++f) {
        peakWet = (std::max)(peakWet, (std::max)(std::fabs(gWetL[f]), std::fabs(gWetR[f])));
        if (outCount >= 2) {
            out[f * outCount] += gWetL[f];
            out[f * outCount + 1] += gWetR[f];
        } else {
            out[f] += (gWetL[f] + gWetR[f]) * 0.5F;
        }
    }
    gStatReverbReads.fetch_add(1, std::memory_order_relaxed);
    if (peakSend > gStatPeakSend.load(std::memory_order_relaxed)) gStatPeakSend.store(peakSend, std::memory_order_relaxed);
    if (peakWet > gStatPeakWet.load(std::memory_order_relaxed)) gStatPeakWet.store(peakWet, std::memory_order_relaxed);
    return 0;
}

DspDescription makeDescription(char const* name, void* read, void* create, void* release) {
    DspDescription d{};
    d.pluginSdkVersion = kPluginSdkVersion;
    strncpy_s(d.name, name, _TRUNCATE);
    d.version          = 1;
    d.numInputBuffers  = 1;
    d.numOutputBuffers = 1;
    d.read             = read;
    d.create           = create;
    d.release          = release;
    return d;
}

void ensureReverb(void* system, void* group) {
    if (gReverbDsp || !group) return;
    void* target = group;
    for (int depth = 0; depth < 8 && target; ++depth) {
        char name[64]{};
        pGroupName(target, name, sizeof(name));
        if (std::strcmp(name, "Sound Channel Group") == 0) break;
        void* parent = nullptr;
        if (pGroupParent(target, &parent) != 0 || !parent) break;
        target = parent;
    }
    int rate = 48000;
    pGetFormat(system, &rate, nullptr, nullptr);
    gSampleRate = static_cast<float>(rate);
    for (int i = 0; i < 4; ++i) gReverbs[static_cast<size_t>(i)].init(rate, reverbParamsForSlot(i));
    gReverbReady = true;
    static DspDescription const desc =
        makeDescription("SoundPhysicsReverb", reinterpret_cast<void*>(&reverbRead), nullptr, nullptr);
    void* dsp = nullptr;
    int const rcCreate = pCreateDsp(system, &desc, &dsp);
    int const rcAdd    = rcCreate == 0 ? pGroupAddDsp(target, kDspTail, dsp) : -1;
    char      name[64]{};
    pGroupName(target, name, sizeof(name));
    logger().info("Created reverb bus: group={} rate={} create={} add={}", name, rate, rcCreate, rcAdd);
    if (rcCreate == 0 && rcAdd == 0) gReverbDsp = dsp;
    else if (dsp) pReleaseDsp(dsp);
}

int __cdecl channelCallback(void* control, int controlType, int callbackType, void*, void*) {
    if (controlType != 0 || callbackType != kCallbackEnd) return 0;
    std::lock_guard lock(gMutex);
    auto            it = gEntries.find(control);
    if (it == gEntries.end()) return 0;
    gPendingRelease.emplace_back(control, it->second.dsp);
    gEntries.erase(it);
    return 0;
}

std::string categoryOf(char const* groupName) {
    static constexpr std::string_view kPrefix = "Sound Category Channel Group (";
    std::string_view                  s(groupName);
    if (!s.starts_with(kPrefix) || !s.ends_with(')')) return {};
    return std::string(s.substr(kPrefix.size(), s.size() - kPrefix.size() - 1));
}

int __fastcall playDetour(void* self, void* sound, void* group, bool paused, void** channel) {
    void*     local  = nullptr;
    void**    target = channel ? channel : &local;
    int const rc     = gPlayOrigin(self, sound, group, paused, target);
    gSystem.store(self, std::memory_order_relaxed);
    if (rc != 0 || !*target || !paused || !gActive.load(std::memory_order_relaxed) || !group) return rc;

    unsigned int mode = 0;
    if (pGetMode(sound, &mode) != 0 || !(mode & kMode3D)) return rc;
    char groupName[64]{};
    pGroupName(group, groupName, sizeof(groupName));
    std::string category = categoryOf(groupName);
    if (category.empty()) return rc;

    ensureReverb(self, group);

    static DspDescription const desc = makeDescription(
        "SoundPhysicsVoice",
        reinterpret_cast<void*>(&voiceRead),
        reinterpret_cast<void*>(&voiceCreate),
        reinterpret_cast<void*>(&voiceRelease)
    );
    auto* voice    = new Voice();
    tCreatingVoice = voice;
    void*     dsp      = nullptr;
    int const rcCreate = pCreateDsp(self, &desc, &dsp);
    tCreatingVoice     = nullptr;
    if (rcCreate != 0 || !dsp) {
        delete voice;
        return rc;
    }
    if (pAddDsp(*target, kDspHead, dsp) != 0) {
        pReleaseDsp(dsp);
        return rc;
    }
    Entry e;
    e.voice    = voice;
    e.dsp      = dsp;
    e.group    = group;
    e.category = std::move(category);
    e.relative = (mode & kHeadRelative) != 0;
    if (tCurrentName) {
        e.name = *tCurrentName;
        gStatNamed.fetch_add(1, std::memory_order_relaxed);
    }
    {
        std::lock_guard lock(gMutex);
        gEntries[*target] = std::move(e);
    }
    pSetCb(*target, &channelCallback);
    gStatVoices.fetch_add(1, std::memory_order_relaxed);
    return rc;
}

int __fastcall set3DDetour(void* self, FmodVector const* pos, FmodVector const* vel) {
    if (pos) {
        std::lock_guard lock(gMutex);
        if (auto it = gEntries.find(self); it != gEntries.end()) {
            it->second.pos    = {pos->x, pos->y, pos->z};
            it->second.hasPos = true;
        }
    }
    return gSet3DOrigin(self, pos, vel);
}

void applyDefault(Voice& v, float distance) {
    float const air = std::pow(kAirAbsorbGainHF, static_cast<float>(config().airAbsorption) * distance);
    v.directGain.store(1.0F);
    v.directHF.store(air);
    for (size_t b = 0; b < 4; ++b) {
        v.sendGain[b].store(0.0F);
        v.sendHF[b].store(1.0F);
    }
    v.dirty.store(true, std::memory_order_release);
}

void rememberSound(std::string const& name, float x, float y, float z) {
    auto const      now = std::chrono::steady_clock::now();
    std::lock_guard lock(gRecentMutex);
    gRecent.push_back({name, {x, y, z}, now});
    while (gRecent.size() > 128 || (!gRecent.empty() && now - gRecent.front().at > std::chrono::seconds(2))) {
        gRecent.pop_front();
    }
}

std::string matchRecentName(D3 pos) {
    std::lock_guard lock(gRecentMutex);
    auto            best     = gRecent.end();
    double          bestDist = 0.25;
    for (auto it = gRecent.begin(); it != gRecent.end(); ++it) {
        double const dx = it->pos.x - pos.x, dy = it->pos.y - pos.y, dz = it->pos.z - pos.z;
        double const d  = dx * dx + dy * dy + dz * dz;
        if (d < bestDist) {
            bestDist = d;
            best     = it;
        }
    }
    if (best == gRecent.end()) return {};
    std::string name = std::move(best->name);
    gRecent.erase(best);
    return name;
}

bool rateLimited(std::string const& name) {
    auto const now = std::chrono::steady_clock::now();
    if (now - gRateReset >= std::chrono::milliseconds(50)) {
        gRateCounts.clear();
        gRateReset = now;
    }
    auto const& table = soundRateTable();
    auto const  it    = table.find(name);
    if (it == table.end() || it->second < 0) return false;
    return ++gRateCounts[name] > it->second;
}

void evaluateChannel(void* channel, bool initial) {
    Entry copy;
    {
        std::lock_guard lock(gMutex);
        auto            it = gEntries.find(channel);
        if (it == gEntries.end()) return;
        copy = it->second;
    }
    void* system = gSystem.load(std::memory_order_relaxed);
    if (!system || !copy.voice) return;
    auto const& cfg = config();

    if (copy.name.empty() && copy.hasPos) {
        copy.name = matchRecentName(copy.pos);
        if (!copy.name.empty()) {
            gStatNamed.fetch_add(1, std::memory_order_relaxed);
            std::lock_guard lock(gMutex);
            if (auto it = gEntries.find(channel); it != gEntries.end()) it->second.name = copy.name;
        }
    }

    float minD = 1.0F, maxD = 16.0F;
    pGetMinMax(channel, &minD, &maxD);
    if (initial) {
        copy.baseMax = maxD;
        maxD         = static_cast<float>(maxD * cfg.soundDistanceAllowance / cfg.attenuationFactor);
        pSetMinMax(channel, minD, maxD);
    }

    FmodVector lp{}, lv{}, lf{}, lu{};
    pListener(system, 0, &lp, &lv, &lf, &lu);
    D3 const     listener{lp.x, lp.y, lp.z};
    double const dx = copy.pos.x - listener.x, dy = copy.pos.y - listener.y, dz = copy.pos.z - listener.z;
    auto const   distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));

    auto ci = ll::service::getClientInstance();
    LocalPlayer* player = ci ? ci->getLocalPlayer() : nullptr;
    bool const skip = !player || !copy.hasPos || copy.relative || distance > cfg.maxSoundProcessingDistance
                   || (!cfg.updateMovingSounds && copy.category == "record") || rateLimited(copy.name)
                   || (!cfg.evaluateAmbientSounds
                       && (copy.name.starts_with("ambient.") || copy.category == "ambient" || copy.category == "weather"));
    if (skip) {
        applyDefault(*copy.voice, distance);
        gStatDefault.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    auto const started = std::chrono::steady_clock::now();
    EvalInput  input{copy.pos, listener, copy.name, copy.category, maxD};
    EvalResult r = evaluateEnvironment(player->getDimensionBlockSource(), input);
    auto const micros =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
    gStatEvalMicros.fetch_add(micros, std::memory_order_relaxed);
    gStatEvaluated.fetch_add(1, std::memory_order_relaxed);

    if (r.hasNewPos) {
        FmodVector const p{static_cast<float>(r.newPos.x), static_cast<float>(r.newPos.y), static_cast<float>(r.newPos.z)};
        gSet3DOrigin(channel, &p, nullptr);
        gStatRedirected.fetch_add(1, std::memory_order_relaxed);
    }

    float categoryVolume = 1.0F;
    if (copy.group) pGroupVolume(copy.group, &categoryVolume);
    float const air = std::pow(kAirAbsorbGainHF, static_cast<float>(cfg.airAbsorption) * distance);
    copy.voice->directGain.store(r.directGain);
    copy.voice->directHF.store(r.directCutoff * air);
    for (size_t b = 0; b < 4; ++b) {
        copy.voice->sendGain[b].store(r.sendGain[b] * categoryVolume);
        copy.voice->sendHF[b].store(r.sendCutoff[b] * air);
    }
    copy.voice->dirty.store(true, std::memory_order_release);

    if (cfg.environmentLogging) {
        logger().info(
            "eval name={} cat={} dist={:.1f} occ={:.2f} shared={} dGain={:.3f} dHF={:.3f} send={:.3f},{:.3f},{:.3f},{:.3f} moved={} {}us",
            copy.name,
            copy.category,
            distance,
            r.occlusion,
            r.sharedAirspace,
            r.directGain,
            r.directCutoff,
            r.sendGain[0],
            r.sendGain[1],
            r.sendGain[2],
            r.sendGain[3],
            r.hasNewPos,
            micros
        );
    }
}

int __fastcall setPausedDetour(void* self, bool paused) {
    if (paused || !gActive.load(std::memory_order_relaxed)) return gPausedOrigin(self, paused);
    bool found = false;
    {
        std::lock_guard lock(gMutex);
        if (auto it = gEntries.find(self); it != gEntries.end() && it->second.waitingStart) {
            it->second.waitingStart = false;
            found                   = true;
            if (GetCurrentThreadId() != gMainThread.load(std::memory_order_relaxed)) {
                it->second.deferred   = true;
                it->second.deferredAt = std::chrono::steady_clock::now();
                gStatDeferred.fetch_add(1, std::memory_order_relaxed);
                return 0;
            }
        }
    }
    if (found) evaluateChannel(self, true);
    return gPausedOrigin(self, paused);
}

int __fastcall updateDetour(void* self) {
    int const rc = gUpdateOrigin(self);
    gSystem.store(self, std::memory_order_relaxed);
    std::vector<std::pair<void*, void*>> pending;
    std::vector<void*>                   expired;
    {
        std::lock_guard lock(gMutex);
        pending.swap(gPendingRelease);
        auto const now = std::chrono::steady_clock::now();
        for (auto& [channel, e] : gEntries) {
            if (e.deferred && now - e.deferredAt > kDeferTimeout) {
                e.deferred = false;
                expired.push_back(channel);
            }
        }
    }
    for (auto const& [channel, dsp] : pending) {
        pRemoveDsp(channel, dsp);
        pReleaseDsp(dsp);
    }
    for (void* channel : expired) {
        gStatTimeout.fetch_add(1, std::memory_order_relaxed);
        gPausedOrigin(channel, false);
    }
    return rc;
}

using FnEnginePlay        = unsigned long long(__fastcall*)(void*, std::string const*, void const*, float, float, void*);
using FnEnginePlayOptions = unsigned long long(__fastcall*)(void*, PlaySoundOptions*);
FnEnginePlay        gEnginePlayOrigin = nullptr;
FnEnginePlayOptions gEngineOptOrigin  = nullptr;
void*               gEnginePlayTarget = nullptr;
void*               gEngineOptTarget  = nullptr;
bool                gEngineHooked     = false;

unsigned long long __fastcall enginePlayDetour(void* self, std::string const* name, void const* pos, float volume, float pitch, void* handle) {
    if (name && pos) {
        auto const* p = static_cast<Vec3 const*>(pos);
        rememberSound(*name, p->x, p->y, p->z);
    }
    auto const* previous = tCurrentName;
    tCurrentName         = name;
    auto const r         = gEnginePlayOrigin(self, name, pos, volume, pitch, handle);
    tCurrentName         = previous;
    return r;
}

unsigned long long __fastcall enginePlayOptionsDetour(void* self, PlaySoundOptions* options) {
    Vec3 const& optionPos = options->mPosition.get();
    rememberSound(options->mName.get(), optionPos.x, optionPos.y, optionPos.z);
    auto const* previous = tCurrentName;
    tCurrentName         = &options->mName.get();
    auto const r         = gEngineOptOrigin(self, options);
    tCurrentName         = previous;
    return r;
}

std::optional<unsigned> thunkIndex(void const* thunk) {
    auto const* b = static_cast<std::uint8_t const*>(thunk);
    if (!b) return std::nullopt;
    for (int k = 0; k < 4 && b[0] == 0xE9; ++k) b = b + 5 + *reinterpret_cast<std::int32_t const*>(b + 1);
    for (int i = 0; i + 3 < 40; ++i) {
        if (!(b[i] == 0x48 && b[i + 1] == 0x8B && b[i + 2] == 0x01)) continue;
        auto const* c = b + i + 3;
        if (c[0] == 0x48 && c[1] == 0x8B && c[2] == 0x40) return static_cast<unsigned>(c[3]) / 8;
        if (c[0] == 0x48 && c[1] == 0x8B && c[2] == 0x80) return *reinterpret_cast<std::uint32_t const*>(c + 3) / 8;
        if (c[0] == 0xFF && c[1] == 0x60) return static_cast<unsigned>(c[2]) / 8;
        if (c[0] == 0xFF && c[1] == 0xA0) return *reinterpret_cast<std::uint32_t const*>(c + 2) / 8;
        return std::nullopt;
    }
    return std::nullopt;
}

template <class M>
void* memberAddress(M member) {
    void* out = nullptr;
    std::memcpy(&out, &member, sizeof(out));
    return out;
}

void hookEngine() {
    if (gEngineHooked) return;
    auto ci = ll::service::getClientInstance();
    if (!ci || !ci->getLocalPlayer()) return;
    gEngineHooked = true;
    SoundEngine* engine = ci->getSoundEngine().get();
    if (!engine) {
        gEngineHooked = false;
        return;
    }
    using PlayA = uint64 (SoundEngine::*)(std::string const&, Vec3 const&, float, float, std::optional<ServerSoundHandle>);
    using PlayB = uint64 (SoundEngine::*)(PlaySoundOptions);
    auto const indexA = thunkIndex(memberAddress(static_cast<PlayA>(&SoundEngine::play)));
    auto const indexB = thunkIndex(memberAddress(static_cast<PlayB>(&SoundEngine::play)));
    void** table = *reinterpret_cast<void***>(engine);
    int rcA = -1, rcB = -1;
    if (indexA) {
        gEnginePlayTarget = table[*indexA];
        rcA = ll::memory::hook(gEnginePlayTarget, reinterpret_cast<void*>(&enginePlayDetour), reinterpret_cast<void**>(&gEnginePlayOrigin), ll::memory::HookPriority::Normal, false);
        if (rcA != 0) gEnginePlayTarget = nullptr;
    }
    if (indexB) {
        gEngineOptTarget = table[*indexB];
        rcB = ll::memory::hook(gEngineOptTarget, reinterpret_cast<void*>(&enginePlayOptionsDetour), reinterpret_cast<void**>(&gEngineOptOrigin), ll::memory::HookPriority::Normal, false);
        if (rcB != 0) gEngineOptTarget = nullptr;
    }
    logger().info("Hooked SoundEngine: play={} (#{}) options={} (#{})", rcA, indexA.value_or(0), rcB, indexB.value_or(0));
}

void pumpMain() {
    hookEngine();
    std::vector<void*> ready;
    std::vector<void*> moving;
    auto const&        cfg = config();
    auto const         now = std::chrono::steady_clock::now();
    bool const         updateMoving =
        cfg.updateMovingSounds && now - gLastMoving >= std::chrono::milliseconds(50LL * cfg.soundUpdateInterval);
    {
        std::lock_guard lock(gMutex);
        for (auto& [channel, e] : gEntries) {
            if (e.deferred) {
                e.deferred = false;
                ready.push_back(channel);
            } else if (updateMoving && !e.waitingStart) {
                moving.push_back(channel);
            }
        }
    }
    for (void* channel : ready) {
        evaluateChannel(channel, true);
        gPausedOrigin(channel, false);
    }
    if (updateMoving) {
        gLastMoving = now;
        for (void* channel : moving) {
            int playing = 0;
            if (pIsPlaying(channel, &playing) == 0 && playing) evaluateChannel(channel, false);
        }
    }
}

LL_TYPE_INSTANCE_HOOK(FramePump, ll::memory::HookPriority::Normal, MinecraftGame, &MinecraftGame::$update, void) {
    origin();
    gMainThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
    if (gActive.load(std::memory_order_relaxed)) pumpMain();
}

template <class T>
bool proc(HMODULE m, char const* name, T& out) {
    out = reinterpret_cast<T>(GetProcAddress(m, name));
    return out != nullptr;
}

bool hookExport(HMODULE m, char const* symbol, void*& target, void* detour, void** origin) {
    target = reinterpret_cast<void*>(GetProcAddress(m, symbol));
    if (!target) return false;
    if (ll::memory::hook(target, detour, origin, ll::memory::HookPriority::Normal) != 0) {
        target = nullptr;
        return false;
    }
    return true;
}

}

bool install() {
    HMODULE fmod = GetModuleHandleW(L"fmod.dll");
    if (!fmod) return false;

    bool ok = proc(fmod, "FMOD5_System_CreateDSP", pCreateDsp);
    ok = proc(fmod, "FMOD5_Channel_AddDSP", pAddDsp) && ok;
    ok = proc(fmod, "FMOD5_ChannelGroup_AddDSP", pGroupAddDsp) && ok;
    ok = proc(fmod, "FMOD5_Channel_RemoveDSP", pRemoveDsp) && ok;
    ok = proc(fmod, "FMOD5_DSP_Release", pReleaseDsp) && ok;
    ok = proc(fmod, "FMOD5_Channel_SetCallback", pSetCb) && ok;
    ok = proc(fmod, "FMOD5_Sound_GetMode", pGetMode) && ok;
    ok = proc(fmod, "FMOD5_ChannelGroup_GetName", pGroupName) && ok;
    ok = proc(fmod, "FMOD5_ChannelGroup_GetParentGroup", pGroupParent) && ok;
    ok = proc(fmod, "FMOD5_ChannelGroup_GetVolume", pGroupVolume) && ok;
    ok = proc(fmod, "FMOD5_Channel_Get3DMinMaxDistance", pGetMinMax) && ok;
    ok = proc(fmod, "FMOD5_Channel_Set3DMinMaxDistance", pSetMinMax) && ok;
    ok = proc(fmod, "FMOD5_System_GetSoftwareFormat", pGetFormat) && ok;
    ok = proc(fmod, "FMOD5_System_Get3DListenerAttributes", pListener) && ok;
    ok = proc(fmod, "FMOD5_Channel_IsPlaying", pIsPlaying) && ok;
    if (!ok) {
        logger().error("fmod.dll is missing required functions");
        return false;
    }
    ok = hookExport(fmod, "?update@System@FMOD@@QEAA?AW4FMOD_RESULT@@XZ", gUpdateTarget, reinterpret_cast<void*>(&updateDetour), reinterpret_cast<void**>(&gUpdateOrigin));
    ok = hookExport(fmod, "?set3DAttributes@ChannelControl@FMOD@@QEAA?AW4FMOD_RESULT@@PEBUFMOD_VECTOR@@0@Z", gSet3DTarget, reinterpret_cast<void*>(&set3DDetour), reinterpret_cast<void**>(&gSet3DOrigin)) && ok;
    ok = hookExport(fmod, "?setPaused@ChannelControl@FMOD@@QEAA?AW4FMOD_RESULT@@_N@Z", gPausedTarget, reinterpret_cast<void*>(&setPausedDetour), reinterpret_cast<void**>(&gPausedOrigin)) && ok;
    ok = hookExport(fmod, "?playSound@System@FMOD@@QEAA?AW4FMOD_RESULT@@PEAVSound@2@PEAVChannelGroup@2@_NPEAPEAVChannel@2@@Z", gPlayTarget, reinterpret_cast<void*>(&playDetour), reinterpret_cast<void**>(&gPlayOrigin)) && ok;
    if (!ok) return false;
    FramePump::hook();
    gActive.store(true);
    return true;
}

void uninstall() { gActive.store(false); }

void logSummary() {
    long long const evaluated = gStatEvaluated.load();
    logger().info(
        "summary voices={} named={} evaluated={} default={} deferred={} timeout={} redirected={} avgEvalUs={} reverbReads={} peakSend={:.4f} peakWet={:.4f}",
        gStatVoices.load(),
        gStatNamed.load(),
        evaluated,
        gStatDefault.load(),
        gStatDeferred.load(),
        gStatTimeout.load(),
        gStatRedirected.load(),
        evaluated ? gStatEvalMicros.load() / evaluated : 0,
        gStatReverbReads.load(),
        gStatPeakSend.load(),
        gStatPeakWet.load()
    );
}

}
