#pragma once
// Glitch parameters as plain tables (pure C++). IDs are part of saved Live sets: never rename one.
// Pad order = UI order = MIDI pitch classes C..A# (C1 = 36 = first pad); Hold has no note.
#include <array>
#include <string_view>

namespace glitch {

enum Fx { Crush, Pitch, Reverb, Stutter, Gate, Phaser, Filter, TapeStop, Reverse, Echo, Downer, kFxCount };

struct PadSpec { const char* id; const char* name; };
inline constexpr std::array<PadSpec, kFxCount + 1> kPads { {
    { "pad_crush", "Crush" }, { "pad_pitch", "Pitch" }, { "pad_reverb", "Reverb" }, { "pad_stutter", "Stutter" },
    { "pad_gate", "Gate" }, { "pad_phaser", "Phaser" }, { "pad_filter", "Filter" }, { "pad_tape", "Tape Stop" },
    { "pad_reverse", "Reverse" }, { "pad_echo", "Echo" }, { "pad_downer", "Downer" }, { "pad_hold", "Hold" },
} };

struct Division { const char* label; double qn; };  // qn = length in quarter notes

inline constexpr std::array<Division, 7> kStutterDivs { { { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 } } };
inline constexpr std::array<Division, 6> kGateDivs { { { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 } } };
inline constexpr std::array<Division, 5> kTapeDivs { { { "1/8 beat", 0.125 }, { "1/4 beat", 0.25 }, { "1/2 beat", 0.5 }, { "1 beat", 1.0 }, { "2 beats", 2.0 } } };
inline constexpr std::array<Division, 5> kReverseDivs { { { "1/4 beat", 0.25 }, { "1/2 beat", 0.5 }, { "1 beat", 1.0 }, { "2 beats", 2.0 }, { "1 bar", 4.0 } } };
inline constexpr std::array<Division, 5> kEchoDivs { { { "1/8", 0.5 }, { "1/8D", 0.75 }, { "1/4", 1.0 }, { "1/4D", 1.5 }, { "1/2", 2.0 } } };
inline constexpr std::array<Division, 5> kDownerDivs { { { "1/16", 0.25 }, { "1/8", 0.5 }, { "1/4", 1.0 }, { "1/2", 2.0 }, { "1 bar", 4.0 } } };

// Knobs: float (min/max/default, step 0 = continuous) or choice (divs != nullptr, def = index).
// Name = "<effect> <knob> (<unit>)"; the UI drops the effect prefix.
struct KnobSpec { Fx fx; const char* id; const char* name; float min, max, def, step; const Division* divs; int nDivs; };

inline constexpr std::array<KnobSpec, 15> kKnobs { {
    { Crush, "crush_bits", "Crush Bits", 2.0f, 16.0f, 6.0f, 0.0f, nullptr, 0 },
    { Crush, "crush_rate", "Crush Rate (kHz)", 1.0f, 24.0f, 8.0f, 0.0f, nullptr, 0 },
    { Pitch, "pitch_semi", "Pitch Semitones (st)", -12.0f, 12.0f, -12.0f, 1.0f, nullptr, 0 },
    { Reverb, "reverb_size", "Reverb Size", 0.0f, 1.0f, 0.6f, 0.0f, nullptr, 0 },
    { Stutter, "stutter_div", "Stutter Division", 0, 0, 3, 0, kStutterDivs.data(), (int) kStutterDivs.size() },
    { Gate, "gate_rate", "Gate Rate", 0, 0, 2, 0, kGateDivs.data(), (int) kGateDivs.size() },
    { Gate, "gate_depth", "Gate Depth", 0.0f, 1.0f, 1.0f, 0.0f, nullptr, 0 },
    { Phaser, "phaser_speed", "Phaser Speed (Hz)", 0.1f, 4.0f, 0.5f, 0.0f, nullptr, 0 },
    { Filter, "filter_freq", "Filter Freq", -1.0f, 1.0f, -0.6f, 0.0f, nullptr, 0 },
    { TapeStop, "tape_time", "Tape Stop Time", 0, 0, 3, 0, kTapeDivs.data(), (int) kTapeDivs.size() },
    { Reverse, "reverse_len", "Reverse Length", 0, 0, 2, 0, kReverseDivs.data(), (int) kReverseDivs.size() },
    { Echo, "echo_time", "Echo Time", 0, 0, 1, 0, kEchoDivs.data(), (int) kEchoDivs.size() },
    { Echo, "echo_feedback", "Echo Feedback", 0.0f, 0.95f, 0.55f, 0.0f, nullptr, 0 },
    { Downer, "downer_rate", "Downer Rate", 0, 0, 2, 0, kDownerDivs.data(), (int) kDownerDivs.size() },
    { Downer, "downer_depth", "Downer Depth", 0.0f, 1.0f, 0.6f, 0.0f, nullptr, 0 },
} };

// Hardware knobs (Akai MPK Mini Plus default program: knobs 1-8 = CC 70-77).
// Knobs 1-2 follow the last pressed effect (its 1st/2nd knob); knobs 3-8 are fixed.
inline constexpr int kFirstKnobCC = 70;
inline constexpr std::array<std::string_view, 6> kFixedKnobIds { "pitch_semi", "reverb_size", "stutter_div", "filter_freq", "echo_feedback", "gate_depth" };

// Index in kKnobs driven by this CC, or -1 (unmapped CC, or the effect has no such knob).
constexpr int knobForCC(int cc, int lastFx) {
    const int slot = cc - kFirstKnobCC;
    if (slot == 0 || slot == 1) {
        for (int i = 0, seen = 0; i < (int) kKnobs.size(); ++i)
            if (kKnobs[(size_t) i].fx == lastFx && seen++ == slot) return i;
        return -1;
    }
    if (slot >= 2 && slot < 2 + (int) kFixedKnobIds.size())
        for (int i = 0; i < (int) kKnobs.size(); ++i)
            if (kKnobs[(size_t) i].id == kFixedKnobIds[(size_t) slot - 2]) return i;
    return -1;
}

}  // namespace glitch
