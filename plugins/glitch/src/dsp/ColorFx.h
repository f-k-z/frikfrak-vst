#pragma once
// Instant "colour" effects, pure C++. Each returns its input bit-exact while disengaged.
#include "dsp/BeatClock.h"
#include "dsp/Effects.h"
#include "dsp/Engage.h"
#include <cmath>

namespace dsp {

// Pitch shift with two rotating read heads (DAFX delay-line shifter): 60 ms window, sin² gains
// summing to 1, each head muted at its wrap. No lookahead; ~30 ms average delay is part of the
// effect. 0 semitones never engages (two frozen heads would comb-filter the mix).
class PitchShift {
public:
    void prepare(double sr) { hist.prepare(sr, 0.2); eng.prepare(sr); win = 0.06 * sr; phase = 0.0; ratio = 1.0; }
    bool idle() const { return eng.idle(); }

    void process(float& l, float& r, bool on, float semis) {
        hist.push(l, r);
        const bool shift = on && std::fabs(semis) >= 0.5f;
        if (shift) ratio = std::exp2((double) std::round(semis) / 12.0);
        const float e = eng.next(shift);
        if (e <= 0.0f) { phase = 0.0; return; }
        phase += (1.0 - ratio) / win;
        phase -= std::floor(phase);
        const double pb = phase + 0.5 - std::floor(phase + 0.5);
        const float ga = (float) std::pow(std::sin(3.14159265358979 * phase), 2.0), gb = 1.0f - ga;
        const double da = 3.0 + phase * win, db = 3.0 + pb * win;
        l = blend(l, ga * hist.read(0, da) + gb * hist.read(0, db), e);
        r = blend(r, ga * hist.read(1, da) + gb * hist.read(1, db), e);
    }

private:
    History hist;
    Engage eng;
    double win = 2880.0, phase = 0.0, ratio = 1.0;
};

// Bit crusher: sample-and-hold decimation (aliasing is the point) then quantisation to `bits`.
class Crush {
public:
    void prepare(double sampleRate) { sr = sampleRate; eng.prepare(sr); acc = 1.0; }
    bool idle() const { return eng.idle(); }

    void process(float& l, float& r, bool on, float bits, float rateKHz) {
        const float e = eng.next(on);
        if (e <= 0.0f) { acc = 1.0; return; }  // next engage captures the first sample
        acc += 1000.0 * rateKHz / sr;
        if (acc >= 1.0) { acc -= std::floor(acc); hl = l; hr = r; }
        const float q = std::exp2(bits - 1.0f);
        l = blend(l, std::round(hl * q) / q, e);
        r = blend(r, std::round(hr * q) / q, e);
    }

private:
    double sr = 48000.0, acc = 1.0;
    float hl = 0.0f, hr = 0.0f;
    Engage eng;
};

// DJ filter on one knob: < 0 low-pass 20 kHz -> 200 Hz, > 0 high-pass 20 Hz -> 4 kHz, ±3 % dead
// zone in the middle (pass-through). 2-pole TPT state-variable filter (Zavalishin), mild Q.
class DjFilter {
public:
    static constexpr float kDead = 0.03f;
    void prepare(double sampleRate) { sr = sampleRate; eng.prepare(sr); amountS.prepare(sr, 0.0f, 0.03f); reset(); }
    bool idle() const { return eng.idle(); }

    void process(float& l, float& r, bool on, float freq) {
        const bool active = on && std::fabs(freq) > kDead;
        if (active) { if ((freq < 0) != lowPass) { lowPass = freq < 0; reset(); } knob = freq; }
        const float e = eng.next(active);
        if (e <= 0.0f) { reset(); return; }
        const float a = amountS.next((std::fabs(knob) - kDead) / (1.0f - kDead));
        const float fc = lowPass ? 20000.0f * std::pow(0.01f, a) : 20.0f * std::pow(200.0f, a);
        const float g = std::tan(3.14159265f * std::min(fc, 0.45f * (float) sr) / (float) sr), k = 1.0f / 0.9f;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        float* ch[2] = { &l, &r };
        for (int c = 0; c < 2; ++c) {
            const float x = *ch[c];
            const float v3 = x - ic2[c], v1 = a1 * ic1[c] + a2 * v3, v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
            ic1[c] = 2.0f * v1 - ic1[c];
            ic2[c] = 2.0f * v2 - ic2[c];
            *ch[c] = blend(x, lowPass ? v2 : x - k * v1 - v2, e);
        }
    }

private:
    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.0f; amountS.value = 0.0f; }
    double sr = 48000.0;
    Engage eng;
    Smoother amountS;
    float ic1[2] {}, ic2[2] {}, knob = 0.0f;
    bool lowPass = true;
};

// Rhythmic gate: open the first half of each grid step, `depth` attenuation on the second half.
// Gain smoothed (2 ms open, 10 ms close) so the chops never click.
class Gate {
public:
    void prepare(double sr) {
        eng.prepare(sr);
        atk = 1.0f - std::exp(-1.0f / (0.002f * (float) sr));
        rel = 1.0f - std::exp(-1.0f / (0.010f * (float) sr));
        g = 1.0f;
    }
    bool idle() const { return eng.idle(); }

    void process(float& l, float& r, bool on, double qn, float depth, double ppq) {
        const float e = eng.next(on);
        if (e <= 0.0f) { g = 1.0f; return; }
        const float target = BeatClock::phase(ppq, qn) < 0.5 ? 1.0f : 1.0f - depth;
        g += (target > g ? atk : rel) * (target - g);
        l = blend(l, l * g, e);
        r = blend(r, r * g, e);
    }

private:
    Engage eng;
    float atk = 0.01f, rel = 0.002f, g = 1.0f;
};

// Shared phaser with Glitch's speed knob, engaged with the common 8 ms ramp.
class GlitchPhaser {
public:
    void prepare(double sr) { ph.prepare(sr, 1.0f); eng.prepare(sr); }
    bool idle() const { return eng.idle(); }

    void process(float& l, float& r, bool on, float hz) {
        const float e = eng.next(on);
        if (e <= 0.0f) return;
        float wl = l, wr = r;
        ph.process(wl, wr, 1.0f, hz);
        l = blend(l, wl, e);
        r = blend(r, wr, e);
    }

private:
    Phaser ph;
    Engage eng;
};

}  // namespace dsp
