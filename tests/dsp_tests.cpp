// Property tests for the pure-C++ DSP. CHECK (not assert) so they also run under NDEBUG.
#include "dsp/Effects.h"
#include "dsp/RhodesTine.h"
#include "dsp/Vinyl.h"
#include "dsp/Space.h"
#include "dsp/Pad.h"
#include "dsp/Engage.h"
#include "dsp/BeatClock.h"
#include "dsp/Pads.h"
#include "dsp/TimeFx.h"
#include "dsp/ColorFx.h"
#include "Presets.h"
#include "../plugins/kankyo/src/Presets.h"
#include <set>
#include <string>
#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

using Buf = std::vector<float>;
constexpr double kPi = 3.14159265358979;

static double goertzel(const Buf& x, size_t from, size_t n, double sr, double f) {  // amplitude at f
    const double w = 2 * kPi * f / sr, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < from + n; ++i) { const double s = x[i] + c * s1 - s2; s2 = s1; s1 = s; }
    return 2.0 * std::sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / (double) n;
}
static double peakFreq(const Buf& x, size_t from, size_t n, double sr, double around) {
    double best = 0, bestF = 0;
    for (double f = around * 0.9; f <= around * 1.1; f += around * 0.001)
        if (double a = goertzel(x, from, n, sr, f); a > best) { best = a; bestF = f; }
    return bestF;
}
static double rms(const Buf& x, size_t from, size_t n) {
    double s = 0; for (size_t i = from; i < from + n; ++i) s += x[i] * x[i];
    return std::sqrt(s / (double) n);
}
static double maxStep(const Buf& x, size_t from, size_t n) {
    double m = 0; for (size_t i = from + 1; i < from + n; ++i) m = std::max(m, (double) std::fabs(x[i] - x[i - 1]));
    return m;
}
static Buf renderNote(double sr, int note, float vel, double seconds, double offAt = -1) {
    dsp::RhodesTine t; t.prepare(sr); t.noteOn(note, vel);
    Buf out((size_t) (seconds * sr));
    for (size_t i = 0; i < out.size(); ++i) {
        if (offAt >= 0 && i == (size_t) (offAt * sr)) t.noteOff();
        out[i] = t.process();
    }
    return out;
}
static bool finiteAndBounded(const Buf& x, float lim) {
    for (float v : x) if (!std::isfinite(v) || std::fabs(v) > lim) return false;
    return true;
}

static void testTine() {
    for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        auto x = renderNote(sr, 69, 0.8f, 0.6);
        const double f = peakFreq(x, (size_t) (0.05 * sr), (size_t) (0.5 * sr), sr, 440);
        CHECK(std::fabs(f - 440) < 4.4, "sr=%.0f pitch=%.2f", sr, f);
    }
    const double sr = 48000;
    auto hard = renderNote(sr, 69, 1.0f, 0.6), soft = renderNote(sr, 69, 0.2f, 0.6);
    auto harmRatio = [&](const Buf& x) {
        const size_t a = (size_t) (0.05 * sr), n = (size_t) (0.5 * sr);
        return (goertzel(x, a, n, sr, 880) + goertzel(x, a, n, sr, 1320)) / goertzel(x, a, n, sr, 440);
    };
    CHECK(harmRatio(hard) > 3 * harmRatio(soft), "bark hard=%.4f soft=%.4f", harmRatio(hard), harmRatio(soft));
    const double dB = 20 * std::log10(rms(hard, 0, hard.size()) / rms(soft, 0, soft.size()));
    CHECK(dB >= 12, "velocity loudness diff=%.1f dB", dB);

    // DC removed: mean of a held hard note ~ 0
    double mean = 0; for (size_t i = (size_t) (0.1 * sr); i < hard.size(); ++i) mean += hard[i];
    mean /= (double) hard.size();
    CHECK(std::fabs(mean) < 2e-3, "dc mean=%.5f", mean);

    // Held note decays monotonically, still audible after 1 s
    auto held = renderNote(sr, 60, 0.8f, 3.0);
    const size_t w = (size_t) (0.1 * sr);
    double prev = 1e9; bool mono = true;
    for (size_t k = 1; k < 30; ++k) { const double r = rms(held, k * w, w); if (r > prev * 1.02) mono = false; prev = r; }
    CHECK(mono, "decay not monotonic");
    CHECK(rms(held, 10 * w, w) > 0.05 * rms(held, w, w), "decays too fast");

    // noteOff: < -60 dB within 300 ms, no click
    auto rel = renderNote(sr, 60, 0.8f, 1.0, 0.5);
    const size_t off = (size_t) (0.5 * sr);
    const double peak = rms(rel, off - w, w);
    CHECK(rms(rel, off + (size_t) (0.29 * sr), (size_t) (0.01 * sr)) < peak * 1e-3, "release too slow");
    CHECK(maxStep(rel, off - 5, 20) <= maxStep(rel, off - w, w) * 1.1, "click at noteOff");

    // Retrigger / voice steal on a sounding voice: no step
    {
        dsp::RhodesTine t; t.prepare(sr); t.noteOn(69, 1.0f);
        float prev = 0, peakAbs = 0;
        for (int i = 0; i < (int) (0.1 * sr); ++i) peakAbs = std::max(peakAbs, std::fabs(prev = t.process()));
        while (std::fabs(prev) < 0.8f * peakAbs * 0.9f) prev = t.process();  // steal near a crest = worst case
        t.noteOn(72, 1.0f);
        const float next = t.process();
        CHECK(std::fabs(next - prev) < 0.2f * std::fabs(prev), "click on steal: %.4f -> %.4f", prev, next);
    }

    // Edges finite and bounded; top note has no strong alias
    for (int note : { 21, 108 }) CHECK(finiteAndBounded(renderNote(44100, note, 1.0f, 1.0), 1.0f), "note %d unbounded", note);
    auto top = renderNote(44100, 108, 1.0f, 0.3);
    const double f0 = 440 * std::pow(2.0, 39 / 12.0), alias = 6 * f0 - 44100;  // 6th harmonic folds to ~19 kHz
    const double ratio = goertzel(top, 441, 8820, 44100, std::fabs(alias)) / goertzel(top, 441, 8820, 44100, f0);
    CHECK(ratio < 0.01, "alias ratio=%.4f", ratio);
}

