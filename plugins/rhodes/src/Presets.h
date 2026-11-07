#pragma once
// Rhodes parameter specs and factory presets (see shared/presets/PresetTable.h).
// Presets are by-ear interpretations of nu soul records, not documented settings.
#include "presets/PresetTable.h"

namespace rhodes {

inline constexpr std::array<presets::ParamSpec, 8> kParams { {
    { "bark", "Bark", 0.0f, 1.0f, 0.5f },
    { "decay", "Decay", 0.25f, 2.0f, 1.0f },
    { "tone", "Tone", 0.0f, 1.0f, 1.0f },
    { "drive", "Drive", 0.0f, 1.0f, 0.2f },
    { "phaser", "Phaser", 0.0f, 1.0f, 0.0f },
    { "tremRate", "Tremolo Rate (Hz)", 0.5f, 10.0f, 4.5f },
    { "tremDepth", "Tremolo Depth", 0.0f, 1.0f, 0.3f },
    { "volume", "Volume (dB)", -24.0f, 6.0f, -6.0f },
} };

inline constexpr std::array<presets::Preset<kParams.size()>, 12> kPresets { {
    //                      bark  decay tone  drive phaser tremHz tremD  vol
    { "Init",             { 0.50f, 1.00f, 1.00f, 0.20f, 0.00f, 4.5f, 0.30f, -6.0f } },
    { "Smoke Room",       { 0.35f, 1.30f, 0.45f, 0.30f, 0.00f, 3.0f, 0.15f, -6.0f } },  // D'Angelo, Untitled (How Does It Feel)
    { "Chicken Grease",   { 0.60f, 0.70f, 0.60f, 0.45f, 0.10f, 4.5f, 0.00f, -7.0f } },  // D'Angelo, Voodoo
    { "Brown Sugar",      { 0.30f, 1.00f, 0.75f, 0.15f, 0.00f, 4.5f, 0.25f, -6.0f } },  // D'Angelo, 1995
    { "Mama's Phase",     { 0.45f, 1.20f, 0.60f, 0.25f, 0.55f, 4.5f, 0.00f, -6.0f } },  // Erykah Badu, Mama's Gun
    { "Water for Chocolate", { 0.30f, 1.00f, 0.50f, 0.40f, 0.00f, 2.5f, 0.10f, -6.0f } },  // Common
    { "Detroit Dust",     { 0.40f, 0.50f, 0.25f, 0.60f, 0.00f, 4.5f, 0.00f, -7.0f } },  // J Dilla / Slum Village
    { "Black Radio",      { 0.35f, 1.60f, 0.90f, 0.10f, 0.20f, 3.0f, 0.20f, -6.0f } },  // Robert Glasper
    { "Suitcase Pan",     { 0.30f, 1.00f, 0.80f, 0.15f, 0.00f, 5.5f, 0.70f, -6.0f } },  // 70s stereo Suitcase
    { "Velvet Bark",      { 0.90f, 1.00f, 0.70f, 0.35f, 0.00f, 4.5f, 0.00f, -8.0f } },
    { "Slow Swirl",       { 0.45f, 1.50f, 0.70f, 0.20f, 0.90f, 4.5f, 0.00f, -6.0f } },
    { "Felt Dream",       { 0.20f, 1.80f, 0.20f, 0.10f, 0.00f, 2.0f, 0.15f, -5.0f } },
} };

}  // namespace rhodes
