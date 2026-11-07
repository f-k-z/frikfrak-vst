#pragma once
#include "dsp/Effects.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace dsp {

// Record-player stage: wow/flutter (modulated delay) + crackle + hiss -> HP 40 Hz -> LP "age".
// Fixed latency of ~1.5 ms is kept even when bypassed, so toggling never shifts timing.
class Vinyl {
public:
    // gate: crackle/hiss only sound while true (MIDI key held); wow/age always process the input.
    // pitch: frequency ratio of the noise colour (1 = crackle at 2.5 kHz, hiss at 6 kHz; 2 = one octave up).
    // level: noise loudness 0..1 (MIDI velocity).
    struct Params { float crackle = 0, hiss = 0, wow = 0, age = 0; bool on = true, gate = true; float pitch = 1, level = 1; };

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
        pitch = -1.0f;
        setPitch(1.0f);
        cx1 = cx2 = cy1 = cy2 = 0.0f;
        for (int c = 0; c < 2; ++c) { hpX[c] = hpY[c] = lp1[c] = lp2[c] = 0.0f; }
        wowS.prepare(sr, 0.0f, 0.05f);
        ageS.prepare(sr, 0.0f, 0.05f);
        crackleCount = 0;
        noiseEnv = 0.0f;
        envAtk = 1.0f - std::exp(-1.0f / (0.005f * (float) sr));
        envRel = 1.0f - std::exp(-1.0f / (0.15f * (float) sr));
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

        if (std::fabs(p.pitch - pitch) > 1e-6f) setPitch(p.pitch);  // recompute only on note change
        const float target = p.gate ? p.level : 0.0f;
        noiseEnv += (target > noiseEnv ? envAtk : envRel) * (target - noiseEnv);

        // Crackle: Poisson clicks, occasional loud pop, through a resonator. Centered (mono).
        float imp = 0.0f;
        if (p.crackle > 0.0f && noiseEnv > 1e-3f && rng.uniform() < 30.0f * p.crackle / (float) sr) {
            ++crackleCount;
            const float a = rng.uniform() < 0.05f ? 0.8f : 0.3f * std::pow(rng.uniform(), 3.0f);
            imp = (rng.uniform() < 0.5f ? -a : a) * p.crackle * noiseEnv;
        }
        const float cr = bb0 * imp + bb2 * cx2 - ba1 * cy1 - ba2 * cy2;  // direct form I (b1 = 0)
        cx2 = cx1; cx1 = imp; cy2 = cy1; cy1 = cr;
        // hiss (-55 dBFS RMS at hiss=1), independent per channel
        const float hl = (hissL += hissA * (rng.bipolar() - hissL)) * hissNorm;
        const float hr = (hissR += hissA * (rng.bipolar() - hissR)) * hissNorm;
        const float hissGain = p.hiss * kHissRms * noiseEnv;
        const float noise[2] = { cr + hl * hissGain, cr + hr * hissGain };

        const float lpA = onePoleCoeff(16000.0f * std::pow(6000.0f / 16000.0f, age));
        float* ch[2] = { &l, &r };
        for (int c = 0; c < 2; ++c) {
            const float x = readFrac(c, delay) + noise[c];  // noise goes through the age filter too
            const float hp = x - hpX[c] + hpR * hpY[c];  // 1-pole HP 40 Hz
            hpX[c] = x; hpY[c] = hp;
            lp1[c] += lpA * (hp - lp1[c]);  // 2 x 1-pole LP
            lp2[c] += lpA * (lp1[c] - lp2[c]);
            *ch[c] = lp2[c];
        }

        advance();
    }

    long crackleCount = 0;  // for tests

private:
    static constexpr float kWowHz = 0.5f, kFlutterHz = 8.0f, kHissRms = 0.0017783f;  // -55 dBFS

    // Noise colour follows the played note: hiss low-pass and crackle resonator move together.
    void setPitch(float ratio) {
        pitch = ratio;
        hissA = onePoleCoeff(std::clamp(6000.0f * ratio, 400.0f, 0.45f * (float) sr));
        hissNorm = std::sqrt((2.0f - hissA) / hissA) / std::sqrt(1.0f / 3.0f);  // post-LP RMS = 1 at any pitch
        // crackle resonator: RBJ band-pass, Q 1.5, 0 dB peak
        const float w = kTwoPi * std::clamp(2500.0f * ratio, 150.0f, 0.4f * (float) sr) / (float) sr, alpha = std::sin(w) / (2.0f * 1.5f);
        const float a0 = 1.0f + alpha;
        bb0 = alpha / a0; bb2 = -alpha / a0; ba1 = -2.0f * std::cos(w) / a0; ba2 = (1.0f - alpha) / a0;
    }

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
    float noiseEnv = 0, envAtk = 1, envRel = 1, pitch = 1;
    float bb0 = 0, bb2 = 0, ba1 = 0, ba2 = 0, cx1 = 0, cx2 = 0, cy1 = 0, cy2 = 0;
};

}  // namespace dsp
