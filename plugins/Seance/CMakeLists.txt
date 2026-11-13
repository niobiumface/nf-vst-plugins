juce_add_plugin(Seance
    COMPANY_NAME "n-f.pro"
    PLUGIN_MANUFACTURER_CODE Glab
    PLUGIN_CODE Sean
    FORMATS VST3 Standalone
    PRODUCT_NAME "Seance"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay
    COPY_PLUGIN_AFTER_BUILD ${PLUGINS_COPY_AFTER_BUILD})

target_sources(Seance PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Seance PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Seance
    PRIVATE
        pixelui
        sharedsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Dev tool: plays sparse hits and a phrase through the real processor and renders the editor to PNG (tests/seance_snapshot.cpp)
add_executable(seance_snapshot ${CMAKE_SOURCE_DIR}/tests/seance_snapshot.cpp)
target_link_libraries(seance_snapshot PRIVATE Seance)
target_include_directories(seance_snapshot PRIVATE $<TARGET_PROPERTY:Seance,INCLUDE_DIRECTORIES>)
target_compile_definitions(seance_snapshot PRIVATE $<TARGET_PROPERTY:Seance,COMPILE_DEFINITIONS>)
target_compile_features(seance_snapshot PRIVATE cxx_std_20)

# Dev tool: renders the A/B demo (dry/wet WAV) through the real processor (tests/seance_render.cpp)
add_executable(seance_render ${CMAKE_SOURCE_DIR}/tests/seance_render.cpp)
target_link_libraries(seance_render PRIVATE Seance)
target_include_directories(seance_render PRIVATE $<TARGET_PROPERTY:Seance,INCLUDE_DIRECTORIES>)
target_compile_definitions(seance_render PRIVATE $<TARGET_PROPERTY:Seance,COMPILE_DEFINITIONS>)
target_compile_features(seance_render PRIVATE cxx_std_20)
