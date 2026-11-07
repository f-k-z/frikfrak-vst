#pragma once
// "Throw" effects, pure C++: the pad opens the send, the return is always mixed in, so the tail
// keeps ringing after release. Idle (bit-exact pass-through) is decided on internal state, never
// on the output level, so a short tap is not bypassed before its first repeat comes back.
#include "dsp/BeatClock.h"
#include "dsp/Effects.h"
#include "dsp/Engage.h"
#include "dsp/Space.h"
#include <cmath>

namespace dsp {

constexpr float kSilent = 3.16e-5f;  // -90 dBFS

// Tempo-synced stereo echo, darker on each repeat (one-pole low-pass ~3 kHz in the feedback).
class Echo {
public:
    void prepare(double sampleRate) {
        sr = sampleRate;
        line.prepare(sr, 2.5);  // 1/2 note at the 60 BPM floor = 2 s
        eng.prepare(sr);
        delayS.prepare(sr, 1.0f, 0.05f);
        lpA = 1.0f - std::exp(-kTwoPi * 3000.0f / (float) sr);
        lp[0] = lp[1] = 0.0f;
        quiet = 0; idleFlag = true;
    }
    bool idle() const { return idleFlag; }

    void process(float& l, float& r, bool on, double qn, float feedback, const BeatClock& clk) {
        const float target = (float) std::min(clk.lengthSamples(qn), (double) line.capacity());
        if (on && idleFlag) { idleFlag = false; delayS.value = target; quiet = 0; }
        if (idleFlag) return;
        const float send = eng.next(on);
        const double d = delayS.next(target);
        float* ch[2] = { &l, &r };
        float w[2];
        bool silent = true;
        for (int c = 0; c < 2; ++c) {
            const float wet = line.read(c, d);
            lp[c] += lpA * (wet - lp[c]);
            w[c] = *ch[c] * send + feedback * lp[c];
            silent = silent && std::fabs(w[c]) < kSilent;
            *ch[c] += wet;
        }
        line.push(w[0], w[1]);
        quiet = silent ? quiet + 1 : 0;
        if (send <= 0.0f && quiet > (int64_t) d + 512) idleFlag = true;  // everything still in the line is below -90 dBFS
    }

private:
    double sr = 48000.0;
    History line;
    Engage eng;
    Smoother delayS;
    float lpA = 0.3f, lp[2] {};
    int64_t quiet = 0;
    bool idleFlag = true;
};

// The shared FDN hall reverb, fed through a gated send; its wet return is added to the dry.
class ReverbThrow {
public:
    static constexpr float kWet = 0.6f;
    void prepare(double sampleRate) {
        sr = sampleRate;
        rv.prepare(sr, 1.0f, 0.6f);
        eng.prepare(sr);
        guard = (int64_t) (0.15 * sr);  // > pre-delay + longest line: the send has fully left the input stage
        closedFor = quiet = 0; idleFlag = true;
    }
    bool idle() const { return idleFlag; }

    void process(float& l, float& r, bool on, float size) {
        if (on) idleFlag = false;
        if (idleFlag) return;
        const float send = eng.next(on);
        float wl = l * send, wr = r * send;
        rv.process(wl, wr, 1.0f, size);  // mix 1: pure wet
        l += kWet * wl;
        r += kWet * wr;
        closedFor = send > 0.0f ? 0 : closedFor + 1;
        quiet = (std::fabs(wl) < kSilent && std::fabs(wr) < kSilent) ? quiet + 1 : 0;
        if (closedFor > guard && quiet > guard) idleFlag = true;
    }

private:
    double sr = 48000.0;
    Reverb rv;
    Engage eng;
    int64_t guard = 7200, closedFor = 0, quiet = 0;
    bool idleFlag = true;
};

}  // namespace dsp