static void testEffects() {
    CHECK(dsp::drive(0.37f, 0.0f) == 0.37f, "drive 0 not bypass");
    for (float x = -4.0f; x <= 4.0f; x += 0.01f) {
        const float y = dsp::drive(x > 1 ? 1.0f : (x < -1 ? -1.0f : x), 1.0f);
        if (!(std::isfinite(y) && std::fabs(y) <= 1.0f)) { CHECK(false, "drive out of range at %f", x); break; }
    }
    const double sr = 48000;
    dsp::Tremolo tr; tr.prepare(sr);
    float l = 0.5f, r = 0.5f; tr.process(l, r, 4.0f, 0.0f);
    CHECK(l == 0.5f && r == 0.5f, "depth 0 not bypass");
    tr.prepare(sr);
    Buf gl((size_t) (2 * sr)), gr(gl.size());
    for (size_t i = 0; i < gl.size(); ++i) { float a = 1, b = 1; tr.process(a, b, 4.0f, 1.0f); gl[i] = a; gr[i] = b; }
    int peaks = 0;  // count maxima of the L gain over 2 s -> 8 at 4 Hz
    for (size_t i = 1; i + 1 < gl.size(); ++i) if (gl[i] > gl[i - 1] && gl[i] >= gl[i + 1] && gl[i] > 0.99f) ++peaks;
    CHECK(peaks >= 7 && peaks <= 9, "tremolo peaks=%d", peaks);
    size_t argL = 0, argR = 0;  // R lags L by a quarter period (62.5 ms at 4 Hz)
    for (size_t i = 0; i < (size_t) (sr / 4); ++i) { if (gl[i] > gl[argL]) argL = i; if (gr[i] > gr[argR]) argR = i; }
    const double lagMs = 1000.0 * (double) ((argR + (size_t) (sr / 4) - argL) % (size_t) (sr / 4)) / sr;
    CHECK(std::fabs(lagMs - 62.5) < 3, "stereo lag=%.1f ms", lagMs);
}

static Buf runVinyl(dsp::Vinyl& v, const dsp::Vinyl::Params& p, const Buf& in, Buf* right = nullptr) {
    Buf out(in.size());
    for (size_t i = 0; i < in.size(); ++i) { float l = in[i], r = in[i]; v.process(l, r, p); out[i] = l; if (right) (*right)[i] = r; }
    return out;
}
static Buf sine(double sr, double f, double sec, float amp) {
    Buf x((size_t) (sec * sr)); for (size_t i = 0; i < x.size(); ++i) x[i] = amp * (float) std::sin(2 * kPi * f * (double) i / sr);
    return x;
}

static void testVinyl() {
    const double sr = 48000;
    dsp::Vinyl v;
    // Transparent when everything is at 0
    v.prepare(sr);
    auto s = sine(sr, 1000, 1.0, 0.5f);
    auto y = runVinyl(v, {}, s);
    const double dB = 20 * std::log10(rms(y, 4800, 38400) / rms(s, 4800, 38400));
    CHECK(std::fabs(dB) < 0.2, "not transparent: %.3f dB", dB);

    // Bypass: exact delayed copy, no noise
    v.prepare(sr);
    dsp::Vinyl::Params off; off.on = false; off.crackle = off.hiss = off.wow = off.age = 1;
    y = runVinyl(v, off, s);
    const int L = v.latencySamples();
    bool exact = true;
    for (size_t i = (size_t) L; i < s.size(); ++i) if (y[i] != s[i - (size_t) L]) { exact = false; break; }
    CHECK(exact && L > 0, "bypass not a pure delay (latency %d)", L);

    // Crackle count follows rate (30/s at 1) on silence, seed fixed
    v.prepare(sr, 42);
    dsp::Vinyl::Params cr; cr.crackle = 1;
    Buf silence((size_t) (10 * sr), 0.0f);
    runVinyl(v, cr, silence);
    CHECK(v.crackleCount > 240 && v.crackleCount < 360, "crackle count=%ld", v.crackleCount);

    // Hiss level ~ -55 dBFS
    v.prepare(sr, 7);
    dsp::Vinyl::Params hs; hs.hiss = 1;
    y = runVinyl(v, hs, Buf((size_t) (2 * sr), 0.0f));
    const double hissDb = 20 * std::log10(rms(y, 4800, y.size() - 4800));
    CHECK(std::fabs(hissDb + 55) < 6, "hiss=%.1f dBFS", hissDb);

    // Wow: pitch of a 1 kHz sine wobbles, peak deviation between 0.1 % and 0.5 %
    v.prepare(sr);
    dsp::Vinyl::Params ww; ww.wow = 1;
    y = runVinyl(v, ww, sine(sr, 1000, 5.0, 0.5f));
    double prevCross = -1, maxDev = 0;
    for (size_t i = (size_t) sr; i < y.size(); ++i)
        if (y[i - 1] < 0 && y[i] >= 0) {
            const double c = (double) (i - 1) + y[i - 1] / (y[i - 1] - y[i]);
            if (prevCross > 0) maxDev = std::max(maxDev, std::fabs(sr / (c - prevCross) - 1000) / 1000);
            prevCross = c;
        }
    CHECK(maxDev > 0.001 && maxDev < 0.005, "wow deviation=%.4f", maxDev);

    // Gate: no crackle/hiss while no key is held; noise dies out after release
    v.prepare(sr, 5);
    dsp::Vinyl::Params gt; gt.crackle = gt.hiss = 1; gt.gate = false;
    y = runVinyl(v, gt, Buf((size_t) sr, 0.0f));
    CHECK(rms(y, 0, y.size()) < 1e-6 && v.crackleCount == 0, "gate closed but noise: rms=%g clicks=%ld", rms(y, 0, y.size()), v.crackleCount);
    gt.gate = true;
    y = runVinyl(v, gt, Buf((size_t) sr, 0.0f));
    CHECK(rms(y, 4800, y.size() - 4800) > 1e-4, "gate open but silent");
    gt.gate = false;
    y = runVinyl(v, gt, Buf((size_t) (2 * sr), 0.0f));
    CHECK(rms(y, (size_t) sr, (size_t) sr) < 1e-5, "noise not released: rms=%g", rms(y, (size_t) sr, (size_t) sr));

    // Age darkens the noise itself (instrument mode: no input, noise only)
    auto hissAt = [&](float age) {
        v.prepare(sr, 9);
        dsp::Vinyl::Params h; h.hiss = 1; h.age = age;
        auto o = runVinyl(v, h, Buf((size_t) sr, 0.0f));
        Buf d(o.size() - 1); for (size_t i = 1; i < o.size(); ++i) d[i - 1] = o[i] - o[i - 1];  // first difference ~ treble
        return rms(d, 4800, d.size() - 4800);
    };
    CHECK(hissAt(1) < 0.7 * hissAt(0), "age does not darken noise: %g vs %g", hissAt(1), hissAt(0));

    // Note follows pitch: one octave up = brighter noise; velocity scales loudness
    auto noiseAt = [&](float pitch, float level, double* rmsOut) {
        v.prepare(sr, 11);
        dsp::Vinyl::Params np; np.hiss = np.crackle = 1; np.pitch = pitch; np.level = level;
        auto o = runVinyl(v, np, Buf((size_t) sr, 0.0f));
        if (rmsOut) *rmsOut = rms(o, 4800, o.size() - 4800);
        Buf d(o.size() - 1); for (size_t i = 1; i < o.size(); ++i) d[i - 1] = o[i] - o[i - 1];
        return rms(d, 4800, d.size() - 4800) / rms(o, 4800, o.size() - 4800);  // treble/total ~ brightness
    };
    const double lowB = noiseAt(0.25f, 1, nullptr), midB = noiseAt(1, 1, nullptr), highB = noiseAt(2, 1, nullptr);
    CHECK(lowB < midB && midB < highB, "pitch does not track: %g %g %g", lowB, midB, highB);
    double loud = 0, soft = 0;
    noiseAt(1, 1, &loud); noiseAt(1, 0.5f, &soft);
    CHECK(std::fabs(20 * std::log10(soft / loud) + 6) < 1.5, "velocity 0.5 not ~-6 dB: %.1f", 20 * std::log10(soft / loud));
    CHECK(finiteAndBounded(runVinyl(v, { 1, 1, 0, 0, true, true, 16.0f, 1 }, Buf((size_t) sr, 0.0f)), 2.0f), "extreme pitch unbounded");

    // Everything maxed on a full-scale input: finite and bounded
    v.prepare(sr, 3);
    dsp::Vinyl::Params mx; mx.crackle = mx.hiss = mx.wow = mx.age = 1;
    CHECK(finiteAndBounded(runVinyl(v, mx, sine(sr, 100, 3.0, 1.0f)), 2.0f), "vinyl max unbounded");
}

