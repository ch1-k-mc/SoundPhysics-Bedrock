// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) config/ReverbParams.java, ported to C++ and modified.
#include "audio/Reverb.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "config/Config.h"

namespace sound_physics {

ReverbParams reverbParamsForSlot(int slot) {
    static constexpr std::array<ReverbParams, 4> kBase{{
        {0.15F, 0.0F, 1.0F, 0.2F, 0.99F, 0.6F, 2.5F, 1.26F, 0.011F},
        {0.55F, 0.0F, 1.0F, 0.3F, 0.99F, 0.7F, 0.2F, 1.26F, 0.011F},
        {1.68F, 0.1F, 1.0F, 0.5F, 0.99F, 0.7F, 0.0F, 1.26F, 0.021F},
        {4.142F, 0.5F, 1.0F, 0.4F, 0.89F, 0.7F, 0.0F, 1.26F, 0.021F},
    }};
    auto const&  cfg = config();
    ReverbParams p   = kBase[static_cast<size_t>(slot)];
    p.gain *= 0.7F * static_cast<float>(cfg.reverbGain) * 0.85F;
    p.decayHFRatio *= static_cast<float>(cfg.reverbBrightness);
    return p;
}

void Reverb::init(int sampleRate, ReverbParams const& params) {
    mParams        = params;
    auto const sr  = static_cast<float>(sampleRate);
    auto samples   = [sr](float seconds) { return (std::max)(size_t{1}, static_cast<size_t>(seconds * sr)); };
    auto allocate  = [](Delay& d, size_t length) {
        d.buffer.assign(length + 1, 0.0F);
        d.pos = 0;
    };

    mPreDelay = samples(params.lateReverbDelay);
    allocate(mPre, mPreDelay);

    static constexpr std::array<float, 4> kEarlyMs{7.1F, 11.3F, 16.7F, 23.9F};
    for (size_t i = 0; i < 4; ++i) {
        mEarlyTap[i] = samples(kEarlyMs[i] * 0.001F);
        allocate(mEarly[i], mEarlyTap[i]);
    }

    float const size = 0.6F + 1.4F * std::cbrt(params.density);
    static constexpr std::array<float, kLines> kLineMs{29.7F, 37.1F, 41.1F, 43.7F};
    float const hfDecay = (std::max)(params.decayTime * params.decayHFRatio, 0.001F);
    for (size_t i = 0; i < kLines; ++i) {
        float const seconds = kLineMs[i] * 0.001F * size;
        mLength[i]          = samples(seconds);
        allocate(mLines[i], mLength[i]);
        float const g   = std::pow(10.0F, -3.0F * seconds / params.decayTime);
        float const gHF = std::pow(10.0F, -3.0F * seconds / hfDecay);
        mFeedback[i]    = g;
        mDamp[i]        = std::clamp(1.0F - gHF / g, 0.0F, 0.95F);
        mLow[i]         = 0.0F;
    }

    mAllpassLen  = {samples(0.0051F), samples(0.0077F)};
    mAllpassGain = 0.6F * params.diffusion;
    allocate(mAllpass[0], mAllpassLen[0]);
    allocate(mAllpass[1], mAllpassLen[1]);

    float const w   = 2.0F * std::numbers::pi_v<float> * 5000.0F / sr;
    float const hf  = std::clamp(params.gainHF, 0.01F, 1.0F);
    float const cosW = std::cos(w);
    float const a   = (1.0F - hf * hf * cosW - hf * std::sqrt(2.0F * (1.0F - cosW) + hf * hf * (cosW * cosW - 1.0F)))
                  / (1.0F - hf * hf);
    mToneCoef = hf >= 0.999F ? 0.0F : std::clamp(a, 0.0F, 0.99F);
    mTone     = {0.0F, 0.0F};
}

void Reverb::process(float const* in, float* wetL, float* wetR, unsigned int frames) {
    float const outGain = mParams.gain;
    float const early   = mParams.reflectionsGain * 0.25F;
    float const late    = mParams.lateReverbGain * 0.5F;
    for (unsigned int f = 0; f < frames; ++f) {
        float const x = in[f];

        float earlyL = 0.0F, earlyR = 0.0F;
        for (size_t i = 0; i < 4; ++i) {
            float const v = mEarly[i].read(mEarlyTap[i]);
            mEarly[i].write(x);
            (i % 2 == 0 ? earlyL : earlyR) += v;
        }

        float d = mPre.read(mPreDelay);
        mPre.write(x);
        for (size_t i = 0; i < 2; ++i) {
            float const delayed = mAllpass[i].read(mAllpassLen[i]);
            float const input   = d + delayed * mAllpassGain;
            mAllpass[i].write(input);
            d = delayed - input * mAllpassGain;
        }

        std::array<float, kLines> v{};
        float                     sum = 0.0F;
        for (size_t i = 0; i < kLines; ++i) {
            float const out = mLines[i].read(mLength[i]);
            mLow[i]         = out * (1.0F - mDamp[i]) + mLow[i] * mDamp[i];
            v[i]            = mLow[i] * mFeedback[i];
            sum += v[i];
        }
        sum *= 0.5F;
        for (size_t i = 0; i < kLines; ++i) mLines[i].write(d + v[i] - sum);

        float lateL = (v[0] - v[1] + v[2] - v[3]) * late;
        float lateR = (v[0] + v[1] - v[2] - v[3]) * late;

        float l = (earlyL * early + lateL) * outGain;
        float r = (earlyR * early + lateR) * outGain;
        mTone[0] = l * (1.0F - mToneCoef) + mTone[0] * mToneCoef;
        mTone[1] = r * (1.0F - mToneCoef) + mTone[1] * mToneCoef;
        wetL[f] += mTone[0];
        wetR[f] += mTone[1];
    }
}

}
