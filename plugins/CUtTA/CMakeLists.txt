juce_add_plugin(CUtTA
    COMPANY_NAME "n-f.pro"
    PLUGIN_MANUFACTURER_CODE Glab
    PLUGIN_CODE Ctta
    FORMATS VST3 Standalone
    PRODUCT_NAME "CUtTA"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${PLUGINS_COPY_AFTER_BUILD})

target_sources(CUtTA PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(CUtTA PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CUtTA
    PRIVATE
        pixelui
        sharedsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Dev tool: renders the editor for a few presets to PNG (tests/cutta_snapshot.cpp)
add_executable(cutta_snapshot ${CMAKE_SOURCE_DIR}/tests/cutta_snapshot.cpp)
target_link_libraries(cutta_snapshot PRIVATE CUtTA)
target_include_directories(cutta_snapshot PRIVATE $<TARGET_PROPERTY:CUtTA,INCLUDE_DIRECTORIES>)
target_compile_definitions(cutta_snapshot PRIVATE $<TARGET_PROPERTY:CUtTA,COMPILE_DEFINITIONS>)
target_compile_features(cutta_snapshot PRIVATE cxx_std_20)