static void testToneAndPhaser() {
    const double sr = 48000;
    auto thru = [&](auto& fx, float amt, const Buf& in) {
        Buf out(in.size());
        for (size_t i = 0; i < in.size(); ++i) { float l = in[i], r = in[i]; fx.process(l, r, amt); out[i] = l; }
        return out;
    };
    auto gainDb = [&](float tone, double f) {
        dsp::Tone t; t.prepare(sr, tone);
        auto s = sine(sr, f, 0.5, 0.5f);
        auto y = thru(t, tone, s);
        return 20 * std::log10(rms(y, 4800, 14400) / rms(s, 4800, 14400));
    };
    CHECK(std::fabs(gainDb(1, 1000)) < 0.5 && std::fabs(gainDb(1, 5000)) < 0.5, "tone=1 not transparent: %.2f / %.2f dB", gainDb(1, 1000), gainDb(1, 5000));
    CHECK(gainDb(0, 5000) < -12, "tone=0 5 kHz only %.1f dB", gainDb(0, 5000));
    CHECK(gainDb(0, 200) > -3, "tone=0 cuts lows: %.1f dB", gainDb(0, 200));

    dsp::Phaser ph; ph.prepare(sr, 0.0f);
    auto s = sine(sr, 1000, 4.0, 0.5f);
    auto y = thru(ph, 0.0f, s);
    CHECK(y == s, "phaser=0 not an exact bypass");

    ph.prepare(sr, 1.0f);
    y = thru(ph, 1.0f, s);
    double lo = 1e9, hi = 0;
    for (size_t w = 4800; w + 2400 <= y.size(); w += 2400) { const double v = rms(y, w, 2400); lo = std::min(lo, v); hi = std::max(hi, v); }
    CHECK(20 * std::log10(hi / lo) > 6, "phaser notches do not sweep: %.1f dB", 20 * std::log10(hi / lo));

    dsp::Tone t; t.prepare(sr, 0.0f);
    ph.prepare(sr, 1.0f);
    auto big = sine(sr, 100, 2.0, 1.0f);
    CHECK(finiteAndBounded(thru(ph, 1.0f, thru(t, 0.0f, big)), 2.0f), "tone/phaser unbounded");
}

template <std::size_t N, std::size_t P>
static void checkPresets(const char* plugin, const std::array<presets::ParamSpec, N>& specs, const std::array<presets::Preset<N>, P>& table, std::size_t minCount) {
    CHECK(P >= minCount, "%s: only %zu presets", plugin, P);
    std::set<std::string> names, ids;
    for (const auto& s : specs) { ids.insert(s.id); CHECK(s.min < s.max && s.def >= s.min && s.def <= s.max, "%s: bad spec %s", plugin, s.id); }
    CHECK(ids.size() == N, "%s: duplicate param ids", plugin);
    for (const auto& p : table) {
        CHECK(p.name && *p.name, "%s: empty preset name", plugin);
        names.insert(p.name);
        for (size_t k = 0; k < N; ++k)
            CHECK(p.v[k] >= specs[k].min && p.v[k] <= specs[k].max, "%s/%s: %s=%g outside [%g, %g]", plugin, p.name, specs[k].id, p.v[k], specs[k].min, specs[k].max);
    }
    CHECK(names.size() == P, "%s: duplicate preset names", plugin);
    for (size_t k = 0; k < N; ++k)
        CHECK(table[0].v[k] == specs[k].def, "%s: Init %s=%g != default %g", plugin, specs[k].id, table[0].v[k], specs[k].def);
}

