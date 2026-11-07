#pragma once
#include "dsp/Effects.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace dsp {

// Record-player stage: wow/flutter (modulated delay) -> HP 40 Hz -> LP "age" -> + crackle + hiss.
// Fixed latency of ~1.5 ms is kept even when bypassed, so toggling never shifts timing.
class Vinyl {
public:
    struct Params { float crackle = 0, hiss = 0, wow = 0, age = 0; bool on = true; };

    void prepare(double sampleRate, uint32_t seed = 0x12345678u) {
        sr = sampleRate;
        latency = (int) std::lround(0.0015 * sr);
        size = 1;
        while (size < latency * 2 + 8) size <<= 1;
        buf[0].assign((size_t) size, 0.0f);
        buf[1].assign((size_t) size, 0.0f);
        write = 0;
        rng.s = seed ? seed : 1u;
        wowPhase = flutterPhase = 0.0f;
        hpR = 1.0f - kTwoPi * 40.0f / (float) sr;
        hissL = hissR = 0.0f;
        hissA = onePoleCoeff(6000.0f);
        hissNorm = std::sqrt((2.0f - hissA) / hissA) / std::sqrt(1.0f / 3.0f);  // post-LP RMS = 1
        // crackle resonator: RBJ band-pass ~2.5 kHz, Q 1.5
        const float w = kTwoPi * 2500.0f / (float) sr, alpha = std::sin(w) / (2.0f * 1.5f);
        const float a0 = 1.0f + alpha;
        bb0 = alpha / a0; bb2 = -alpha / a0; ba1 = -2.0f * std::cos(w) / a0; ba2 = (1.0f - alpha) / a0;
        cx1 = cx2 = cy1 = cy2 = 0.0f;
        for (int c = 0; c < 2; ++c) { hpX[c] = hpY[c] = lp1[c] = lp2[c] = 0.0f; }
        wowS.prepare(sr, 0.0f, 0.05f);
        ageS.prepare(sr, 0.0f, 0.05f);
        crackleCount = 0;
    }

    int latencySamples() const { return latency; }

    void process(float& l, float& r, const Params& p) {
        buf[0][(size_t) write] = l;
        buf[1][(size_t) write] = r;
        const float wow = wowS.next(p.wow), age = ageS.next(p.age);

        if (!p.on) {
            const int idx = (write - latency) & (size - 1);
            l = buf[0][(size_t) idx]; r = buf[1][(size_t) idx];
            advance();
            return;
        }

        // Delay modulation sized by pitch deviation: dev = 2*pi*f*A  ->  A = dev / (2*pi*f)
        const float aWow = 0.003f * wow / (kTwoPi * kWowHz);           // up to ~0.3 % pitch
        const float aFlutter = 0.0005f * wow / (kTwoPi * kFlutterHz);  // up to ~0.05 % pitch
        const float delay = (float) latency + (float) sr * (aWow * std::sin(wowPhase) + aFlutter * std::sin(flutterPhase));
        wowPhase = wrap(wowPhase + kTwoPi * kWowHz / (float) sr);
        flutterPhase = wrap(flutterPhase + kTwoPi * kFlutterHz / (float) sr);

        const float lpA = onePoleCoeff(16000.0f * std::pow(6000.0f / 16000.0f, age));
        float* ch[2] = { &l, &r };
        for (int c = 0; c < 2; ++c) {
            float x = readFrac(c, delay);
            const float hp = x - hpX[c] + hpR * hpY[c];  // 1-pole HP 40 Hz
            hpX[c] = x; hpY[c] = hp;
            lp1[c] += lpA * (hp - lp1[c]);  // 2 x 1-pole LP
            lp2[c] += lpA * (lp1[c] - lp2[c]);
            *ch[c] = lp2[c];
        }

        // Crackle: Poisson clicks, occasional loud pop, through a resonator. Centered (mono).
        float imp = 0.0f;
        if (p.crackle > 0.0f && rng.uniform() < 30.0f * p.crackle / (float) sr) {
            ++crackleCount;
            const float a = rng.uniform() < 0.05f ? 0.8f : 0.3f * std::pow(rng.uniform(), 3.0f);
            imp = (rng.uniform() < 0.5f ? -a : a) * p.crackle;
        }
        const float cr = bb0 * imp + bb2 * cx2 - ba1 * cy1 - ba2 * cy2;  // direct form I (b1 = 0)
        cx2 = cx1; cx1 = imp; cy2 = cy1; cy1 = cr;
        // hiss (-55 dBFS RMS at hiss=1), independent per channel
        const float hl = (hissL += hissA * (rng.bipolar() - hissL)) * hissNorm;
        const float hr = (hissR += hissA * (rng.bipolar() - hissR)) * hissNorm;
        const float hissGain = p.hiss * kHissRms;
        l += cr + hl * hissGain;
        r += cr + hr * hissGain;
        advance();
    }

    long crackleCount = 0;  // for tests

private:
    static constexpr float kWowHz = 0.5f, kFlutterHz = 8.0f, kHissRms = 0.0017783f;  // -55 dBFS

    float onePoleCoeff(float fc) const { return 1.0f - std::exp(-kTwoPi * fc / (float) sr); }
    static float wrap(float ph) { return ph >= kTwoPi ? ph - kTwoPi : ph; }
    void advance() { write = (write + 1) & (size - 1); }

    float readFrac(int c, float delay) const {  // 4-point Hermite
        const float pos = (float) write - delay;
        const int i = (int) std::floor(pos);
        const float t = pos - (float) i;
        auto at = [&](int k) { return buf[c][(size_t) ((i + k) & (size - 1))]; };
        const float y0 = at(-1), y1 = at(0), y2 = at(1), y3 = at(2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    double sr = 44100.0;
    int latency = 0, size = 1, write = 0;
    std::vector<float> buf[2];
    Rng rng;
    Smoother wowS, ageS;
    float wowPhase = 0, flutterPhase = 0, hpR = 0.99f, hpX[2] {}, hpY[2] {}, lp1[2] {}, lp2[2] {};
    float hissA = 0.5f, hissNorm = 1.0f, hissL = 0, hissR = 0;
    float bb0 = 0, bb2 = 0, ba1 = 0, ba2 = 0, cx1 = 0, cx2 = 0, cy1 = 0, cy2 = 0;
};

}  // namespace dsp
