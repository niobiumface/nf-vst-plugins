#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace params
{
// IDs are permanent once released - never rename.
inline constexpr auto cutoff = "cutoff";
inline constexpr auto reso   = "reso";
inline constexpr auto layers = "layers";
inline constexpr auto drive  = "drive";
inline constexpr auto mix    = "mix";
inline constexpr auto output = "output";

inline juce::String hzText (float v)
{
    if (v < 1000.0f) return juce::String (juce::roundToInt (v));
    return juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + "K";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;

    auto db  = A().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return String (v, 1); });
    auto pct = A().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); });
    auto hz  = A().withLabel ("Hz")
                   .withStringFromValueFunction ([] (float v, int) { return hzText (v); })
                   .withValueFromStringFunction ([] (const String& s)
                   {
                       const auto t = s.trim().toLowerCase();
                       const float v = t.getFloatValue();
                       return t.containsChar ('k') ? v * 1000.0f : v;
                   });

    NormalisableRange<float> cutRange (20.0f, 20000.0f);
    cutRange.setSkewForCentre (630.0f); // ~ logarithmic feel

    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> (ParameterID { cutoff, 1 }, "Cutoff", cutRange, 2000.0f, hz));
    l.add (std::make_unique<P> (ParameterID { reso, 1 },   "Resonance", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 10.0f, pct));
    l.add (std::make_unique<AudioParameterInt> (ParameterID { layers, 1 }, "Layers", 1, 4, 2,
                                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int)
                                                { return String (v * 12) + " dB/oct"; })));
    l.add (std::make_unique<P> (ParameterID { drive, 1 },  "Drive",  NormalisableRange<float> (0.0f, 24.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<P> (ParameterID { mix, 1 },    "Mix",    NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));
    l.add (std::make_unique<P> (ParameterID { output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, db));
    return l;
}
} // namespace params