static void testSpace() {
    const double sr = 48000;
    auto noise = [](size_t n, float amp) { dsp::Rng g; Buf x(n); for (auto& v : x) v = amp * g.bipolar(); return x; };
    // Bypass at mix 0
    {
        auto s = noise(48000, 0.5f);
        dsp::Chorus ch; ch.prepare(sr, 0.0f);
        dsp::Reverb rv; rv.prepare(sr, 0.0f, 0.5f);
        bool same = true;
        for (float x : s) { float l = x, r = x; ch.process(l, r, 0.0f); rv.process(l, r, 0.0f, 0.5f); if (l != x || r != x) { same = false; break; } }
        CHECK(same, "chorus/reverb mix 0 not an exact bypass");
    }
    // Reverb RT60 follows size; L/R decorrelated
    auto rt60 = [&](float size, double* corr) {
        dsp::Reverb rv; rv.prepare(sr, 1.0f, size);
        const size_t n = (size_t) (16 * sr);
        Buf L(n), R(n);
        for (size_t i = 0; i < n; ++i) { float l = i == 0 ? 1.0f : 0.0f, r = l; rv.process(l, r, 1.0f, size); L[i] = l; R[i] = r; }
        if (corr) {
            double lr = 0, ll = 0, rr = 0;
            for (size_t i = 0; i < (size_t) sr; ++i) { lr += L[i] * R[i]; ll += L[i] * L[i]; rr += R[i] * R[i]; }
            *corr = lr / std::sqrt(ll * rr);
        }
        const size_t w = 2400;  // 50 ms windows
        double peak = 0; size_t tPeak = 0;
        std::vector<double> e;
        for (size_t i = 0; i + w <= n; i += w) { e.push_back(rms(L, i, w)); if (e.back() > peak) { peak = e.back(); tPeak = e.size() - 1; } }
        double t5 = -1, t25 = -1;
        for (size_t k = tPeak; k < e.size(); ++k) {
            const double db = 20 * std::log10(e[k] / peak + 1e-12);
            if (t5 < 0 && db < -5) t5 = (double) k * 0.05;
            if (t25 < 0 && db < -25) { t25 = (double) k * 0.05; break; }
        }
        return t25 > 0 ? 3.0 * (t25 - t5) : 99.0;
    };
    double corr = 1;
    const double rtLo = rt60(0.0f, &corr), rtHi = rt60(1.0f, nullptr);
    CHECK(rtLo > 0.9 && rtLo < 2.1, "reverb size 0 RT60=%.2f s (want ~1.5)", rtLo);
    CHECK(rtHi > 6.0 && rtHi < 14.0, "reverb size 1 RT60=%.2f s (want ~10)", rtHi);
    CHECK(std::fabs(corr) < 0.5, "reverb L/R correlation %.2f", corr);
    // Stability: loud noise at max size, then silence -> decays, finite, bounded
    {
        dsp::Reverb rv; rv.prepare(sr, 1.0f, 1.0f);
        auto in = noise((size_t) sr, 1.0f);
        Buf out((size_t) (30 * sr));
        for (size_t i = 0; i < out.size(); ++i) { float l = i < in.size() ? in[i] : 0.0f, r = l; rv.process(l, r, 1.0f, 1.0f); out[i] = l; }
        CHECK(finiteAndBounded(out, 2.0f), "reverb unbounded");
        CHECK(rms(out, out.size() - 48000, 48000) < 0.01 * rms(out, 48000, 48000), "reverb does not decay");
    }
    // Chorus: moving comb on a sine -> level wobbles over time, bounded
    {
        dsp::Chorus ch; ch.prepare(sr, 1.0f);
        auto s = sine(sr, 1000, 4.0, 0.5f);
        Buf y(s.size());
        for (size_t i = 0; i < s.size(); ++i) { float l = s[i], r = s[i]; ch.process(l, r, 1.0f); y[i] = l; }
        double lo = 1e9, hi = 0;
        for (size_t w = 4800; w + 2400 <= y.size(); w += 2400) { const double v = rms(y, w, 2400); lo = std::min(lo, v); hi = std::max(hi, v); }
        CHECK(20 * std::log10(hi / lo) > 0.5, "chorus does not modulate: %.2f dB", 20 * std::log10(hi / lo));
        CHECK(finiteAndBounded(y, 1.0f), "chorus unbounded");
    }
}

static Buf renderPad(const dsp::Pad::Params& p, float hz, double seconds, double offAt = -1, Buf* right = nullptr, float vel = 1.0f) {
    const double sr = 48000;
    dsp::Pad v; v.prepare(sr); v.noteOn(hz, vel, 7);
    Buf out((size_t) (seconds * sr));
    if (right) right->assign(out.size(), 0.0f);
    for (size_t i = 0; i < out.size(); ++i) {
        if (offAt >= 0 && i == (size_t) (offAt * sr)) v.noteOff();
        float l, r; v.process(l, r, p); out[i] = l; if (right) (*right)[i] = r;
    }
    return out;
}

