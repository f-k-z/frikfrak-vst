#pragma once
// The Glitch effect chain, pure C++. Fixed serial order:
// Stutter -> Reverse -> Tape Stop -> Downer -> Pitch -> Crush -> Filter -> Phaser -> Gate -> Echo -> Reverb.
// Every effect returns its input bit-exact while disengaged, so with no pad active and no tail
// ringing the whole chain is an exact pass-through.
#include "dsp/BeatClock.h"
#include <array>

namespace dsp {

struct GlitchSettings {
    float crushBits = 6, crushRateKHz = 8;
    float pitchSemis = -12;
    float reverbSize = 0.6f;
    double stutterQn = 0.25;
    double gateQn = 0.25; float gateDepth = 1;
    float phaserHz = 0.5f;
    float filterFreq = -0.6f;
    double tapeQn = 1;
    double reverseQn = 1;
    double echoQn = 0.75; float echoFeedback = 0.55f;
    double downerQn = 1; float downerDepth = 0.6f;
};

class GlitchChain {
public:
    static constexpr int kFx = 11;
    using Active = std::array<bool, kFx>;

    void prepare(double) {}
    // One stereo sample; ppq = musical position of this sample.
    void process(float&, float&, const Active&, const GlitchSettings&, const BeatClock&, double) {}
    bool idle() const { return true; }
};

}  // namespace dsp
