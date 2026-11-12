// Plays a drum + bass loop through the real PyreProcessor while the editor animates, and renders the editor
// into PNGs (visual check without a host).
// Usage: pyre_snapshot <outDir>          -> pyre_<shot>.png at 3x (with scanlines, like in a DAW)
//        pyre_snapshot <outDir> --web    -> preview.png (native 320x184) for the n-f.pro web preview
#include "../plugins/Pyre/Source/PluginProcessor.h"
#include "../plugins/Pyre/Source/PluginEditor.h"

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 800; // 1/60 s

void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s\n", f.getFullPathName().toRawUTF8());
}

juce::Image nativeFrame (PyreEditor& ed)
{
    ed.showScanlines = false;
    const auto big = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    const int s = ed.getPixelScale();
    juce::Image out (juce::Image::RGB, PyreEditor::W, PyreEditor::H, false);
    for (int y = 0; y < PyreEditor::H; ++y)
        for (int x = 0; x < PyreEditor::W; ++x)
            out.setPixelAt (x, y, big.getPixelAt (x * s + s / 2, y * s + s / 2));
    return out;
}

/** 120 bpm loop: kick on every beat, noise snare on 2 and 4, a saw bass on eighths. */
struct Loop
{
    juce::Random rng { 7 };
    double t = 0.0;

    float next()
    {
        const double beat = 0.5, tb = std::fmod (t, beat), te = std::fmod (t, beat * 0.5);
        const int beatIdx = (int) (std::fmod (t, beat * 4.0) / beat);
        t += 1.0 / sr;
        const float kick  = 0.7f * (float) std::exp (-tb * 18.0) * (float) std::sin (6.283185307 * (45.0 + 120.0 * std::exp (-tb * 40.0)) * tb);
        const float snare = (beatIdx == 1 || beatIdx == 3) ? 0.3f * (float) std::exp (-tb * 25.0) * (rng.nextFloat() * 2.0f - 1.0f) : 0.0f;
        const double f = beatIdx == 2 ? 49.0 : 55.0;
        const float bass  = 0.22f * (float) (2.0 * std::fmod (t * f, 1.0) - 1.0) * (float) std::min (1.0, te * 200.0) * (float) std::exp (-te * 4.0);
        return kick + snare + bass;
    }
};

void play (PyreProcessor& proc, PyreEditor& ed, Loop& loop, double seconds)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < (int) (seconds * 60.0); ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const float s = loop.next();
            buf.setSample (0, i, s); buf.setSample (1, i, s);
        }
        proc.processBlock (buf, midi);
        ed.advanceForTest (1.0f / 60.0f);
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const bool web = argc > 2 && juce::String (argv[2]) == "--web";

    PyreProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    auto set = [&] (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };

    if (web)
    {
        proc.apvts.state.setProperty ("uiScale", 2, nullptr);
        std::unique_ptr<PyreEditor> ed (static_cast<PyreEditor*> (proc.createEditor()));
        Loop loop;
        play (proc, *ed, loop, 4.3);
        writePng (nativeFrame (*ed), outDir.getChildFile ("preview.png"));
        return 0;
    }

    struct Shot { const char* name; int preset; float gain; };
    const Shot shots[] = { { "0_idle", 0, 0.0f }, { "1_embers", 1, 1.0f }, { "2_warm_tube", 2, 1.0f },
                           { "3_hot_tape", 4, 1.0f }, { "4_fuzz_box", 8, 1.0f }, { "5_witch_howl", 11, 1.0f } };

    for (auto& s : shots)
    {
        proc.setCurrentProgram (s.preset);
        if (s.preset == 11) set (params::bias, 60.0f);
        std::unique_ptr<PyreEditor> ed (static_cast<PyreEditor*> (proc.createEditor()));
        Loop loop;
        if (s.gain > 0.0f) play (proc, *ed, loop, 3.3);
        else               ed->advanceForTest (3.0f);
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f),
                  outDir.getChildFile (juce::String ("pyre_") + s.name + ".png"));
    }
    return 0;
}