static void testPad() {
    const double sr = 48000;
    dsp::Pad::Params base; base.attack = 0.005f; base.motion = 0; base.detune = 0; base.air = 0;
    // Analog saw: fundamental on pitch
    { auto p = base; p.blend = 0; p.shape = 0.5f; p.color = 1;
      auto y = renderPad(p, 220, 1.0);
      const double f = peakFreq(y, 4800, 38400, sr, 220);
      CHECK(std::fabs(f - 220) < 1.1, "analog pitch %.2f Hz", f); }
    // FM: carrier on pitch; sidebands only when Bright > 0
    { auto p = base; p.blend = 1; p.ratio = 1; p.bright = 0;
      auto y = renderPad(p, 220, 1.0);
      CHECK(std::fabs(peakFreq(y, 4800, 38400, sr, 220) - 220) < 1.1, "fm pitch off");
      p.ratio = 3.5f;
      const double side0 = goertzel(renderPad(p, 220, 1.0), 4800, 38400, sr, 220 * 4.5);
      p.bright = 0.5f;
      auto yb = renderPad(p, 220, 1.0);
      const double fund = goertzel(yb, 4800, 38400, sr, 220), side = goertzel(yb, 4800, 38400, sr, 220 * 4.5);
      CHECK(side > 0.01 * fund, "no FM sideband at 4.5f with bright: %g vs %g", side, fund);
      CHECK(side0 < 0.01 * side, "FM sideband without bright: %g vs %g", side0, side); }
    // Envelope: slow attack, release from held level, voice ends
    { auto p = base; p.blend = 1; p.bright = 0; p.ratio = 1; p.attack = 2; p.sustain = 1; p.release = 4;
      auto y = renderPad(p, 220, 8.0, 3.0);
      const double a01 = rms(y, (size_t) (0.08 * sr), 2400), a2 = rms(y, (size_t) (2.5 * sr), 2400), r1 = rms(y, (size_t) (4.0 * sr), 2400);
      CHECK(a01 < 0.2 * a2, "attack too fast: %g vs %g", a01, a2);
      CHECK(r1 > 0.05 * a2 && r1 < 0.5 * a2, "release 1 s after off: %.2f of held", r1 / a2);
      dsp::Pad v; v.prepare(sr); v.noteOn(220, 1); 
      for (int i = 0; i < (int) (0.5 * sr); ++i) { float l, r; v.process(l, r, p); }
      v.noteOff();
      for (int i = 0; i < (int) (8 * sr) && v.isActive(); ++i) { float l, r; v.process(l, r, p); }
      CHECK(!v.isActive(), "voice never ends after release"); }
    // Struck sound: sustain 0 decays away
    { auto p = base; p.blend = 1; p.bright = 0.4f; p.sustain = 0; p.decay = 0.4f;
      auto y = renderPad(p, 440, 3.0);
      CHECK(rms(y, (size_t) (2 * sr), 2400) < 0.05 * rms(y, 240, 2400), "struck note does not decay"); }
    // No click on note-off during attack (pure sine: max step << 0.05)
    { auto p = base; p.blend = 1; p.bright = 0; p.ratio = 1; p.attack = 3; p.release = 0.05f;
      auto y = renderPad(p, 220, 1.0, 0.3);
      CHECK(maxStep(y, 1, y.size() - 1) < 0.05, "click: step %g", maxStep(y, 1, y.size() - 1)); }
    // Anti-aliasing: saw at 4.1 kHz, folded 11th harmonic (2.9 kHz) well below the fundamental
    { auto p = base; p.blend = 0; p.shape = 0.5f; p.color = 1;
      auto y = renderPad(p, 4100, 1.0);
      const double fund = goertzel(y, 4800, 38400, sr, 4100), alias = goertzel(y, 4800, 38400, sr, 2900);
      CHECK(20 * std::log10(alias / fund) < -30, "aliasing %.1f dB", 20 * std::log10(alias / fund)); }
    // Breath adds noise
    { auto p = base; p.blend = 1; p.bright = 0; p.ratio = 1;
      auto a = renderPad(p, 220, 0.5); p.air = 1; auto b = renderPad(p, 220, 0.5);
      double d = 0; for (size_t i = 0; i < a.size(); ++i) d += (a[i] - b[i]) * (a[i] - b[i]);
      CHECK(std::sqrt(d / (double) a.size()) > 1e-3, "air adds nothing"); }
    // Extremes: finite and bounded
    for (float hz : { 27.5f, 4186.0f })
        for (float e : { 0.0f, 1.0f }) {
            dsp::Pad::Params p; p.blend = e; p.shape = e; p.detune = e; p.color = e; p.ratio = e ? 14.0f : 0.5f; p.bright = e; p.air = e; p.motion = e; p.attack = 0.005f; p.sustain = e;
            CHECK(finiteAndBounded(renderPad(p, hz, 0.5), 1.0f), "pad unbounded at %.0f Hz, extreme %.0f", hz, e);
        }
}

static void testPresets() {
    checkPresets("rhodes", rhodes::kParams, rhodes::kPresets, 10);
    checkPresets("kankyo", kankyo::kParams, kankyo::kPresets, 30);

    // Every Kankyo preset, full chain (4-note chord, chorus, reverb, volume): no clipping, homogeneous level
    const double sr = 48000;
    for (const auto& pr : kankyo::kPresets) {
        const auto& v = pr.v;
        const dsp::Pad::Params p { v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11] };
        std::array<dsp::Pad, 4> voices;
        const float notes[4] = { 130.81f, 164.81f, 196.0f, 246.94f };  // Cmaj7, C3
        for (size_t k = 0; k < 4; ++k) { voices[k].prepare(sr); voices[k].noteOn(notes[k], 0.8f, (uint32_t) k); }
        dsp::Chorus ch; ch.prepare(sr, v[12]);
        dsp::Reverb rv; rv.prepare(sr, v[13], v[14]);
        const float g = std::pow(10.0f, v[15] / 20.0f) * kankyo::kOutputMakeup;
        Buf out((size_t) (8 * sr));
        for (size_t i = 0; i < out.size(); ++i) {
            if (i == (size_t) (6 * sr)) for (auto& vo : voices) vo.noteOff();
            float l = 0, r = 0;
            for (auto& vo : voices) { float a, b; vo.process(a, b, p); l += a; r += b; }
            ch.process(l, r, v[12]); rv.process(l, r, v[13], v[14]);
            out[i] = l * g;
        }
        double peak = 0; for (float x : out) peak = std::max(peak, (double) std::fabs(x));
        double loud = 0;  // loudest 0.5 s window
        for (size_t w = 0; w + 24000 <= out.size(); w += 12000) loud = std::max(loud, rms(out, w, 24000));
        const double db = 20 * std::log10(loud + 1e-12);
        CHECK(peak <= 1.0 && db > -22 && db < -10, "kankyo preset %-13s peak %.2f  loudest %.1f dBFS", pr.name, peak, db);
    }
}

// ---------- Glitch: clock, pads, engage, history ----------

