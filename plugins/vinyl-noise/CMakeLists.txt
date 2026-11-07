juce_add_plugin(FrikfrakVinylNoise
    COMPANY_NAME "Frikfrak"
    PRODUCT_NAME "Frikfrak Vinyl Noise"
    PLUGIN_MANUFACTURER_CODE Frik
    PLUGIN_CODE Vnzx           # fixed: a random code would break saved Live sets
    IS_SYNTH TRUE              # plug and play on a MIDI track, like the Rhodes
    NEEDS_MIDI_INPUT TRUE      # keys gate crackle/hiss
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 AU)

target_sources(FrikfrakVinylNoise PRIVATE src/PluginProcessor.cpp)
frikfrak_plugin_setup(FrikfrakVinylNoise)
