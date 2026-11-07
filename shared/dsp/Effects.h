#pragma once
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

}  // namespace dsp