static void testGlitchBasics() {
    const double sr = 48000;
    {   // Grid: 120 BPM, ppq 0 -> next 1/16 boundary at 6000 samples; all 1/32 boundaries in a block found
        dsp::BeatClock c; c.prepare(sr); c.begin(4096, true, 120, true, 0.0, true);
        int first = -1, n32 = 0;
        for (int i = 1; i < 4096; ++i) {
            if (std::floor(c.ppqAt(i) / 0.125) > std::floor(c.ppqAt(i - 1) / 0.125)) ++n32;
        }
        c.begin(8192, true, 120, true, 0.0, true);
        for (int i = 1; i < 8192 && first < 0; ++i) if (std::floor(c.ppqAt(i) / 0.25) > std::floor(c.ppqAt(i - 1) / 0.25)) first = i;
        CHECK(first == 6000, "1/16 boundary at %d", first);
        CHECK(n32 == 1, "1/32 boundaries in 4096 samples = %d", n32);  // 1/32 = 3000 samples
    }
    {   // Stopped transport: free-runs at the last BPM, 120 if none; resync and loop detection
        dsp::BeatClock c; c.prepare(sr);
        c.begin(480, false, 0, false, 0, false);
        CHECK(std::fabs(c.tempo() - 120) < 1e-9, "default bpm %.1f", c.tempo());
        c.begin(480, true, 140, false, 0, false);
        const double a = c.ppqAt(0);
        c.begin(480, true, 140, false, 0, false);
        CHECK(std::fabs((c.ppqAt(0) - a) - 480 * 140 / (60 * sr)) < 1e-9, "free-run at 140 BPM");
        c.begin(512, true, 140, true, 8.0, true);
        CHECK(std::fabs(c.ppqAt(0) - 8.0) < 1e-12, "resync on play");
        c.begin(512, true, 140, true, 8.0 + 512 * 140 / (60 * sr), true);
        CHECK(!c.jumpedThisBlock(), "steady playback flagged as jump");
        c.begin(512, true, 141, true, 8.0 + 2 * 512 * 140 / (60 * sr), true);
        CHECK(!c.jumpedThisBlock(), "tempo change flagged as jump");
        c.begin(512, true, 141, true, 0.0, true);
        CHECK(c.jumpedThisBlock(), "loop back to 0 not detected");
        CHECK(std::fabs(c.lengthSamples(1.0) - 20425.5) < 1, "length at 141 BPM %.1f", c.lengthSamples(1.0));
        c.begin(512, true, 20, true, 0.0, true);
        CHECK(std::fabs(c.lengthSamples(1.0) - sr) < 1e-6, "length floored at 60 BPM");
    }
    {   // Pads state machine
        dsp::Pads p; p.reset();
        p.setParam(0, true); p.update(); CHECK(p.active(0), "pressed not active");
        p.setParam(0, false); p.update(); CHECK(!p.active(0), "released still active");
        p.setParam(1, true); p.update(); p.setParam(11, true); p.update(); p.setParam(1, false); p.update();
        CHECK(p.active(1), "pad pressed when Hold turned on did not latch");
        p.setParam(2, true); p.update(); p.setParam(2, false); p.update();
        CHECK(p.active(2), "press under Hold did not latch");
        p.setParam(2, true); p.update(); p.setParam(2, false); p.update();
        CHECK(!p.active(2), "second press under Hold did not unlatch");
        p.setParam(3, true); p.update();
        p.setParam(11, false); p.update();
        CHECK(!p.active(1) && p.active(3), "Hold off: latches must drop, held pads stay");
        p.setParam(3, false); p.update(); CHECK(!p.active(3), "held pad stuck");
        // Notes 36..47, out of range ignored, param + note combined
        p.noteOn(36); p.update(); CHECK(p.active(0) && p.mask() == 1u, "note 36 -> crush, mask %u", p.mask());
        p.noteOn(35); p.noteOn(48); p.update(); CHECK(p.mask() == 1u, "out of range notes changed mask");
        p.setParam(0, true); p.noteOff(36); p.update(); CHECK(p.active(0), "param still held");
        p.setParam(0, false); p.update(); CHECK(!p.active(0), "nothing held");
        p.noteOn(47); p.update(); p.setParam(11, true); p.update(); p.noteOff(47); p.update();
        CHECK(p.holdOn(), "Hold toggled on screen + note: must stay on while either is on");
        p.setParam(11, false); p.update(); CHECK(!p.holdOn() && p.mask() == 0u, "hold stuck");
    }
    {   // Engage: 0 -> 1 in 8 ms, monotone; at 0 the blend is the dry signal bit-exact
        dsp::Engage e; e.prepare(sr);
        int n = 0; float last = 0; bool mono = true;
        while (e.next(true) < 1.0f) { ++n; mono = mono && e.value >= last; last = e.value; }
        CHECK(std::abs(n + 1 - 384) <= 1 && mono, "engage ramp %d samples", n + 1);
        CHECK(dsp::blend(0.123456f, 0.9f, 0.0f) == 0.123456f, "blend 0 not dry");
    }
    {   // History: integer delays exact, fractional reads between neighbours
        dsp::History h; h.prepare(sr, 0.1);
        for (int i = 0; i < 1000; ++i) h.push((float) i, (float) -i);
        CHECK(h.read(0, 1) == 999.0f && h.read(1, 10) == -990.0f, "integer delays not exact");
        CHECK(std::fabs(h.read(0, 10.5) - 989.5f) < 1e-3f && std::fabs(h.read(0, 1.5) - 998.5f) < 1e-3f, "fractional read on a ramp");
        CHECK(h.at(0, 500) == 500.0f, "absolute index");
    }
}

// ---------- Glitch: time effects ----------

// Runs `fx(l, r, clock, ppq, i)` over a mono input (L = R), feeding the clock every 512 samples.
template <class F>
static Buf runClocked(double sr, double bpm, bool playing, const Buf& in, F fx, Buf* right = nullptr) {
    dsp::BeatClock c; c.prepare(sr);
    Buf out(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        const size_t k = i % 512;
        if (k == 0) c.begin(512, true, bpm, playing, (double) i * bpm / (60.0 * sr), playing);
        float l = in[i], r = in[i];
        fx(l, r, c, c.ppqAt((int) k), i);
        out[i] = l;
        if (right) (*right)[i] = r;
    }
    return out;
}
static Buf ramp(size_t n) { Buf x(n); for (size_t i = 0; i < n; ++i) x[i] = (float) i * 1e-6f; return x; }
static Buf noise(size_t n, uint32_t seed) { dsp::Rng g; g.s = seed; Buf x(n); for (auto& v : x) v = g.bipolar(); return x; }
static int zeroCrossings(const Buf& x, size_t from, size_t n) {
    int z = 0; for (size_t i = from + 1; i < from + n; ++i) if ((x[i - 1] < 0) != (x[i] < 0)) ++z;
    return z;
}

