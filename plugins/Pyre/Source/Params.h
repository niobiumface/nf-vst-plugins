#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace params
{
// IDs are permanent once released - never rename.
inline constexpr auto drive    = "drive";
inline constexpr auto mode     = "mode";     // 0 tube, 1 tape, 2 fuzz, 3 fold
inline constexpr auto bias     = "bias";
inline constexpr auto tone     = "tone";
inline constexpr auto mix      = "mix";
inline constexpr auto output   = "output";
inline constexpr auto autoGain = "autogain";
inline constexpr auto bypass   = "bypass";

inline constexpr float driveMax = 36.0f;

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;

    auto db  = A().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return String (v, 1); });
    auto pct = A().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); });
    auto sgn = A().withLabel ("%").withStringFromValueFunction ([] (float v, int)
               {
                   const int i = roundToInt (v);
                   return (i > 0 ? "+" : "") + String (i);
               });

    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> (ParameterID { drive, 1 },  "Drive",  NormalisableRange<float> (0.0f, driveMax, 0.1f), 9.0f, db));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { mode, 1 }, "Mode", StringArray { "Tube", "Tape", "Fuzz", "Fold" }, 0));
    l.add (std::make_unique<P> (ParameterID { bias, 1 },   "Bias",   NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, sgn));
    l.add (std::make_unique<P> (ParameterID { tone, 1 },   "Tone",   NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, sgn));
    l.add (std::make_unique<P> (ParameterID { mix, 1 },    "Mix",    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));
    l.add (std::make_unique<P> (ParameterID { output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { autoGain, 1 }, "Auto Gain", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));
    return l;
}
} // namespace params
