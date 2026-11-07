juce_add_plugin(FrikfrakRhodes
    COMPANY_NAME "Frikfrak"
    PRODUCT_NAME "Frikfrak Rhodes"
    PLUGIN_MANUFACTURER_CODE Frik
    PLUGIN_CODE Rhdx           # fixed: a random code would break saved Live sets
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 AU)

target_sources(FrikfrakRhodes PRIVATE src/PluginProcessor.cpp)
frikfrak_plugin_setup(FrikfrakRhodes)