static void testGlitchTime() {
    const double sr = 48000;
    {   // Stutter 1/16 at 120 BPM (6000 samples), pressed at 10000: slice [6000, 12000) repeats
        for (bool playing : { true, false }) {
            dsp::Stutter st; st.prepare(sr);
            const Buf in = ramp(40000);
            const Buf out = runClocked(sr, 120, playing, in, [&](float& l, float& r, auto& c, double ppq, size_t i) { st.process(l, r, i >= 10000, 0.25, c, ppq); });
            CHECK(out[11000] == in[11000], "first pass is not live (%s)", playing ? "playing" : "stopped");
            CHECK(std::fabs(out[12500] - in[6500]) < 1e-7f && std::fabs(out[18500] - in[6500]) < 1e-7f, "slice not repeated from the last boundary (%s)", playing ? "playing" : "stopped");
            bool periodic = true;
            for (size_t i = 12200; i < 17000; ++i) periodic = periodic && out[i + 6000] == out[i];
            CHECK(periodic, "stutter period not 6000 samples (%s)", playing ? "playing" : "stopped");
        }
        dsp::Stutter st; st.prepare(sr);
        const Buf in = sine(sr, 440, 1.5, 0.5f);
        const Buf out = runClocked(sr, 120, true, in, [&](float& l, float& r, auto& c, double ppq, size_t i) { st.process(l, r, i >= 10000 && i < 60000, 0.25, c, ppq); });
        CHECK(maxStep(out, 0, out.size()) < 0.05, "stutter seam step %.3f", maxStep(out, 0, out.size()));
        CHECK(out[70000] == in[70000], "stutter not bypassed after release");
    }
    {   // Reverse 1/4 beat on a rising ramp: falling output; seams smooth on a sine
        dsp::Reverse rv; rv.prepare(sr);
        const Buf in = ramp(60000);
        const Buf out = runClocked(sr, 120, true, in, [&](float& l, float& r, auto& c, double, size_t i) { rv.process(l, r, i >= 20000, 0.25, c); });
        bool falling = true;
        for (size_t i = 20400; i < 26000; ++i) falling = falling && out[i] < out[i - 1];
        CHECK(falling, "reverse output not descending");
        dsp::Reverse rs; rs.prepare(sr);
        const Buf sn = sine(sr, 440, 1.5, 0.5f);
        const Buf so = runClocked(sr, 120, true, sn, [&](float& l, float& r, auto& c, double, size_t i) { rs.process(l, r, i >= 20000 && i < 60000, 0.25, c); });
        CHECK(maxStep(so, 0, so.size()) < 0.05, "reverse seam step %.3f", maxStep(so, 0, so.size()));
    }
    {   // Tape stop 1 beat: pitch falls monotonically, then silence; release returns to live without a jump
        dsp::TapeStop ts; ts.prepare(sr);
        const size_t t0 = 20000, rel = 60000;
        const Buf in = sine(sr, 1000, 2.0, 0.5f);
        const Buf out = runClocked(sr, 120, true, in, [&](float& l, float& r, auto& c, double, size_t i) { ts.process(l, r, i >= t0 && i < rel, 1.0, c); });
        int z[4]; for (int w = 0; w < 4; ++w) z[w] = zeroCrossings(out, t0 + 400 + (size_t) w * 5000, 5000);
        CHECK(z[0] > z[1] && z[1] > z[2] && z[2] > z[3], "tape stop pitch not falling %d %d %d %d", z[0], z[1], z[2], z[3]);
        CHECK(rms(out, t0 + 25000, 10000) < 1e-3, "not silent once stopped (%.4f)", rms(out, t0 + 25000, 10000));
        CHECK(maxStep(out, rel - 1000, 20000) <= 1.25 * maxStep(in, 0, 2000), "tape start step %.3f", maxStep(out, rel - 1000, 20000));  // 1 kHz sine: natural step 0.065
        bool exact = true; for (size_t i = rel + 3000 + 400; i < out.size(); ++i) exact = exact && out[i] == in[i];
        CHECK(exact, "tape stop not bypassed after tape start");
    }
    {   // Held 20 s (longer than the buffer), then released: no jump, back to the live input
        dsp::TapeStop ts; ts.prepare(sr);
        const size_t rel = (size_t) (21 * sr);
        const Buf in = sine(sr, 300, 22.0, 0.5f);
        const Buf out = runClocked(sr, 120, true, in, [&](float& l, float& r, auto& c, double, size_t i) { ts.process(l, r, i >= (size_t) sr && i < rel, 1.0, c); });
        CHECK(maxStep(out, rel - 100, 8000) < 0.05, "long hold release step %.3f", maxStep(out, rel - 100, 8000));
        bool exact = true; for (size_t i = rel + 3500; i < out.size(); ++i) exact = exact && out[i] == in[i];
        CHECK(exact, "long hold: not back to live input");
    }
    {   // 20 BPM: lengths computed at 60 BPM, loops stay inside the buffer
        dsp::Stutter st; st.prepare(sr);
        const Buf in = ramp((size_t) (6 * sr));
        const Buf out = runClocked(sr, 20, true, in, [&](float& l, float& r, auto& c, double ppq, size_t i) { st.process(l, r, i >= 1000, 2.0, c, ppq); });
        bool periodic = true;  // 1/2 = 2 beats = 96000 samples at the 60 BPM floor
        for (size_t i = 100000; i < 150000; ++i) periodic = periodic && out[i + 96000] == out[i];
        CHECK(periodic, "stutter 1/2 at 20 BPM not looping on 96000 samples");
        dsp::Reverse rv; rv.prepare(sr);
        const Buf nz = noise((size_t) (20 * sr), 7);
        const Buf ro = runClocked(sr, 20, true, nz, [&](float& l, float& r, auto& c, double, size_t i) { rv.process(l, r, i >= 1000, 4.0, c); });
        CHECK(finiteAndBounded(ro, 1.01f), "reverse 1 bar at 20 BPM unbounded");
    }
    {   // Downer: depth 0 = bit-exact even when engaged; depth 1 = lower pitch late in each step
        dsp::Downer dw; dw.prepare(sr);
        const Buf in = sine(sr, 1000, 1.0, 0.5f);
        Buf out = runClocked(sr, 120, true, in, [&](float& l, float& r, auto& c, double ppq, size_t) { dw.process(l, r, true, 1.0, 0.0f, c, ppq); });
        CHECK(out == in, "downer depth 0 not bit-exact");
        dsp::Downer d1; d1.prepare(sr);
        out = runClocked(sr, 120, true, in, [&](float& l, float& r, auto& c, double ppq, size_t) { d1.process(l, r, true, 1.0, 1.0f, c, ppq); });
        const int live = zeroCrossings(in, 24000 + 12000, 6000), dipped = zeroCrossings(out, 24000 + 12000, 6000);
        CHECK(dipped < live * 0.8, "downer not pitching down (%d vs %d)", dipped, live);
    }
    {   // All four on loud noise with extreme settings, pads toggled: finite and bounded
        const Buf in = noise((size_t) (10 * sr), 3);
        dsp::Stutter a; a.prepare(sr); dsp::Reverse b; b.prepare(sr); dsp::TapeStop c2; c2.prepare(sr); dsp::Downer d; d.prepare(sr);
        const Buf out = runClocked(sr, 174, true, in, [&](float& l, float& r, auto& c, double ppq, size_t i) {
            const bool on = (i / 33600) % 2 == 0;
            a.process(l, r, on, 0.125, c, ppq); b.process(l, r, !on, 4.0, c); c2.process(l, r, on, 0.125, c); d.process(l, r, true, 0.25, 1.0f, c, ppq);
        });
        CHECK(finiteAndBounded(out, 1.5f), "time effects unbounded on noise");  // Hermite may overshoot a little on white noise
    }
}

