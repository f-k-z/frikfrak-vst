#pragma once
// Building blocks shared by the Glitch effects, pure C++: engage ramp, stereo history buffer
// with Hermite reads. Buffers are allocated in prepare(), never in process().
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dsp {

// Linear engage ramp (8 ms by default). value 0 = effect fully out, so callers can bypass exactly.
struct Engage {
    float value = 0.0f, step = 1.0f;
    void prepare(double sr, float timeSec = 0.008f) { step = 1.0f / (timeSec * (float) sr); value = 0.0f; }
    float next(bool on) { value = on ? std::min(1.0f, value + step) : std::max(0.0f, value - step); return value; }
    bool idle() const { return value <= 0.0f; }
};

// Linear blend a -> b: t <= 0 returns a and t >= 1 returns b bit-exact (also used for crossfades,
// so equal inputs come out unchanged).
inline float blend(float a, float b, float t) { return t <= 0.0f ? a : (t >= 1.0f ? b : a + t * (b - a)); }

// 4-point, 3rd-order Hermite interpolation (Niemitalo); t = 0 returns y0 exactly.
inline float hermite(float ym1, float y0, float y1, float y2, float t) {
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * t + c2) * t + c1) * t + y0;
}

// Stereo circular history. push() then read(c, d): d = 1 is the sample just pushed.
// Integer delays are exact; fractional delays use Hermite from d >= 3 (needs two newer
// samples), linear below. Absolute sample index = written(), so effects can anchor loops.
class History {
public:
    void prepare(double sr, double seconds) {
        size = 1;
        while (size < (int64_t) (seconds * sr) + 8) size <<= 1;
        for (auto& b : buf) b.assign((size_t) size, 0.0f);
        mask = size - 1;
        count = 0;
    }
    void push(float l, float r) {
        buf[0][(size_t) (count & mask)] = l;
        buf[1][(size_t) (count & mask)] = r;
        ++count;
    }
    int64_t written() const { return count; }
    int64_t capacity() const { return size - 8; }  // largest safe delay

    float at(int c, int64_t abs) const { return buf[c][(size_t) (abs & mask)]; }  // absolute index, must be < written()

    float read(int c, double delay) const {
        delay = std::clamp(delay, 1.0, (double) capacity());
        const double pos = (double) count - delay;
        const auto i = (int64_t) std::floor(pos);
        const float t = (float) (pos - (double) i);
        if (t == 0.0f) return at(c, i);
        if (delay < 3.0) return at(c, i) + t * (at(c, i + 1) - at(c, i));
        return hermite(at(c, i - 1), at(c, i), at(c, i + 1), at(c, i + 2), t);
    }

private:
    std::vector<float> buf[2];
    int64_t size = 1, mask = 0, count = 0;
};

}  // namespace dsp
