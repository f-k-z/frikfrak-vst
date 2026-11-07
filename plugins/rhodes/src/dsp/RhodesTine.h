#pragma once
#include "dsp/Effects.h"
#include <algorithm>
#include <cmath>

namespace dsp {

// One Rhodes voice: 3 damped cantilever modes -> asymmetric pickup -> DC blocker -> damper env.
// Bark comes from tine amplitude (velocity) driving a fixed pickup curve, as on the real instrument.
class RhodesTine {
public:
    void prepare(double sampleRate) { sr = sampleRate; dc.prepare(sr); reset(); }

    void reset() { state = Idle; residual = lastOut = 0.0f; for (auto& p : phase) p = 0.0f; }

    // bark 0..1 (pickup drive), decayScale ~0.25..2
    void noteOn(int midiNote, float velocity, float bark = 0.5f, float decayScale = 1.0f) {
        residual = lastOut;  // declick: fade out whatever this voice was playing (steal / retrigger)
        dc.x1 = dc.y1 = 0.0f;  // continuity is carried by residual, not by the old filter state
        const float f0 = 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
        const float vAmp = std::pow(std::clamp(velocity, 0.0f, 1.0f), 1.7f);
        const float t60Fund = decayScale * std::clamp(6.0f * std::pow(2.0f, -(midiNote - 60) / 24.0f), 0.8f, 14.0f);
        const float t60[kModes] = { t60Fund, 0.15f, 0.04f };
        for (int m = 0; m < kModes; ++m) {
            const float f = f0 * kRatios[m];
            amp[m] = f < 0.45f * (float) sr ? vAmp * kAmps[m] : 0.0f;  // skip modes that would alias
            inc[m] = kTwoPi * f / (float) sr;
            mul[m] = decayMul(t60[m]);
            phase[m] = 0.0f;
        }
        const float treble = std::max(0.3f, 1.0f - std::max(0, midiNote - 84) / 40.0f);  // less drive up top
        g = (0.5f + 4.5f * bark) * treble;
        norm = 0.4f / (g * (1.0f - std::tanh(kBias) * std::tanh(kBias)));  // unity small-signal gain x0.4 headroom
        env = 1.0f;
        envMul = 1.0f;
        state = Playing;
    }

    void noteOff() { if (state == Playing) { envMul = decayMul(0.12f); state = Releasing; } }

    bool isActive() const { return state != Idle; }

    float process() {
        if (state == Idle) return 0.0f;
        float x = 0.0f;
        for (int m = 0; m < kModes; ++m) {
            x += amp[m] * std::sin(phase[m]);
            amp[m] *= mul[m];
            phase[m] += inc[m];
            if (phase[m] >= kTwoPi) phase[m] -= kTwoPi;
        }
        const float picked = norm * (std::tanh(g * x + kBias) - std::tanh(kBias));
        env *= envMul;
        float out = dc.process(picked) * env + residual;
        residual *= kResidualMul;
        if (env < 1e-4f || amp[0] < 1e-5f) state = Idle;  // released, or fully decayed
        lastOut = out;
        return out;
    }

private:
    static constexpr int kModes = 3;
    static constexpr float kRatios[kModes] = { 1.0f, 6.27f, 17.55f };  // clamped-free cantilever
    static constexpr float kAmps[kModes] = { 1.0f, 0.25f, 0.1f };
    static constexpr float kBias = 0.6f;          // pickup voicing (tine offset from pole)
    static constexpr float kResidualMul = 0.985f; // ~3 ms at 48 kHz

    float decayMul(float t60) const { return std::pow(10.0f, -3.0f / (t60 * (float) sr)); }

    enum State { Idle, Playing, Releasing } state = Idle;
    double sr = 44100.0;
    float phase[kModes] {}, inc[kModes] {}, amp[kModes] {}, mul[kModes] {};
    float g = 1.0f, norm = 1.0f, env = 0.0f, envMul = 1.0f, residual = 0.0f, lastOut = 0.0f;
    DcBlocker dc;
};

}  // namespace dsp
