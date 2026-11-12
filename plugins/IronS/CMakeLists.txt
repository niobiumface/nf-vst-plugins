juce_add_plugin(IronS
    COMPANY_NAME "n-f.pro"
    PLUGIN_MANUFACTURER_CODE Glab
    PLUGIN_CODE IrnS
    FORMATS VST3 Standalone
    PRODUCT_NAME "Iron S"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${PLUGINS_COPY_AFTER_BUILD})

target_sources(IronS PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(IronS PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(IronS
    PRIVATE
        pixelui
        sharedsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Dev tool: plays a synthetic voice through the real processor and renders the editor to PNG (tests/irons_snapshot.cpp)
add_executable(irons_snapshot ${CMAKE_SOURCE_DIR}/tests/irons_snapshot.cpp)
target_link_libraries(irons_snapshot PRIVATE IronS)
target_include_directories(irons_snapshot PRIVATE $<TARGET_PROPERTY:IronS,INCLUDE_DIRECTORIES>)
target_compile_definitions(irons_snapshot PRIVATE $<TARGET_PROPERTY:IronS,COMPILE_DEFINITIONS>)
target_compile_features(irons_snapshot PRIVATE cxx_std_20)

# Dev tool: renders the A/B demo (dry/wet WAV) through the real processor (tests/irons_render.cpp)
add_executable(irons_render ${CMAKE_SOURCE_DIR}/tests/irons_render.cpp)
target_link_libraries(irons_render PRIVATE IronS)
target_include_directories(irons_render PRIVATE $<TARGET_PROPERTY:IronS,INCLUDE_DIRECTORIES>)
target_compile_definitions(irons_render PRIVATE $<TARGET_PROPERTY:IronS,COMPILE_DEFINITIONS>)
target_compile_features(irons_render PRIVATE cxx_std_20)
