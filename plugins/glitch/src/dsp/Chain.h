#pragma once
// The Glitch effect chain, pure C++. Fixed serial order:
// Stutter -> Reverse -> Tape Stop -> Downer -> Pitch -> Crush -> Filter -> Phaser -> Gate -> Echo -> Reverb.
// Every effect returns its input bit-exact while disengaged, so with no pad active and no tail
// ringing the whole chain is an exact pass-through. While anything is engaged, a soft ceiling
// keeps the Main output under 0 dBFS (echo and reverb add to the dry signal).
#include "dsp/BeatClock.h"
#include "dsp/ColorFx.h"
#include "dsp/ThrowFx.h"
#include "dsp/TimeFx.h"
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

// Identity below 0.9, smooth knee up to 1.0 (continuous slope at the knee).
inline float softCeiling(float x) {
    constexpr float a = 0.9f;
    const float m = std::fabs(x);
    if (m <= a) return x;
    return std::copysign(a + (1.0f - a) * std::tanh((m - a) / (1.0f - a)), x);
}

class GlitchChain {
public:
    static constexpr int kFx = 11;
    using Active = std::array<bool, kFx>;
    enum { Crush, Pitch, Reverb, Stutter, Gate, Phaser, Filter, TapeStop, Reverse, Echo, Downer };  // = pad order

    void prepare(double sr) {
        stutter.prepare(sr); reverse.prepare(sr); tape.prepare(sr); downer.prepare(sr);
        pitch.prepare(sr); crush.prepare(sr); filter.prepare(sr); phaser.prepare(sr); gate.prepare(sr);
        echo.prepare(sr); reverb.prepare(sr);
    }

    // One stereo sample; ppq = musical position of this sample.
    void process(float& l, float& r, const Active& a, const GlitchSettings& s, const BeatClock& clk, double ppq) {
        stutter.process(l, r, a[Stutter], s.stutterQn, clk, ppq);
        reverse.process(l, r, a[Reverse], s.reverseQn, clk);
        tape.process(l, r, a[TapeStop], s.tapeQn, clk);
        downer.process(l, r, a[Downer], s.downerQn, s.downerDepth, clk, ppq);
        pitch.process(l, r, a[Pitch], s.pitchSemis);
        crush.process(l, r, a[Crush], s.crushBits, s.crushRateKHz);
        filter.process(l, r, a[Filter], s.filterFreq);
        phaser.process(l, r, a[Phaser], s.phaserHz);
        gate.process(l, r, a[Gate], s.gateQn, s.gateDepth, ppq);
        echo.process(l, r, a[Echo], s.echoQn, s.echoFeedback, clk);
        reverb.process(l, r, a[Reverb], s.reverbSize);
        if (!idle()) { l = softCeiling(l); r = softCeiling(r); }
    }

    bool idle() const {
        return stutter.idle() && reverse.idle() && tape.idle() && downer.idle() && pitch.idle() && crush.idle()
            && filter.idle() && phaser.idle() && gate.idle() && echo.idle() && reverb.idle();
    }

private:
    dsp::Stutter stutter;
    dsp::Reverse reverse;
    dsp::TapeStop tape;
    dsp::Downer downer;
    dsp::PitchShift pitch;
    dsp::Crush crush;
    dsp::DjFilter filter;
    dsp::GlitchPhaser phaser;
    dsp::Gate gate;
    dsp::Echo echo;
    dsp::ReverbThrow reverb;
};

}  // namespace dsp
