#pragma once
// Parameter specs + factory presets as plain tables (pure C++, shared by plugins and dsp_tests).
// A spec ID is part of saved Live sets: never rename one. Spec order = knob order in the UI.
#include <array>
#include <cstddef>

namespace presets {

struct ParamSpec { const char* id; const char* name; float min, max, def; };

template <std::size_t N>
struct Preset { const char* name; std::array<float, N> v; };  // values in spec order

}  // namespace presets