// ---------- Glitch: colour effects ----------

template <class F>
static Buf runPlain(const Buf& in, F fx) { Buf out(in.size()); for (size_t i = 0; i < in.size(); ++i) { float l = in[i], r = in[i]; fx(l, r, i); out[i] = l; } return out; }

static void testGlitchColor() {
    const double sr = 48000;
    {   // Pitch: +12 -> 880 Hz, -12 -> 220 Hz, 0 -> bit-exact with the pad engaged
        const Buf in = sine(sr, 440, 1.0, 0.5f);
        for (auto [semis, want] : { std::pair { 12.0f, 880.0 }, std::pair { -12.0f, 220.0 } }) {
            dsp::PitchShift p; p.prepare(sr);
            const Buf out = runPlain(in, [&](float& l, float& r, size_t) { p.process(l, r, true, semis); });
            const double f = peakFreq(out, (size_t) (0.2 * sr), (size_t) (0.6 * sr), sr, want);
            CHECK(std::fabs(f - want) < want * 0.02, "pitch %+.0f st -> %.1f Hz", semis, f);
        }
        dsp::PitchShift p; p.prepare(sr);
        const Buf out = runPlain(in, [&](float& l, float& r, size_t) { p.process(l, r, true, 0.0f); });
        CHECK(out == in, "pitch 0 not bit-exact");
    }
    {   // Crush: 4 bits -> at most 16 levels per polarity; 4 kHz -> steps of 12 samples at 48 kHz
        dsp::Crush c; c.prepare(sr);
        const Buf in = sine(sr, 97, 0.5, 0.9f);
        const Buf out = runPlain(in, [&](float& l, float& r, size_t) { c.process(l, r, true, 4.0f, 48.0f); });
        std::set<float> pos, neg;
        for (size_t i = 1000; i < out.size(); ++i) (out[i] >= 0 ? pos : neg).insert(out[i]);
        CHECK(pos.size() <= 16 && neg.size() <= 16, "crush levels %zu / %zu", pos.size(), neg.size());
        dsp::Crush d; d.prepare(sr);
        const Buf held = runPlain(in, [&](float& l, float& r, size_t) { d.process(l, r, true, 16.0f, 4.0f); });
        int changes = 0; for (size_t i = 1001; i < 1001 + 1200; ++i) if (held[i] != held[i - 1]) ++changes;
        CHECK(changes >= 99 && changes <= 101, "crush 4 kHz: %d value changes in 1200 samples", changes);
    }
    {   // Filter: -100 % low-pass, +100 % high-pass, 0 = bit-exact
        const Buf lo = sine(sr, 100, 1.0, 0.5f), hi = sine(sr, 5000, 1.0, 0.5f);
        auto through = [&](const Buf& in, float freq) { dsp::DjFilter f; f.prepare(sr); return runPlain(in, [&](float& l, float& r, size_t) { f.process(l, r, true, freq); }); };
        auto db = [&](const Buf& out, const Buf& in) { return 20 * std::log10(rms(out, 24000, 24000) / rms(in, 24000, 24000)); };
        CHECK(db(through(hi, -1.0f), hi) < -20 && db(through(lo, -1.0f), lo) > -3, "low-pass: 5 kHz %.1f dB, 100 Hz %.1f dB", db(through(hi, -1.0f), hi), db(through(lo, -1.0f), lo));
        CHECK(db(through(lo, 1.0f), lo) < -20 && db(through(hi, 1.0f), hi) > -3, "high-pass: 100 Hz %.1f dB, 5 kHz %.1f dB", db(through(lo, 1.0f), lo), db(through(hi, 1.0f), hi));
        CHECK(through(hi, 0.0f) == hi, "filter at 0 not bit-exact");
    }
    {   // Gate 1/16, depth 100 %: closed half-steps silent (within the ramps), no clicks
        dsp::Gate g; g.prepare(sr);
        const Buf in = sine(sr, 220, 1.0, 0.5f);
        const Buf out = runClocked(sr, 120, true, in, [&](float& l, float& r, auto&, double ppq, size_t) { g.process(l, r, true, 0.25, 1.0f, ppq); });
        const double open = rms(out, 6000 + 1500, 1000), closed = rms(out, 6000 + 3000 + 2000, 1000);  // a 1/16 = 6000 samples, closed from 3000
        CHECK(closed < 0.02 * open, "gate closed %.4f vs open %.4f", closed, open);
        CHECK(maxStep(out, 0, out.size()) <= 1.25 * maxStep(in, 0, 2000), "gate click %.3f", maxStep(out, 0, out.size()));
    }
    {   // Phaser: old call = new call at 0.5 Hz; 4 Hz speed -> envelope periodic over 0.25 s
        dsp::Phaser a, b; a.prepare(sr, 0.7f); b.prepare(sr, 0.7f);
        const Buf in = noise((size_t) sr, 11);
        bool same = true;
        for (float x : in) { float l1 = x, r1 = x, l2 = x, r2 = x; a.process(l1, r1, 0.7f); b.process(l2, r2, 0.7f, 0.5f); same = same && l1 == l2 && r1 == r2; }
        CHECK(same, "phaser overload changed the existing behaviour");
        dsp::GlitchPhaser gp; gp.prepare(sr);
        const Buf saw = [&] { Buf x((size_t) (3 * sr)); for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float) (std::fmod((double) i * 110 / sr, 1.0) * 2 - 1); return x; }();
        const Buf out = runPlain(saw, [&](float& l, float& r, size_t) { gp.process(l, r, true, 4.0f); });
        std::vector<double> env; for (size_t w = 4800; w + 480 <= out.size(); w += 480) env.push_back(rms(out, w, 480));  // 10 ms windows
        auto corr = [&](size_t lag) { double m = 0; for (double v : env) m += v; m /= (double) env.size(); double num = 0, den = 0;
            for (size_t i = 0; i + lag < env.size(); ++i) num += (env[i] - m) * (env[i + lag] - m);
            for (double v : env) den += (v - m) * (v - m); return num / den; };
        CHECK(corr(25) > corr(12) + 0.2, "phaser 4 Hz: corr(0.25 s) %.2f vs corr(0.12 s) %.2f", corr(25), corr(12));
    }
}

int main() {
    testTine();
    testEffects();
    testVinyl();
    testToneAndPhaser();
    testSpace();
    testPad();
    testPresets();
    testGlitchBasics();
    testGlitchTime();
    testGlitchColor();
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("all DSP tests passed\n");
    return 0;
}
