#pragma once
// Performance pads, pure C++: 11 effect pads + Hold, fed by host parameters and MIDI notes.
// An effect is active while its pad is pressed (parameter OR note) or latched.
// Hold (parameter OR note) latches: pads pressed when Hold turns on stay on; while Hold is on,
// each press toggles a pad's latch; Hold turning off releases every latch.
#include <array>
#include <cstdint>

namespace dsp {

class Pads {
public:
    static constexpr int kEffects = 11, kHold = 11, kFirstNote = 36;  // C1 = first pad of most controllers

    void reset() { param.fill(false); note.fill(false); prev.fill(false); latched.fill(false); holdPrev = false; }

    void setParam(int i, bool on) { if (i >= 0 && i <= kHold) param[(size_t) i] = on; }
    void noteOn(int n) { if (const int i = n - kFirstNote; i >= 0 && i <= kHold) note[(size_t) i] = true; }
    void noteOff(int n) { if (const int i = n - kFirstNote; i >= 0 && i <= kHold) note[(size_t) i] = false; }
    void allNotesOff() { note.fill(false); }

    // Apply the edges since the last call. Call after every batch of setParam/note changes.
    void update() {
        const bool hold = pressed(kHold);
        if (hold && !holdPrev)
            for (int i = 0; i < kEffects; ++i) if (pressed(i)) latched[(size_t) i] = true;
        if (!hold && holdPrev) latched.fill(false);
        for (int i = 0; i < kEffects; ++i) {
            const bool p = pressed(i);
            if (hold && holdPrev && p && !prev[(size_t) i]) latched[(size_t) i] = !latched[(size_t) i];
            prev[(size_t) i] = p;
        }
        holdPrev = hold;
    }

    bool active(int i) const { return pressed(i) || latched[(size_t) i]; }
    bool holdOn() const { return holdPrev; }

    // Bit i = effect i active, bit 11 = Hold.
    uint32_t mask() const {
        uint32_t m = holdPrev ? (1u << kHold) : 0u;
        for (int i = 0; i < kEffects; ++i) if (active(i)) m |= 1u << i;
        return m;
    }

private:
    bool pressed(int i) const { return param[(size_t) i] || note[(size_t) i]; }

    std::array<bool, kHold + 1> param {}, note {};
    std::array<bool, kEffects> prev {}, latched {};
    bool holdPrev = false;
};

}  // namespace dsp
