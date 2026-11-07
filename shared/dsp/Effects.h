#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dsp {

constexpr float kTwoPi = 6.28318530718f;

// One-pole parameter smoother (~20 ms) to avoid zipper noise.
struct Smoother {
    float value = 0.0f, coeff = 0.0f;
    void prepare(double sr, float initial, float timeSec = 0.02f) {
        coeff = 1.0f - std::exp(-1.0f / (timeSec * (float) sr));
        value = initial;
    }
    float next(float target) { return value += coeff * (target - value); }
};

// First-order DC blocker (~15 Hz).
struct DcBlocker {
    float r = 0.995f, x1 = 0.0f, y1 = 0.0f;
    void prepare(double sr) { r = 1.0f - kTwoPi * 15.0f / (float) sr; x1 = y1 = 0.0f; }
    float process(float x) { float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

struct Rng {  // xorshift32, deterministic for tests
    uint32_t s = 0x12345678u;
    float uniform() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) s / 4294967296.0f; }  // [0,1)
    float bipolar() { return uniform() * 2.0f - 1.0f; }
};

// amount 0 = exact bypass, 1 = tanh(10x)/tanh(10) (always within [-1, 1]).
inline float drive(float x, float amount) {
    if (amount <= 0.0f) return x;
    const float pre = 1.0f + 9.0f * amount;
    return x + amount * (std::tanh(pre * x) / std::tanh(pre) - x);
}

// Amplitude tremolo, R channel 90 degrees behind L (soft Suitcase-style auto-pan).
struct Tremolo {
    double sr = 44100.0;
    float phase = 0.0f;
    void prepare(double sampleRate) { sr = sampleRate; phase = 0.0f; }
    void process(float& l, float& r, float rateHz, float depth) {
        const float gl = 1.0f - depth * 0.5f * (1.0f - std::sin(phase));
        const float gr = 1.0f - depth * 0.5f * (1.0f - std::sin(phase - kTwoPi * 0.25f));
        l *= gl; r *= gr;
        phase += kTwoPi * rateHz / (float) sr;
        if (phase >= kTwoPi) phase -= kTwoPi;
    }
};

// Brightness: 2 x 1-pole low-pass, cutoff 1.2 kHz (tone 0, felt/dark) -> 20 kHz (tone 1, ~transparent).
struct Tone {
    double sr = 44100.0;
    Smoother toneS;
    float s[2][2] {};
    void prepare(double sampleRate, float initial) { sr = sampleRate; toneS.prepare(sr, initial); for (auto& c : s) c[0] = c[1] = 0.0f; }
    void process(float& l, float& r, float tone) {
        const float fc = 1200.0f * std::pow(20000.0f / 1200.0f, toneS.next(tone));
        const float a = 1.0f - std::exp(-kTwoPi * std::min(fc, 0.45f * (float) sr) / (float) sr);
        float* ch[2] = { &l, &r };
        for (int c = 0; c < 2; ++c) {
            s[c][0] += a * (*ch[c] - s[c][0]);
            s[c][1] += a * (s[c][0] - s[c][1]);
            *ch[c] = s[c][1];
        }
    }
};

// Slow 4-stage phaser (Phase 90 / Small Stone flavour): allpass notch sweep 300 Hz..2 kHz at 0.5 Hz,
// R LFO 90 degrees behind L. amount 0 = exact bypass, 1 = equal dry/wet (deepest notches).
struct Phaser {
    static constexpr float kRateHz = 0.5f, kLoHz = 300.0f, kHiHz = 2000.0f;
    double sr = 44100.0;
    float phase = 0.0f;
    Smoother amountS;
    float x1[2][4] {}, y1[2][4] {};
    void prepare(double sampleRate, float initial) {
        sr = sampleRate; phase = 0.0f; amountS.prepare(sr, initial);
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 4; ++k) x1[c][k] = y1[c][k] = 0.0f;
    }
    void process(float& l, float& r, float amount) {
        const float m = amountS.next(amount);
        phase += kTwoPi * kRateHz / (float) sr;
        if (phase >= kTwoPi) phase -= kTwoPi;
        if (m < 1e-5f) return;  // bypass
        float* ch[2] = { &l, &r };
        for (int c = 0; c < 2; ++c) {
            const float lfo = 0.5f + 0.5f * std::sin(phase - (float) c * kTwoPi * 0.25f);
            const float fc = kLoHz * std::pow(kHiHz / kLoHz, lfo);
            const float t = std::tan(3.14159265f * fc / (float) sr), k = (t - 1.0f) / (t + 1.0f);
            const float dry = *ch[c];
            float y = dry;
            for (int st = 0; st < 4; ++st) {  // first-order allpass: y = k*x + x[n-1] - k*y[n-1]
                const float out = k * y + x1[c][st] - k * y1[c][st];
                x1[c][st] = y; y1[c][st] = out; y = out;
            }
            *ch[c] = dry + 0.5f * m * (y - dry);
        }
    }
};

}  // namespace dsp
