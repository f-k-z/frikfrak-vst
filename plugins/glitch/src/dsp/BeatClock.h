#pragma once
// Host tempo clock, pure C++. Fed once per block with what the host play head reports;
// gives the musical position (in quarter notes, "ppq") of every sample of the block.
// When the transport is stopped or the position is missing it free-runs at the last known
// tempo (120 BPM if none was ever received), so tempo-synced effects keep a steady grid.
#include <algorithm>
#include <cmath>

namespace dsp {

class BeatClock {
public:
    static constexpr double kDefaultBpm = 120.0, kMinLengthBpm = 60.0;
    static constexpr double kJumpQn = 0.25 * 0.125;  // 1/4 of a 1/32 note: tempo automation never counts as a jump

    void prepare(double sampleRate) { sr = sampleRate; bpm = kDefaultBpm; started = false; jumped = false; start = 0.0; prevStart = 0.0; prevN = 0; }

    // hasBpm/hasPpq: whether the host provided them for this block.
    void begin(int numSamples, bool hasBpm, double hostBpm, bool hasPpq, double hostPpq, bool playing) {
        if (hasBpm && hostBpm > 0.0) bpm = hostBpm;
        qnPerSample = bpm / (60.0 * sr);
        const double expected = started ? prevStart + qnPerSample * prevN : 0.0;  // predicted with this block's tempo
        if (playing && hasPpq) {
            jumped = started && std::fabs(hostPpq - expected) > kJumpQn;
            start = hostPpq;
        } else {
            jumped = false;
            start = expected;
        }
        started = true;
        prevStart = start;
        prevN = numSamples;
    }

    double ppqAt(int i) const { return start + qnPerSample * i; }
    double tempo() const { return bpm; }
    bool jumpedThisBlock() const { return jumped; }

    // Length in samples of a duration in quarter notes, tempo floored at 60 BPM so lengths
    // always fit the effect buffers (Live goes down to 20 BPM).
    double lengthSamples(double qn) const { return qn * 60.0 * sr / std::max(bpm, kMinLengthBpm); }

    // Position inside the current step of a grid of `qn` quarter notes, 0..1.
    static double phase(double ppq, double qn) { const double x = ppq / qn; return x - std::floor(x); }

private:
    double sr = 48000.0, bpm = kDefaultBpm, qnPerSample = 0.0, start = 0.0, prevStart = 0.0;
    int prevN = 0;
    bool started = false, jumped = false;
};

}  // namespace dsp
