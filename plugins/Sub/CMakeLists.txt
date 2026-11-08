juce_add_plugin(Sub
    COMPANY_NAME "n-f.pro"
    PLUGIN_MANUFACTURER_CODE Glab
    PLUGIN_CODE Subx
    FORMATS VST3 Standalone
    PRODUCT_NAME "Sub"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Filter
    COPY_PLUGIN_AFTER_BUILD ${PLUGINS_COPY_AFTER_BUILD})

target_sources(Sub PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Sub PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Sub
    PRIVATE
        pixelui
        sharedsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Dev tool: renders the editor at several darkness levels to PNG (tests/sub_snapshot.cpp)
add_executable(sub_snapshot ${CMAKE_SOURCE_DIR}/tests/sub_snapshot.cpp)
target_link_libraries(sub_snapshot PRIVATE Sub)
target_include_directories(sub_snapshot PRIVATE $<TARGET_PROPERTY:Sub,INCLUDE_DIRECTORIES>)
target_compile_definitions(sub_snapshot PRIVATE $<TARGET_PROPERTY:Sub,COMPILE_DEFINITIONS>)
target_compile_features(sub_snapshot PRIVATE cxx_std_20)
