#pragma once
// Space effects, pure C++: Juno-style stereo chorus and an 8-line FDN hall reverb.
// Buffers are allocated in prepare(), never in process().
#include "dsp/Effects.h"
#include <array>
#include <vector>

namespace dsp {

// Two BBD-like modulated delays, L and R LFOs in opposition (Juno-60 style).
// amount 0 = exact bypass; rises mix and LFO rate (~0.5 Hz "I" -> ~0.83 Hz "II").
class Chorus {
public:
    void prepare(double sampleRate, float initial) {
        sr = sampleRate;
        size = 1;
        while (size < (int) (0.02 * sr) + 4) size <<= 1;
        for (auto& b : buf) b.assign((size_t) size, 0.0f);
        write = 0; phase = 0.0f;
        amountS.prepare(sr, initial);
    }
    void process(float& l, float& r, float amount) {
        buf[0][(size_t) write] = l;
        buf[1][(size_t) write] = r;
        const float a = amountS.next(amount);
        phase += kTwoPi * (0.5f + 0.33f * a) / (float) sr;
        if (phase >= kTwoPi) phase -= kTwoPi;
        if (a >= 1e-5f) {
            const float tri = 2.0f * std::fabs(phase / kTwoPi - 0.5f);  // 0..1 triangle
            const float base = 0.0035f * (float) sr, depth = 0.0017f * (float) sr;
            const float wl = read(0, base + depth * (2.0f * tri - 1.0f));
            const float wr = read(1, base - depth * (2.0f * tri - 1.0f));
            l += 0.5f * a * (wl - l);
            r += 0.5f * a * (wr - r);
        }
        write = (write + 1) & (size - 1);
    }

private:
    float read(int c, float delay) const {
        const float pos = (float) write - delay;
        const int i = (int) std::floor(pos);
        const float t = pos - (float) i;
        const float a = buf[c][(size_t) (i & (size - 1))], b = buf[c][(size_t) ((i + 1) & (size - 1))];
        return a + t * (b - a);
    }
    double sr = 44100.0;
    int size = 1, write = 0;
    float phase = 0.0f;
    Smoother amountS;
    std::vector<float> buf[2];
};

// 8-line feedback delay network, Householder mixing, per-line damping and slow delay modulation.
// mix 0 = exact bypass, 1 = fully wet. size 0..1 -> RT60 1.5 s .. 10 s.
class Reverb {
public:
    static constexpr int kLines = 8;
    static float rt60For(float size) { return 1.5f * std::pow(10.0f / 1.5f, size); }

    void prepare(double sampleRate, float initialMix, float initialSize) {
        sr = sampleRate;
        static constexpr std::array<int, kLines> base48 { 1559, 1907, 2297, 2711, 3089, 3539, 3967, 4421 };  // primes, ~32-92 ms
        for (size_t k = 0; k < (size_t) kLines; ++k) {
            len[k] = (float) base48[k] * (float) (sr / 48000.0);
            int n = 1;
            while (n < (int) len[k] + 16) n <<= 1;
            line[k].assign((size_t) n, 0.0f);
            mask[k] = n - 1;
            damp[k] = 0.0f;
            modPhase[k] = (float) k * 0.7f;
        }
        preSize = 1;
        while (preSize < (int) (0.03 * sr) + 2) preSize <<= 1;
        pre.assign((size_t) preSize, 0.0f);
        preDelay = (int) (0.03 * sr);
        write = preWrite = 0;
        dampA = 1.0f - std::exp(-kTwoPi * 5000.0f / (float) sr);
        mixS.prepare(sr, initialMix);
        sizeS.prepare(sr, initialSize, 0.1f);
        lastSize = -1.0f;
    }

    void process(float& l, float& r, float mix, float size) {
        const float m = mixS.next(mix), s = sizeS.next(size);
        if (std::fabs(s - lastSize) > 1e-4f) setDecay(s);

        pre[(size_t) preWrite] = 0.5f * (l + r);
        const float in = pre[(size_t) ((preWrite - preDelay) & (preSize - 1))];
        preWrite = (preWrite + 1) & (preSize - 1);

        std::array<float, kLines> y {};
        float sum = 0.0f;
        for (size_t k = 0; k < (size_t) kLines; ++k) {
            modPhase[k] += kTwoPi * (0.07f + 0.031f * (float) k) / (float) sr;
            if (modPhase[k] >= kTwoPi) modPhase[k] -= kTwoPi;
            const float d = len[k] + (k < 4 ? 6.0f * std::sin(modPhase[k]) : 0.0f);  // anti-metallic wobble
            damp[k] += dampA * (read(k, d) - damp[k]);
            y[(size_t) k] = damp[k];
            sum += damp[k];
        }
        const float h = sum * (2.0f / (float) kLines);  // Householder: x - 2/N * sum
        float wl = 0.0f, wr = 0.0f;
        for (size_t k = 0; k < (size_t) kLines; ++k) {
            const float x = y[(size_t) k] - h;
            line[k][(size_t) (write & mask[k])] = in * inGain * ((k & 1) ? -1.0f : 1.0f) + gain[k] * x;
            if (k & 1) wr += y[(size_t) k]; else wl += y[(size_t) k];
        }
        write++;
        if (m >= 1e-5f) {
            l += m * (0.5f * wl - l);
            r += m * (0.5f * wr - r);
        }
    }

private:
    void setDecay(float s) {
        lastSize = s;
        const float rt = rt60For(s);
        float g2 = 0.0f;
        for (size_t k = 0; k < (size_t) kLines; ++k) { gain[k] = std::pow(10.0f, -3.0f * len[k] / (rt * (float) sr)); g2 += gain[k] * gain[k]; }
        inGain = std::sqrt(1.0f - g2 / (float) kLines);  // steady-state level independent of decay
    }
    float read(size_t k, float delay) const {
        const float pos = (float) write - delay;
        const int i = (int) std::floor(pos);
        const float t = pos - (float) i;
        const float a = line[k][(size_t) (i & mask[k])], b = line[k][(size_t) ((i + 1) & mask[k])];
        return a + t * (b - a);
    }

    double sr = 44100.0;
    std::array<std::vector<float>, kLines> line;
    std::array<int, kLines> mask {};
    std::array<float, kLines> len {}, gain {}, damp {}, modPhase {};
    std::vector<float> pre;
    int preSize = 1, preDelay = 0, write = 0, preWrite = 0;
    float dampA = 0.5f, lastSize = -1.0f, inGain = 1.0f;
    Smoother mixS, sizeS;
};

}  // namespace dsp
