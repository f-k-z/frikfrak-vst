#pragma once
// Kankyo parameter specs and factory presets (see shared/presets/PresetTable.h).
// Presets are mood interpretations of 80s Japanese environmental music, not documented settings.
#include "presets/PresetTable.h"

namespace kankyo {

inline constexpr std::array<presets::ParamSpec, 16> kParams { {
    { "blend", "Blend", 0.0f, 1.0f, 0.3f },          // analog .. FM
    { "shape", "Shape", 0.0f, 1.0f, 0.5f },          // tri .. saw .. pulse
    { "detune", "Detune", 0.0f, 1.0f, 0.4f },
    { "color", "Color", 0.0f, 1.0f, 0.45f },         // low-pass 150 Hz .. 16 kHz
    { "ratio", "Ratio", 0.5f, 14.0f, 3.5f, 0.4f },   // FM modulator : carrier
    { "bright", "Bright", 0.0f, 1.0f, 0.3f },        // FM index
    { "air", "Air", 0.0f, 1.0f, 0.0f },              // breath noise
    { "motion", "Motion", 0.0f, 1.0f, 0.3f },        // slow LFO
    { "attack", "Attack (s)", 0.005f, 8.0f, 1.2f, 0.35f },
    { "decay", "Decay (s)", 0.05f, 8.0f, 2.0f, 0.35f },
    { "sustain", "Sustain", 0.0f, 1.0f, 0.8f },
    { "release", "Release (s)", 0.05f, 12.0f, 4.0f, 0.35f },
    { "chorus", "Chorus", 0.0f, 1.0f, 0.5f },
    { "space", "Space", 0.0f, 1.0f, 0.45f },         // reverb mix
    { "size", "Size", 0.0f, 1.0f, 0.6f },            // reverb decay 1.5 .. 10 s
    { "volume", "Volume (dB)", -24.0f, 6.0f, -9.0f },
} };

inline constexpr std::array<presets::Preset<kParams.size()>, 1> kPresets { {
    //            blend  shape  detune color  ratio  bright air    motion  atk    dec    sus    rel    chorus space  size   vol
    { "Init", { 0.30f, 0.50f, 0.40f, 0.45f, 3.5f, 0.30f, 0.00f, 0.30f, 1.20f, 2.00f, 0.80f, 4.00f, 0.50f, 0.45f, 0.60f, -9.0f } },
} };

}  // namespace kankyo
