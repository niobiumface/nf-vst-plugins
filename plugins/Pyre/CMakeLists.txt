juce_add_plugin(Pyre
    COMPANY_NAME "n-f.pro"
    PLUGIN_MANUFACTURER_CODE Glab
    PLUGIN_CODE Pyre
    FORMATS VST3 Standalone
    PRODUCT_NAME "Pyre"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    COPY_PLUGIN_AFTER_BUILD ${PLUGINS_COPY_AFTER_BUILD})

target_sources(Pyre PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Pyre PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Pyre
    PRIVATE
        pixelui
        sharedsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Dev tool: plays a drum + bass loop through the real processor and renders the editor to PNG (tests/pyre_snapshot.cpp)
add_executable(pyre_snapshot ${CMAKE_SOURCE_DIR}/tests/pyre_snapshot.cpp)
target_link_libraries(pyre_snapshot PRIVATE Pyre)
target_include_directories(pyre_snapshot PRIVATE $<TARGET_PROPERTY:Pyre,INCLUDE_DIRECTORIES>)
target_compile_definitions(pyre_snapshot PRIVATE $<TARGET_PROPERTY:Pyre,COMPILE_DEFINITIONS>)
target_compile_features(pyre_snapshot PRIVATE cxx_std_20)

# Dev tool: renders the A/B demo (dry/wet WAV) through the real processor (tests/pyre_render.cpp)
add_executable(pyre_render ${CMAKE_SOURCE_DIR}/tests/pyre_render.cpp)
target_link_libraries(pyre_render PRIVATE Pyre)
target_include_directories(pyre_render PRIVATE $<TARGET_PROPERTY:Pyre,INCLUDE_DIRECTORIES>)
target_compile_definitions(pyre_render PRIVATE $<TARGET_PROPERTY:Pyre,COMPILE_DEFINITIONS>)
target_compile_features(pyre_render PRIVATE cxx_std_20)
