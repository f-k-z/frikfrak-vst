#pragma once
// One Kankyo voice, pure C++: 3 detuned morphing oscillators (tri -> saw -> pulse, polyBLEP) through a
// stereo SVF low-pass, a 2-operator FM layer (bells / glass / mallets), band-passed breath noise,
// and an ADSR (linear attack, exponential decay/release). Output stereo, ~0.25 peak per voice.
#include "dsp/Effects.h"
#include <cmath>

namespace dsp {

class Pad {
public:
    struct Params {
        float blend = 0.3f;    // 0 analog .. 1 FM
        float shape = 0.5f;    // 0 tri, 0.5 saw, 1 pulse
        float detune = 0.4f;   // 0..1 -> 0..25 cents spread over 3 oscillators
        float color = 0.45f;   // cutoff 150 Hz .. 16 kHz (exponential)
        float ratio = 3.5f;    // FM modulator : carrier
        float bright = 0.3f;   // FM index 0..6
        float air = 0.0f;      // breath noise level
        float motion = 0.3f;   // slow LFO on cutoff and pulse width
        float attack = 1.2f, decay = 2.0f, sustain = 0.8f, release = 4.0f;  // seconds / level
    };

    void prepare(double sampleRate) { sr = sampleRate; level = 0.0f; stage = Stage::Off; }

    void noteOn(float freqHz, float velocity, uint32_t seed = 1) {
        f0 = freqHz;
        vel = 0.4f + 0.6f * velocity;
        if (stage == Stage::Off) {  // fresh voice: reset state; retrigger keeps phases (no click)
            for (int i = 0; i < 3; ++i) ph[i] = 0.31f * (float) i;
            pc = pm = 0.0f;
            for (auto& s : svf) s = {};
            noiseF = {};
            rng.s = seed * 2654435761u + 1u;
            lfoPh = rng.uniform() * kTwoPi;
            lfoHz = 0.1f + 0.2f * rng.uniform();
        }
        idx = 1.0f;
        stage = Stage::Attack;
    }
    void noteOff() { if (stage != Stage::Off) stage = Stage::Release; }
    bool isActive() const { return stage != Stage::Off; }
    float envelope() const { return level; }

    void process(float& outL, float& outR, const Params& p) {
        if (stage == Stage::Off) { outL = outR = 0.0f; return; }
        advanceEnvelope(p);

        lfoPh += kTwoPi * lfoHz / (float) sr;
        if (lfoPh >= kTwoPi) lfoPh -= kTwoPi;
        const float lfo = std::sin(lfoPh) * p.motion;

        // Analog layer: 3 oscillators, outer ones panned
        const float cents = 25.0f * p.detune, pw = 0.5f + 0.2f * lfo;
        float aL = 0.0f, aR = 0.0f;
        for (int i = 0; i < 3; ++i) {
            const float f = f0 * std::exp2(cents * (float) (i - 1) / 1200.0f);
            const float dt = std::min(f / (float) sr, 0.45f);
            const float w = wave(ph[i], dt, p.shape, pw);
            ph[i] += dt;
            if (ph[i] >= 1.0f) ph[i] -= 1.0f;
            const float pan = 0.5f + 0.35f * (float) (i - 1);
            aL += w * (1.0f - pan);
            aR += w * pan;
        }
        const float key = std::sqrt(std::max(f0, 20.0f) / 261.6f);  // half key tracking
        const float fc = std::clamp(150.0f * std::pow(16000.0f / 150.0f, p.color) * key * (1.0f + 0.35f * lfo), 30.0f, 0.45f * (float) sr);
        const float gA = Svf::coeff(fc, (float) sr);  // one tan() shared by both channels
        aL = svf[0].lowpass(aL * 0.45f, gA);
        aR = svf[1].lowpass(aR * 0.45f, gA);

        // FM layer: index decays toward sustain with ~0.3 x decay time (woody mallets when sustain is low)
        idx += (p.sustain - idx) * (1.0f - std::exp(-1.0f / (0.3f * p.decay * (float) sr)));
        const float mod = std::sin(kTwoPi * pm) * 6.0f * p.bright * idx;
        const float fm = std::sin(kTwoPi * pc + mod) * 0.7f;
        pc += f0 / (float) sr; if (pc >= 1.0f) pc -= 1.0f;
        pm += f0 * p.ratio / (float) sr; pm -= std::floor(pm);

        // Breath: band-passed noise around the cutoff
        const float n = p.air > 0.0f ? noiseF.bandpass(rng.bipolar(), Svf::coeff(std::clamp(fc * 1.5f, 200.0f, 0.4f * (float) sr), (float) sr)) * p.air * 1.2f : 0.0f;

        const float g = level * vel * 0.25f;
        outL = g * ((1.0f - p.blend) * aL + p.blend * fm + n);
        outR = g * ((1.0f - p.blend) * aR + p.blend * fm + n);
    }

private:
    enum class Stage { Off, Attack, Decay, Release };

    struct Svf {  // TPT state-variable filter (Zavalishin), Q ~0.8
        float s1 = 0.0f, s2 = 0.0f;
        static float coeff(float fc, float sr) { return std::tan(3.14159265f * fc / sr); }
        void run(float x, float g, float& lp, float& bp) {
            const float k = 1.25f;
            const float h = 1.0f / (1.0f + g * (g + k));
            const float hp = (x - (g + k) * s1 - s2) * h;
            bp = g * hp + s1; s1 = g * hp + bp;
            lp = g * bp + s2; s2 = g * bp + lp;
        }
        float lowpass(float x, float g) { float lp, bp; run(x, g, lp, bp); return lp; }
        float bandpass(float x, float g) { float lp, bp; run(x, g, lp, bp); return bp; }
    };

    static float blep(float t, float dt) {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    static float wave(float t, float dt, float shape, float pw) {
        const float tri = 4.0f * std::fabs(t - 0.5f) - 1.0f;
        const float saw = 2.0f * t - 1.0f - blep(t, dt);
        if (shape <= 0.5f) return tri + 2.0f * shape * (saw - tri);
        float t2 = t + 1.0f - pw; if (t2 >= 1.0f) t2 -= 1.0f;
        const float pulse = (t < pw ? 1.0f : -1.0f) + blep(t, dt) - blep(t2, dt);
        return saw + 2.0f * (shape - 0.5f) * (pulse - saw);
    }

    void advanceEnvelope(const Params& p) {
        switch (stage) {
            case Stage::Attack:
                level += 1.0f / (std::max(p.attack, 0.001f) * (float) sr);
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:  // -60 dB per `decay` seconds toward sustain
                level += (p.sustain - level) * (1.0f - std::exp(-6.9f / (std::max(p.decay, 0.01f) * (float) sr)));
                if (p.sustain <= 0.0f && level < 1e-4f) { level = 0.0f; stage = Stage::Off; }
                break;
            case Stage::Release:  // -60 dB per `release` seconds, from wherever we are
                level *= std::exp(-6.9f / (std::max(p.release, 0.01f) * (float) sr));
                if (level < 1e-4f) { level = 0.0f; stage = Stage::Off; }
                break;
            case Stage::Off: break;
        }
    }

    double sr = 44100.0;
    Stage stage = Stage::Off;
    float level = 0.0f, f0 = 440.0f, vel = 1.0f, idx = 1.0f;
    float ph[3] {}, pc = 0.0f, pm = 0.0f, lfoPh = 0.0f, lfoHz = 0.2f;
    Svf svf[2], noiseF;
    Rng rng;
};

}  // namespace dsp
