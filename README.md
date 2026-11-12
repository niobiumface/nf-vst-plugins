# n-f.pro VST plugins

Free audio plugins (VST3 + Standalone, Windows x64) built with **JUCE + CMake + MSVC**.
All plugins share one look: dark, modern pixel art with animations.

More info and direct downloads: <https://n-f.pro/tools/vst/>

| Plugin | What it does | Page |
| --- | --- | --- |
| **CUtTA** | Noise gate + lookahead brickwall limiter, 12 factory presets | <https://n-f.pro/tools/cutta/> |
| **Sub** | Stacked lowpass (1–4 layers × 12 dB/oct), resonance, drive, mix; a demon nun kneels deeper the darker the filter | <https://n-f.pro/tools/sub/> |
| **Iron S** | De-esser with click / lip-smack removal, real-time spectrum and 12 factory presets; a steam iron presses the wrinkles (sibilants) out of the cloth | – |

## CUtTA

Gate and limiter in one strip, for drums, guitars, vocals and podcasts.

- **Gate:** threshold (−80…0 dB), range (−80…0 dB), attack (0.05–50 ms), hold (0–500 ms), release (5–2000 ms)
- **Limiter:** drive (0–24 dB), ceiling (−12…0 dBFS), release (1–500 ms), stereo-linked, 1.5 ms lookahead
- Latency stays constant, also when the limiter is switched off
- Presets: INIT, KICK TIGHT, SNARE CLEAN, TOMS, GUITAR CHUG, BASS DI, VOCAL CLEANUP, PODCAST, SOFT EXPANDER, GATED VERB, BRICKWALL, CRUSHER

## Sub

Up to four cascaded zero-delay-feedback state-variable lowpass layers (12–48 dB/oct).

- **Cutoff** 20 Hz–20 kHz, **Resonance** 0–100 %, **Layers** 1–4, **Drive** 0–24 dB, **Mix** 0–100 %, **Output** −24…+12 dB
- The UI is a demon nun: the darker the filter, the further she sinks from standing to kneeling, head bowed

## Iron S

De-esser for vocals, podcasts and anything harsh, with a second stage for lip smacks and clicks.

- **Threshold** (−60…0 dB) – or simply drag the iron: the height of its sole above the board *is* the threshold
- **Range** 0–24 dB (maximum reduction), **Frequency** 2–16 kHz, **Width** 0.3–3 oct, **Speed** (release) 5–300 ms
- **Mode** relative (level-independent, follows the voice at any input gain) or absolute
- **Detect** band or highpass, **Cut** wideband or split (dynamic high shelf – bit-transparent while nothing is reduced)
- **Smack** 0–100 %: transient detector that ducks short clicks and lip smacks above 1.5 kHz
- **Lookahead** 0–5 ms at a constant 5 ms latency, **Stereo link** 0–100 %, **Output** ±12 dB, bypass with latency compensation
- **Listen**: output, detector sidechain, or delta (only what is removed)
- **Displays**: spectrum of input and output with the removed part highlighted, detector band and filter curve (drag: frequency / width), IN / GR / SMK / OUT meters with peak hold, the cloth as level and reduction history
- **FIND S** listens to the playing audio for three seconds and sets the frequency to the sibilance peak
- Presets: INIT, FEMALE VOX, MALE VOX, BRIGHT POP VOX, PODCAST, SMACK KILLER, WHISPER, HEAVY LISP, GENTLE WIDE, CYMBAL TAME, GUITAR FIZZ, MASTER DE-HARSH

## Build

Needs Visual Studio 2022 (MSVC) and CMake ≥ 3.22. JUCE is not committed:

```bash
git clone --depth 1 https://github.com/juce-framework/JUCE.git libs/JUCE
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Artefacts: `build/plugins/<Name>/<Name>_artefacts/Release/{VST3,Standalone}/`.

## Install

Copy `<Name>.vst3` to `C:\Program Files\Common Files\VST3\` and rescan plugins in your DAW.

## Layout

```
plugins/CUtTA   gate + limiter
plugins/Sub     stacked lowpass
plugins/IronS   de-esser + smack remover
shared/dsp      header-only DSP (NoiseGate, LookaheadLimiter, StackedLowpass, DeEsser)
shared/pixelui  header-only pixel-art UI framework
tests/          DSP unit test and UI snapshot tools
```

## License

Copyright (C) 2026 n-f.pro

This project is free software: you can redistribute it and/or modify it under the terms of the
**GNU Affero General Public License v3.0** as published by the Free Software Foundation – see [`LICENSE`](LICENSE).
It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

- Built with [JUCE](https://juce.com), used under the AGPLv3 option of JUCE's dual licence.
- The licence covers the code and the assets in this repository. It grants no rights to the names
  “n-f.pro” or the plugin names as trademarks.
- VST is a registered trademark of Steinberg Media Technologies GmbH.

Ready-made Windows builds with SHA-256 checksums: [n-f.pro/tools](https://n-f.pro/tools/) and the GitHub releases of this repository.
