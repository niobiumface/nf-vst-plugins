#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace params
{
// IDs are permanent once released - never rename.
inline constexpr auto thresh = "thresh";
inline constexpr auto range  = "range";
inline constexpr auto freq   = "freq";
inline constexpr auto width  = "width";
inline constexpr auto speed  = "speed";
inline constexpr auto smack  = "smack";
inline constexpr auto look   = "look";
inline constexpr auto link   = "link";
inline constexpr auto output = "output";
inline constexpr auto mode   = "mode";    // 0 relative, 1 absolute
inline constexpr auto detect = "detect";  // 0 band, 1 high
inline constexpr auto cut    = "cut";     // 0 wide, 1 split
inline constexpr auto listen = "listen";  // 0 off, 1 sidechain, 2 delta
inline constexpr auto bypass = "bypass";

inline constexpr float freqMin = 2000.0f, freqMax = 16000.0f;

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
    using C = AudioParameterChoice;

    auto db  = A().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return String (v, 1); });
    auto pct = A().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); });
    auto ms  = A().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0); });
    auto oct = A().withLabel ("oct").withStringFromValueFunction ([] (float v, int) { return String (v, 2); });
    auto hz  = A().withLabel ("Hz")
                   .withStringFromValueFunction ([] (float v, int) { return hzText (v); })
                   .withValueFromStringFunction ([] (const String& s)
                   {
                       const auto t = s.trim().toLowerCase();
                       const float v = t.getFloatValue();
                       return t.containsChar ('k') ? v * 1000.0f : v;
                   });

    NormalisableRange<float> freqRange (freqMin, freqMax);
    freqRange.setSkewForCentre (5657.0f); // geometric centre = logarithmic feel
    NormalisableRange<float> speedRange (5.0f, 300.0f, 0.1f);
    speedRange.setSkewForCentre (50.0f);

    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> (ParameterID { thresh, 1 }, "Threshold", NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -14.0f, db));
    l.add (std::make_unique<P> (ParameterID { range, 1 },  "Range",     NormalisableRange<float> (0.0f, 24.0f, 0.1f), 9.0f, db));
    l.add (std::make_unique<P> (ParameterID { freq, 1 },   "Frequency", freqRange, 6500.0f, hz));
    l.add (std::make_unique<P> (ParameterID { width, 1 },  "Width",     NormalisableRange<float> (0.3f, 3.0f, 0.01f), 1.2f, oct));
    l.add (std::make_unique<P> (ParameterID { speed, 1 },  "Speed",     speedRange, 40.0f, ms));
    l.add (std::make_unique<P> (ParameterID { smack, 1 },  "Smack",     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct));
    l.add (std::make_unique<P> (ParameterID { look, 1 },   "Lookahead", NormalisableRange<float> (0.0f, 5.0f, 0.1f), 2.0f, ms));
    l.add (std::make_unique<P> (ParameterID { link, 1 },   "Stereo Link", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct));
    l.add (std::make_unique<P> (ParameterID { output, 1 }, "Output",    NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, db));
    l.add (std::make_unique<C> (ParameterID { mode, 1 },   "Mode",      StringArray { "Relative", "Absolute" }, 0));
    l.add (std::make_unique<C> (ParameterID { detect, 1 }, "Detection", StringArray { "Band", "High" }, 0));
    l.add (std::make_unique<C> (ParameterID { cut, 1 },    "Reduction", StringArray { "Wide", "Split" }, 1));
    l.add (std::make_unique<C> (ParameterID { listen, 1 }, "Listen",    StringArray { "Off", "Sidechain", "Delta" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));
    return l;
}
} // namespace params
