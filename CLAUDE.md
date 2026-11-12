# VST plugin workspace

Audio plugins (VST3 + Standalone) built with **JUCE 9 + CMake + MSVC (VS 2022)** on Windows.
All plugins share one look: **dark, modern pixel art with animations**.

## Skills (in `.claude/skills/`) — load the matching one before working
- `juce-plugin-dev` – repo layout, build commands, adding plugins, real-time rules.
- `audio-dsp` – gate / limiter / envelope recipes in `shared/dsp`.
- `pixel-ui` – the `shared/pixelui` framework, palette and animation rules.

## Quick build
```bash
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Setup on a fresh machine
JUCE is not committed: `git clone --depth 1 https://github.com/juce-framework/JUCE.git libs/JUCE`

## Plugins
- **CUtTA** (`plugins/CUtTA`) – noise gate + lookahead brickwall limiter. Logo: the first "t" is an inverted cross. Accent: blood red.
- **Sub** (`plugins/Sub`) – stacked lowpass (1–4 layers × 12 dB/oct, reso, drive, mix). UI: a demon nun that sinks from standing to kneeling with head bowed the darker the filter setting; face/eyes/horns darken too. Accent: sulphur violet (`subpal`).
- **Iron S** (`plugins/IronS`, target `IronS`, product name "Iron S") – de-esser + click/lip-smack remover (`shared/dsp/DeEsser.h`). UI: a steam iron on an ironing board. Wrinkle height = detector level, the height of the iron's sole = threshold (drag the iron); what sticks out is pressed flat and steams, a ghost shows what was removed. Spectrum with FIND S (learns the sibilance frequency). Accent: hot-metal orange (`ironpal`).
- **Pyre** (`plugins/Pyre`) – saturator (`shared/dsp/Saturator.h`: tube/tape/fuzz/fold, bias, tilt tone, mix, auto gain), 4× oversampled. UI: a pyre of logs around a stake with a skull; flame height = level into the shaper (drag the fire = drive), bias = wind, tone = flame colour, mode = how it burns. Live transfer curve + harmonics view. Accent: flame gold (`pyrepal`).

## Conventions
- UI text and comments in English; talk to the user in German.
- Never rename parameter IDs or plugin codes once released.
