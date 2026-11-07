#pragma once
// Kankyo parameter specs and factory presets (see shared/presets/PresetTable.h).
// Presets are mood interpretations of 80s Japanese environmental music, not documented settings.
#include "presets/PresetTable.h"

namespace kankyo {

inline constexpr float kOutputMakeup = 8.0f;  // +18 dB after the reverb, so Volume sits around -10 dB

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
    { "volume", "Volume (dB)", -24.0f, 6.0f, -7.5f },
} };

inline constexpr std::array<presets::Preset<kParams.size()>, 36> kPresets { {
    //            blend  shape  detune color  ratio  bright air    motion  atk    dec    sus    rel    chorus space  size   vol
    { "Init", { 0.30f, 0.50f, 0.40f, 0.45f, 3.5f, 0.30f, 0.00f, 0.30f, 1.20f, 2.00f, 0.80f, 4.00f, 0.50f, 0.45f, 0.60f, -7.5f } },

    // Warm pads
    { "Asagiri",      { 0.15f, 0.50f, 0.55f, 0.38f, 2.0f,  0.15f, 0.10f, 0.45f, 2.50f, 3.00f, 0.85f, 6.00f, 0.60f, 0.50f, 0.70f, -7.5f } },
    { "Kumo",         { 0.05f, 0.00f, 0.35f, 0.28f, 1.0f,  0.00f, 0.05f, 0.30f, 3.00f, 3.00f, 0.90f, 7.00f, 0.80f, 0.55f, 0.75f, -7.5f } },
    { "Hidamari",     { 0.10f, 0.85f, 0.40f, 0.45f, 2.0f,  0.10f, 0.00f, 0.55f, 1.80f, 2.50f, 0.85f, 5.00f, 0.50f, 0.45f, 0.60f, -18.0f } },
    { "Yuki",         { 0.35f, 0.30f, 0.30f, 0.35f, 3.5f,  0.25f, 0.10f, 0.25f, 2.50f, 4.00f, 0.75f, 7.00f, 0.60f, 0.60f, 0.85f, -7.0f } },
    { "Tegami",       { 0.55f, 0.10f, 0.15f, 0.45f, 1.0f,  0.35f, 0.00f, 0.60f, 0.02f, 3.00f, 0.50f, 2.50f, 0.70f, 0.40f, 0.50f, -5.5f } },
    { "Ensoku",       { 0.20f, 0.45f, 0.45f, 0.42f, 2.0f,  0.10f, 0.00f, 0.30f, 1.20f, 2.00f, 0.85f, 4.00f, 0.50f, 0.40f, 0.55f, -7.5f } },
    // Glass and bells (FM)
    { "Garasu",       { 0.90f, 0.50f, 0.20f, 0.50f, 3.5f,  0.45f, 0.00f, 0.10f, 0.005f, 4.00f, 0.00f, 4.50f, 0.30f, 0.50f, 0.75f, -10.5f } },
    { "Kori",         { 0.95f, 0.50f, 0.10f, 0.60f, 5.19f, 0.50f, 0.00f, 0.00f, 0.005f, 1.50f, 0.00f, 2.00f, 0.20f, 0.45f, 0.60f, -9.0f } },
    { "Suisho",       { 0.90f, 0.50f, 0.15f, 0.50f, 7.0f,  0.35f, 0.00f, 0.10f, 0.005f, 6.00f, 0.10f, 7.00f, 0.40f, 0.60f, 0.90f, -9.0f } },
    { "Fuurin",       { 1.00f, 0.50f, 0.00f, 0.60f, 5.19f, 0.60f, 0.00f, 0.00f, 0.005f, 2.50f, 0.00f, 3.00f, 0.30f, 0.55f, 0.80f, -10.0f } },
    { "Tsurara",      { 1.00f, 0.50f, 0.00f, 0.70f, 14.0f, 0.30f, 0.00f, 0.00f, 0.005f, 0.80f, 0.00f, 1.50f, 0.10f, 0.50f, 0.65f, -7.5f } },
    { "Yoake",        { 0.60f, 0.30f, 0.35f, 0.35f, 3.5f,  0.30f, 0.05f, 0.30f, 1.50f, 4.00f, 0.60f, 6.00f, 0.60f, 0.55f, 0.75f, -10.0f } },
    // Breath and sho
    { "Sho",          { 0.10f, 0.95f, 0.60f, 0.50f, 2.0f,  0.00f, 0.35f, 0.15f, 2.00f, 3.00f, 0.90f, 3.50f, 0.30f, 0.40f, 0.60f, -14.0f } },
    { "Kaze",         { 0.50f, 0.00f, 0.20f, 0.40f, 1.0f,  0.00f, 0.80f, 0.60f, 2.50f, 3.00f, 0.85f, 5.00f, 0.60f, 0.55f, 0.75f, -10.0f } },
    { "Iki",          { 0.20f, 0.30f, 0.40f, 0.35f, 1.0f,  0.00f, 0.40f, 1.00f, 3.00f, 3.00f, 0.80f, 6.00f, 0.50f, 0.50f, 0.70f, -4.5f } },
    { "Asa no Ki",    { 0.40f, 0.20f, 0.30f, 0.55f, 2.0f,  0.15f, 0.30f, 0.35f, 1.50f, 2.50f, 0.80f, 4.00f, 0.50f, 0.45f, 0.60f, -9.0f } },
    { "Yuuge",        { 0.15f, 0.60f, 0.45f, 0.25f, 1.0f,  0.00f, 0.30f, 0.40f, 3.00f, 4.00f, 0.85f, 7.00f, 0.60f, 0.55f, 0.80f, -7.5f } },
    { "Kasumi",       { 0.25f, 0.40f, 0.80f, 0.40f, 2.0f,  0.10f, 0.20f, 0.50f, 3.50f, 4.00f, 0.90f, 8.00f, 1.00f, 0.60f, 0.80f, -4.5f } },
    // Dark drones
    { "Yoru",         { 0.10f, 0.50f, 0.30f, 0.18f, 1.0f,  0.00f, 0.05f, 0.30f, 4.00f, 4.00f, 1.00f, 8.00f, 0.50f, 0.50f, 0.80f, -7.0f } },
    { "Fukai",        { 0.20f, 0.00f, 0.50f, 0.12f, 0.5f,  0.20f, 0.10f, 0.40f, 5.00f, 4.00f, 1.00f, 10.0f, 0.70f, 0.60f, 0.90f, -4.0f } },
    { "Tsuki",        { 0.70f, 0.00f, 0.20f, 0.30f, 2.0f,  0.08f, 0.00f, 0.50f, 4.00f, 4.00f, 1.00f, 9.00f, 0.60f, 0.65f, 0.95f, -12.5f } },
    { "Mori",         { 0.15f, 0.60f, 0.55f, 0.22f, 1.0f,  0.00f, 0.15f, 0.35f, 4.00f, 4.00f, 0.95f, 9.00f, 0.60f, 0.60f, 0.85f, -7.0f } },
    { "Oku",          { 0.30f, 0.50f, 0.40f, 0.20f, 1.0f,  0.10f, 0.10f, 0.20f, 0.30f, 4.00f, 0.90f, 10.0f, 0.40f, 0.85f, 1.00f, -12.0f } },
    { "Zen",          { 1.00f, 0.00f, 0.00f, 0.30f, 1.0f,  0.00f, 0.00f, 0.00f, 3.00f, 4.00f, 1.00f, 8.00f, 0.20f, 0.50f, 0.85f, -17.0f } },
    // Mallets and water
    { "Mizu",         { 1.00f, 0.50f, 0.00f, 0.50f, 1.0f,  0.60f, 0.00f, 0.00f, 0.005f, 0.35f, 0.00f, 0.60f, 0.30f, 0.55f, 0.75f, -4.5f } },
    { "Ame",          { 0.90f, 0.50f, 0.10f, 0.65f, 2.0f,  0.40f, 0.05f, 0.00f, 0.005f, 0.50f, 0.00f, 0.80f, 0.40f, 0.50f, 0.60f, -3.0f } },
    { "Shizuku",      { 1.00f, 0.50f, 0.00f, 0.50f, 1.0f,  0.50f, 0.00f, 0.00f, 0.005f, 0.50f, 0.00f, 1.00f, 0.20f, 0.75f, 1.00f, -4.0f } },
    { "Marimba Kage", { 1.00f, 0.50f, 0.00f, 0.40f, 3.5f,  0.35f, 0.00f, 0.00f, 0.005f, 0.80f, 0.00f, 0.60f, 0.10f, 0.35f, 0.45f, -9.0f } },
    { "Vibe Asa",     { 0.90f, 0.50f, 0.05f, 0.50f, 4.0f,  0.25f, 0.00f, 0.40f, 0.005f, 3.00f, 0.00f, 2.50f, 0.60f, 0.45f, 0.60f, -9.0f } },
    { "Take",         { 0.95f, 0.50f, 0.00f, 0.45f, 1.5f,  0.50f, 0.10f, 0.00f, 0.005f, 0.25f, 0.00f, 0.40f, 0.10f, 0.40f, 0.50f, -6.5f } },
    // Evolving and hybrid
    { "Nagare",       { 0.20f, 0.50f, 0.50f, 0.35f, 2.0f,  0.10f, 0.10f, 0.90f, 2.50f, 3.00f, 0.85f, 6.00f, 0.60f, 0.50f, 0.75f, -8.0f } },
    { "Tsuyu",        { 0.45f, 0.40f, 0.40f, 0.40f, 3.5f,  0.40f, 0.05f, 0.40f, 0.80f, 5.00f, 0.50f, 6.00f, 0.60f, 0.55f, 0.80f, -7.5f } },
    { "Shio",         { 0.25f, 0.45f, 0.50f, 0.30f, 1.0f,  0.10f, 0.20f, 0.70f, 5.00f, 4.00f, 0.90f, 8.00f, 0.60f, 0.55f, 0.85f, -4.5f } },
    { "Niwa",         { 0.60f, 0.30f, 0.30f, 0.45f, 3.5f,  0.40f, 0.10f, 0.40f, 0.01f, 2.50f, 0.35f, 4.00f, 0.50f, 0.50f, 0.70f, -6.5f } },
    { "Umi",          { 0.35f, 0.50f, 0.70f, 0.35f, 5.19f, 0.25f, 0.25f, 0.60f, 2.00f, 5.00f, 0.80f, 8.00f, 0.80f, 0.60f, 0.90f, -8.0f } },
} };

}  // namespace kankyo
