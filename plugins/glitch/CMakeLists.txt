juce_add_plugin(FrikfrakGlitch
    COMPANY_NAME "Frikfrak"
    PRODUCT_NAME "Frikfrak Glitch"
    PLUGIN_MANUFACTURER_CODE Frik
    PLUGIN_CODE Gltx           # fixed: a random code would break saved Live sets
    IS_SYNTH FALSE             # audio effect, for the Main track
    NEEDS_MIDI_INPUT TRUE      # notes 36..47 play the pads (VST3; Live sends no MIDI to AUs)
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE       # a JUCE "MIDI effect" does not load in Live
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS VST3 AU)

target_sources(FrikfrakGlitch PRIVATE src/PluginProcessor.cpp)
frikfrak_plugin_setup(FrikfrakGlitch)
