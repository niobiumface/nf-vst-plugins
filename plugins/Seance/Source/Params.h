#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace params
{
// IDs are permanent once released - never rename.
inline constexpr auto time     = "time";     // ms (free)
inline constexpr auto sync     = "sync";
inline constexpr auto division = "division"; // note value when synced
inline constexpr auto feedback = "feedback";
inline constexpr auto wow      = "wow";      // wow + flutter
inline constexpr auto lowCut   = "lowcut";
inline constexpr auto highCut  = "highcut";
inline constexpr auto pingPong = "pingpong";
inline constexpr auto mix      = "mix";
inline constexpr auto bypass   = "bypass";

inline constexpr float timeMin = 1.0f, timeMax = 2000.0f;
inline constexpr float maxDelaySeconds = 4.0f; // synced values are clamped to this
inline constexpr float feedbackMax = 110.0f;

/** Division names and their length in quarter notes. Order is permanent once released. */
inline const juce::StringArray divisionNames { "1/64", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                                               "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" };
inline constexpr float divisionBeats[] = { 1.0f / 16, 1.0f / 8, 1.0f / 6, 1.0f / 4, 3.0f / 8, 1.0f / 3, 1.0f / 2, 3.0f / 4,
                                           2.0f / 3, 1.0f, 3.0f / 2, 4.0f / 3, 2.0f, 3.0f, 4.0f };
inline constexpr int numDivisions = (int) (sizeof (divisionBeats) / sizeof (divisionBeats[0]));

inline float divisionMs (int index, double bpm) noexcept
{
    return (float) (60000.0 / juce::jlimit (20.0, 999.0, bpm)) * divisionBeats[juce::jlimit (0, numDivisions - 1, index)];
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using P = AudioParameterFloat;
    using A = AudioParameterFloatAttributes;

    auto ms  = A().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return v < 100.0f ? String (v, 1) : String (roundToInt (v)); });
    auto pct = A().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)); });
    auto hz  = A().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
               {
                   return v >= 1000.0f ? String (v / 1000.0f, v >= 10000.0f ? 1 : 2) + "k" : String (roundToInt (v));
               });

    NormalisableRange<float> timeRange (timeMin, timeMax, 0.1f);
    timeRange.setSkewForCentre (250.0f);
    NormalisableRange<float> lowRange (20.0f, 2000.0f, 1.0f);
    lowRange.setSkewForCentre (200.0f);
    NormalisableRange<float> highRange (1000.0f, 20000.0f, 1.0f);
    highRange.setSkewForCentre (5000.0f);

    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> (ParameterID { time, 1 }, "Time", timeRange, 375.0f, ms));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { sync, 1 }, "Sync", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { division, 1 }, "Division", divisionNames, 7)); // 1/8D
    l.add (std::make_unique<P> (ParameterID { feedback, 1 }, "Feedback", NormalisableRange<float> (0.0f, feedbackMax, 0.1f), 45.0f, pct));
    l.add (std::make_unique<P> (ParameterID { wow, 1 },      "Wow/Flutter", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, pct));
    l.add (std::make_unique<P> (ParameterID { lowCut, 1 },   "Low Cut", lowRange, 120.0f, hz));
    l.add (std::make_unique<P> (ParameterID { highCut, 1 },  "High Cut", highRange, 5500.0f, hz));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { pingPong, 1 }, "Ping-Pong", false));
    l.add (std::make_unique<P> (ParameterID { mix, 1 },      "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f, pct));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { bypass, 1 }, "Bypass", false));
    return l;
}
} // namespace params
