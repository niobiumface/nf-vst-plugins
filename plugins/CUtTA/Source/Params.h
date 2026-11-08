#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace params
{
// IDs are permanent once released - never rename.
inline constexpr auto gateOn      = "gateOn";
inline constexpr auto gateThresh  = "gateThresh";
inline constexpr auto gateRange   = "gateRange";
inline constexpr auto gateAttack  = "gateAttack";
inline constexpr auto gateHold    = "gateHold";
inline constexpr auto gateRelease = "gateRelease";
inline constexpr auto limOn       = "limOn";
inline constexpr auto limDrive    = "limDrive";
inline constexpr auto limCeiling  = "limCeiling";
inline constexpr auto limRelease  = "limRelease";

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;

    auto db = [] (int dec) { return A().withLabel ("dB").withStringFromValueFunction ([dec] (float v, int) { return String (v, dec); }); };
    auto ms = A().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return v < 10.0f ? String (v, 1) : String (roundToInt (v)); });

    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterBool> (ParameterID { gateOn, 1 }, "Gate On", true));
    l.add (std::make_unique<P> (ParameterID { gateThresh, 1 },  "Gate Threshold", NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -40.0f, db (1)));
    l.add (std::make_unique<P> (ParameterID { gateRange, 1 },   "Gate Range",     NormalisableRange<float> (-80.0f, 0.0f, 0.1f), -80.0f, db (1)));
    l.add (std::make_unique<P> (ParameterID { gateAttack, 1 },  "Gate Attack",    skewed (0.05f, 50.0f, 3.0f),    0.5f,  ms));
    l.add (std::make_unique<P> (ParameterID { gateHold, 1 },    "Gate Hold",      skewed (0.0f, 500.0f, 60.0f),   40.0f, ms));
    l.add (std::make_unique<P> (ParameterID { gateRelease, 1 }, "Gate Release",   skewed (5.0f, 2000.0f, 150.0f), 120.0f, ms));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { limOn, 1 }, "Limiter On", true));
    l.add (std::make_unique<P> (ParameterID { limDrive, 1 },    "Limiter Drive",   NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, db (1)));
    l.add (std::make_unique<P> (ParameterID { limCeiling, 1 },  "Limiter Ceiling", NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f, db (1)));
    l.add (std::make_unique<P> (ParameterID { limRelease, 1 },  "Limiter Release", skewed (1.0f, 500.0f, 60.0f), 50.0f, ms));
    return l;
}
} // namespace params
