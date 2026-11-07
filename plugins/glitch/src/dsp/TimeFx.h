#pragma once
// Time effects, pure C++: they replay the recent past from their own history buffer.
// Each one keeps recording while disengaged and returns its input bit-exact when idle.
#include "dsp/BeatClock.h"
#include "dsp/Engage.h"
#include <cmath>
#include <cstdint>

namespace dsp {

constexpr double kTimeFxSeconds = 8.5;  // 2 bars at 60 BPM (lengths are computed with tempo floored at 60)

// Beat repeat. On press the loop starts at the last grid boundary already past: the first pass
// is the live signal, then the slice repeats. 3 ms crossfade at each seam. A new division set
// while held takes effect at the next seam.
class Stutter {
public:
    void prepare(double sr) { hist.prepare(sr, kTimeFxSeconds); eng.prepare(sr); xf = (int) (0.003 * sr); looping = false; }
    bool idle() const { return !looping; }

    void process(float& l, float& r, bool on, double qn, const BeatClock& clk, double ppq) {
        hist.push(l, r);
        const int64_t now = hist.written() - 1;
        const auto want = std::max<int64_t>(xf * 2, (int64_t) std::llround(clk.lengthSamples(qn)));
        if (on && !looping) {
            len = pending = want;
            pos = std::min<int64_t>(len - 1, (int64_t) (BeatClock::phase(ppq, qn) * (double) len));
            start = now - pos;
            pass = 0;
            looping = true;
        } else if (on) {
            pending = want;
        }
        const float e = eng.next(on);
        if (!looping) return;

        float wl = hist.at(0, start + pos), wr = hist.at(1, start + pos);
        if (const int64_t cont = start + len + pos; pass > 0 && pos < xf && cont <= now) {  // seam: fade from the slice's natural continuation
            const float a = (float) pos / (float) xf;
            wl = blend(hist.at(0, cont), wl, a);
            wr = blend(hist.at(1, cont), wr, a);
        }
        l = blend(l, wl, e);
        r = blend(r, wr, e);
        if (++pos >= len) { pos = 0; ++pass; len = pending; }
        if (e <= 0.0f) looping = false;
    }

private:
    History hist;
    Engage eng;
    int64_t start = 0, len = 1, pending = 1, pos = 0, pass = 0;
    int xf = 1;
    bool looping = false;
};

// Plays each segment that just went by backwards, segment after segment, 8 ms crossfades.
class Reverse {
public:
    void prepare(double sr) { hist.prepare(sr, kTimeFxSeconds); eng.prepare(sr); xf = (int) (0.008 * sr); playing = false; }
    bool idle() const { return !playing; }

    void process(float& l, float& r, bool on, double qn, const BeatClock& clk) {
        hist.push(l, r);
        const int64_t now = hist.written() - 1;
        const int64_t maxLen = (hist.capacity() - 2 * xf - 8) / 2;  // reads reach back 2 x the segment
        const int64_t want = std::clamp<int64_t>((int64_t) std::llround(clk.lengthSamples(qn)), 2 * xf, maxLen);
        if (on && !playing) { len = prevLen = want; segEnd = now + 1; pos = 0; seg = 0; playing = true; }
        const float e = eng.next(on);
        if (!playing) return;

        const int64_t idx = segEnd - 1 - pos;
        float wl = hist.at(0, idx), wr = hist.at(1, idx);
        if (seg > 0 && pos < xf) {  // fade out of where the previous segment would have gone next
            const int64_t prev = segEnd - 1 - prevLen - pos - prevLen;
            const float a = (float) pos / (float) xf;
            wl = blend(hist.at(0, prev), wl, a);
            wr = blend(hist.at(1, prev), wr, a);
        }
        l = blend(l, wl, e);
        r = blend(r, wr, e);
        if (++pos >= len) { segEnd += len; prevLen = len; len = want; pos = 0; ++seg; }
        if (e <= 0.0f) playing = false;
    }

private:
    History hist;
    Engage eng;
    int64_t len = 1, prevLen = 1, segEnd = 0, pos = 0, seg = 0;
    int xf = 1;
    bool playing = false;
};

// Turntable stop: speed 1 -> 0 over the chosen time (quadratic), then parked silence for as
// long as the pad is held. Release: tape start from (almost) now, speed 0 -> 1 over 1/8 beat,
// then the engage ramp crossfades back to the live signal.
class TapeStop {
public:
    enum class State { Idle, Stopping, Parked, Starting };

    void prepare(double sr) { hist.prepare(sr, kTimeFxSeconds); eng.prepare(sr); state = State::Idle; }
    bool idle() const { return state == State::Idle && eng.idle(); }
    State current() const { return state; }

    void process(float& l, float& r, bool on, double qn, const BeatClock& clk) {
        hist.push(l, r);
        if (on && (state == State::Idle || state == State::Starting)) {
            if (state == State::Idle) delay = 1.0;
            state = State::Stopping; t = 0; dur = std::max(1.0, clk.lengthSamples(qn));
        } else if (!on && (state == State::Stopping || state == State::Parked)) {
            state = State::Starting; t = 0; dur = std::max(1.0, clk.lengthSamples(0.125)); delay = 1.0;
        }
        const float e = eng.next(state != State::Idle);
        if (e <= 0.0f && state == State::Idle) return;

        float v = 1.0f;
        switch (state) {
            case State::Stopping: { const double x = 1.0 - t / dur; v = (float) (x * x); if (++t >= dur) state = State::Parked; break; }
            case State::Parked: v = 0.0f; break;
            case State::Starting: v = (float) std::min(1.0, t / dur); if (++t >= dur) state = State::Idle; break;
            case State::Idle: break;
        }
        delay = std::min(delay + (1.0 - v), (double) hist.capacity());
        const float g = std::min(1.0f, v * 8.0f);  // fade the last crawl to silence (and back in)
        l = blend(l, g * hist.read(0, delay), e);
        r = blend(r, g * hist.read(1, delay), e);
    }

private:
    History hist;
    Engage eng;
    State state = State::Idle;
    double delay = 1.0, t = 0.0, dur = 1.0;
};

// SP-404 style "Downer": within each grid step of `qn` the read head slows down linearly
// (speed 1 -> 1 - depth), then snaps back to live with an 8 ms crossfade at the next step.
// Stateless apart from the engage ramp: the head position is a function of the grid phase.
class Downer {
public:
    void prepare(double sr) { hist.prepare(sr, kTimeFxSeconds); eng.prepare(sr); xf = 0.008 * sr; }
    bool idle() const { return eng.idle(); }

    void process(float& l, float& r, bool on, double qn, float depth, const BeatClock& clk, double ppq) {
        hist.push(l, r);
        const float e = eng.next(on);
        if (e <= 0.0f) return;
        const double period = std::max(1.0, clk.lengthSamples(qn)), t = BeatClock::phase(ppq, qn) * period;
        auto delayAt = [&](double x) { return 1.0 + depth * x * x / (2.0 * period); };
        float wl = hist.read(0, delayAt(t)), wr = hist.read(1, delayAt(t));
        if (t < xf) {  // previous step's head, continued, fades out
            const float a = (float) (t / xf);
            wl = blend(hist.read(0, delayAt(period + t)), wl, a);
            wr = blend(hist.read(1, delayAt(period + t)), wr, a);
        }
        l = blend(l, wl, e);
        r = blend(r, wr, e);
    }

private:
    History hist;
    Engage eng;
    double xf = 1.0;
};

}  // namespace dsp
