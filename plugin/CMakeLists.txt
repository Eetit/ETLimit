# The VST3 plugin: a thin JUCE layer around etlimit_core.

juce_add_plugin(ETLimit
    COMPANY_NAME            "Eetit"
    PRODUCT_NAME            "ETLimit"
    # These two 4-character codes identify the plugin to hosts. They must
    # NEVER change after the first public release, or saved projects will
    # no longer find the plugin.
    PLUGIN_MANUFACTURER_CODE Eeti
    PLUGIN_CODE              EtLm
    FORMATS                  VST3
    VST3_CATEGORIES          Fx Dynamics
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(ETLimit PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/Parameters.cpp)

target_compile_definitions(ETLimit PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ETLimit
    PRIVATE
        etlimit_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
