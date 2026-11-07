#pragma once
// Performance pads, pure C++: 11 effect pads + Hold, fed by host parameters and MIDI notes.
// An effect is active while its pad is pressed (parameter OR note) or latched.
// Hold (parameter OR note) latches: pads pressed when Hold turns on stay on; while Hold is on,
// each press toggles a pad's latch; Hold turning off releases every latch.
#include <array>
#include <bitset>
#include <cstdint>

namespace dsp {

class Pads {
public:
    static constexpr int kEffects = 11, kHold = 11;

    void reset() { param.fill(false); notes.reset(); prev.fill(false); latched.fill(false); holdPrev = false; last = 0; }

    // Notes map by pitch class in every octave: C = pad 1 ... A# = pad 11; B does nothing (Hold is
    // on screen only). Pads from note 36 (C1) play the effects in order, any keyboard octave reaches all.
    void setParam(int i, bool on) { if (i >= 0 && i <= kHold) param[(size_t) i] = on; }
    void noteOn(int n) { if (n >= 0 && n < 128) notes.set((size_t) n); }
    void noteOff(int n) { if (n >= 0 && n < 128) notes.reset((size_t) n); }
    void allNotesOff() { notes.reset(); }

    // Apply the edges since the last call. Call after every batch of setParam/note changes.
    void update() {
        const bool hold = pressed(kHold);
        if (hold && !holdPrev)
            for (int i = 0; i < kEffects; ++i) if (pressed(i)) latched[(size_t) i] = true;
        if (!hold && holdPrev) latched.fill(false);
        for (int i = 0; i < kEffects; ++i) {
            const bool p = pressed(i);
            if (p && !prev[(size_t) i]) {
                last = i;
                if (hold && holdPrev) latched[(size_t) i] = !latched[(size_t) i];
            }
            prev[(size_t) i] = p;
        }
        holdPrev = hold;
    }

    bool active(int i) const { return pressed(i) || latched[(size_t) i]; }
    bool holdOn() const { return holdPrev; }
    int lastPressed() const { return last; }  // effect most recently pressed (0 until any press)

    // Bit i = effect i active, bit 11 = Hold.
    uint32_t mask() const {
        uint32_t m = holdPrev ? (1u << kHold) : 0u;
        for (int i = 0; i < kEffects; ++i) if (active(i)) m |= 1u << i;
        return m;
    }

private:
    bool pressed(int i) const {
        if (param[(size_t) i]) return true;
        if (i == kHold) return false;
        for (size_t n = (size_t) i; n < 128; n += 12) if (notes[n]) return true;
        return false;
    }

    std::array<bool, kHold + 1> param {};
    std::bitset<128> notes;
    std::array<bool, kEffects> prev {}, latched {};
    bool holdPrev = false;
    int last = 0;
};

}  // namespace dsp
