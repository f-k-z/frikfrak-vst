juce_add_plugin(FrikfrakKankyo
    COMPANY_NAME "Frikfrak"
    PRODUCT_NAME "Frikfrak Kankyo"
    PLUGIN_MANUFACTURER_CODE Frik
    PLUGIN_CODE Knkx           # fixed: a random code would break saved Live sets
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 AU)

target_sources(FrikfrakKankyo PRIVATE src/PluginProcessor.cpp)
frikfrak_plugin_setup(FrikfrakKankyo)
