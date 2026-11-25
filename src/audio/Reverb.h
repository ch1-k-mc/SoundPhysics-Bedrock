// SPDX-License-Identifier: GPL-3.0-only
// Based on henkelmax/sound-physics-remastered (master @3d10807) config/ReverbParams.java, ported to C++ and modified.
#pragma once

#include <array>
#include <vector>

namespace sound_physics {

struct ReverbParams {
    float decayTime;
    float density;
    float diffusion;
    float gain;
    float gainHF;
    float decayHFRatio;
    float reflectionsGain;
    float lateReverbGain;
    float lateReverbDelay;
};

ReverbParams reverbParamsForSlot(int slot);

class Reverb {
public:
    void init(int sampleRate, ReverbParams const& params);
    void process(float const* in, float* wetL, float* wetR, unsigned int frames);

private:
    static constexpr int kLines = 4;

    struct Delay {
        std::vector<float> buffer;
        size_t             pos = 0;
        float read(size_t delay) const {
            return buffer[(pos + buffer.size() - delay) % buffer.size()];
        }
        void write(float v) {
            buffer[pos] = v;
            pos         = (pos + 1) % buffer.size();
        }
    };

    ReverbParams                 mParams{};
    Delay                        mPre;
    size_t                       mPreDelay = 1;
    std::array<Delay, 4>         mEarly;
    std::array<size_t, 4>        mEarlyTap{};
    std::array<Delay, kLines>    mLines;
    std::array<size_t, kLines>   mLength{};
    std::array<float, kLines>    mFeedback{};
    std::array<float, kLines>    mDamp{};
    std::array<float, kLines>    mLow{};
    std::array<Delay, 2>         mAllpass;
    std::array<size_t, 2>        mAllpassLen{};
    float                        mAllpassGain = 0.0F;
    float                        mToneCoef    = 0.0F;
    std::array<float, 2>         mTone{};
};

}
