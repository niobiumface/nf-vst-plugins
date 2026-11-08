---
name: juce-plugin-dev
description: Build, structure and extend VST3/standalone audio plugins in this repo (JUCE 8 + CMake + MSVC on Windows). Use when creating a new plugin, adding parameters, touching CMakeLists, building, debugging host issues, or validating a plugin.
---

# JUCE plugin development (this repo)

## Layout
```
CMakeLists.txt            root: adds libs/JUCE, shared/, every plugins/<Name>
libs/JUCE/                JUCE framework (git clone, not modified)
shared/pixelui/           header-only pixel-art UI framework (see pixel-ui skill)
shared/dsp/               header-only reusable DSP (see audio-dsp skill)
plugins/<Name>/
  CMakeLists.txt          juce_add_plugin(...) for that plugin
  Source/PluginProcessor.{h,cpp}
  Source/PluginEditor.{h,cpp}
  Source/Params.h         parameter IDs + layout factory
build/                    out-of-source build dir (gitignored)
```

## Build (Windows, VS 2022 at E:\Microsoft Visual Studio\2022\Community)
```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target CUtTA_VST3 CUtTA_Standalone
```
Artefacts: `build/plugins/<Name>/<Name>_artefacts/Release/{VST3,Standalone}/`.
`COPY_PLUGIN_AFTER_BUILD` is OFF by default (copying into `C:\Program Files\Common Files\VST3` needs admin).
Configure with `-DPLUGINS_COPY_AFTER_BUILD=ON` from an elevated shell to install automatically.

## Adding a new plugin
1. Copy `plugins/CUtTA` to `plugins/<Name>`, rename classes, change `PLUGIN_CODE` (4 chars, one uppercase) and `PRODUCT_NAME`.
2. Add `add_subdirectory(plugins/<Name>)` to the root CMakeLists.
3. Keep manufacturer code `Ctta`-style codes unique per plugin; never change a released plugin's code (hosts lose sessions).

## Rules that must hold
- **Real-time safety in `processBlock`**: no allocation, no locks, no logging, no `std::vector::push_back`, no string ops. Preallocate in `prepareToPlay`.
- Wrap processing in `juce::ScopedNoDenormals`.
- Parameters live in an `AudioProcessorValueTreeState`; read them through cached `std::atomic<float>*` (`apvts.getRawParameterValue`) once per block.
- Smooth anything that multiplies audio (`juce::SmoothedValue`), otherwise zipper noise.
- Parameter IDs are versioned with `juce::ParameterID { "id", 1 }` and **never renamed** after release.
- Report lookahead with `setLatencySamples()` and keep it constant (don't change latency when a section is bypassed).
- DSP → UI communication only through `std::atomic` meters (relaxed ordering is fine). UI → DSP only through parameters.
- State save/load: `apvts.copyState()` → XML. Non-automatable UI state (e.g. UI scale) goes into `apvts.state` properties.
- Editor never holds raw pointers into DSP buffers.

## Validation checklist
- Builds in Release without warnings that matter.
- Load the Standalone build, then a host (Reaper/Ableton/FL). Check: automation, preset recall, bypass, sample-rate change 44.1/48/96 k, buffer size 32..2048, mono and stereo.
- Optional: run Tracktion **pluginval** (`pluginval --strictness-level 5 --validate <path.vst3>`) — not installed by default.
