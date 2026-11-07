// Property tests for the pure-C++ DSP. CHECK (not assert) so they also run under NDEBUG.
#include "dsp/Effects.h"
#include "dsp/RhodesTine.h"
#include "dsp/Vinyl.h"
#include "dsp/Space.h"
#include "dsp/Pad.h"
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

int main() {
    testTine();
    testEffects();
    testVinyl();
    testToneAndPhaser();
    testSpace();
    testPad();
    testPresets();
    if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
    std::printf("all DSP tests passed\n");
    return 0;
}
