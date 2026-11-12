// Plays a synthetic "voice" (vowel, sibilants, lip smacks) through the real IronSProcessor while the editor
// animates, and renders the editor into PNGs (visual check without a host).
// Usage: irons_snapshot <outDir>          -> irons_<shot>.png at 3x (with scanlines, like in a DAW)
//        irons_snapshot <outDir> --web    -> preview.png (native 336x192) for the n-f.pro web preview
#include "../plugins/IronS/Source/PluginProcessor.h"
#include "../plugins/IronS/Source/PluginEditor.h"

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

juce::Image nativeFrame (IronSEditor& ed)
{
    ed.showScanlines = false;
    const auto big = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    const int s = ed.getPixelScale();
    juce::Image out (juce::Image::RGB, IronSEditor::W, IronSEditor::H, false);
    for (int y = 0; y < IronSEditor::H; ++y)
        for (int x = 0; x < IronSEditor::W; ++x)
            out.setPixelAt (x, y, big.getPixelAt (x * s + s / 2, y * s + s / 2));
    return out;
}

/** 0.9 s loop: vowel (0-0.45 s), "s" (0.45-0.7 s), pause with a lip smack at 0.8 s. */
struct Voice
{
    juce::Random rng { 42 };
    double t = 0.0;
    float hp1 = 0.0f, hp2 = 0.0f, last = 0.0f;

    float next()
    {
        const double p = std::fmod (t, 0.9);
        t += 1.0 / sr;
        float y = 0.0f;
        if (p < 0.45)
        {
            const float env = (float) std::min (1.0, std::min (p, 0.45 - p) * 40.0);
            for (int h = 1; h <= 12; ++h)
                y += env * 0.25f / (float) h * (float) std::sin (6.283185307 * 140.0 * h * t) * (h == 4 || h == 5 ? 2.0f : 1.0f);
        }
        const float n = rng.nextFloat() * 2.0f - 1.0f;
        hp1 += 0.6f * (n - last - hp1); last = n;            // crude highpassed noise
        hp2 += 0.5f * (hp1 - hp2);
        if (p >= 0.45 && p < 0.7) y += 0.5f * (hp1 - hp2 * 0.5f) * (float) std::min (1.0, std::min (p - 0.45, 0.7 - p) * 50.0);
        if (p >= 0.8 && p < 0.802) y += 0.6f * n;            // smack
        return y * 0.4f;
    }
};

void play (IronSProcessor& proc, IronSEditor& ed, Voice& voice, double seconds)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < (int) (seconds * 60.0); ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const float s = voice.next();
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

    IronSProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    auto set = [&] (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };

    struct Shot { const char* name; float thresh, range, smack; double seconds; };
    const Shot shots[] = { { "0_hovering", 0.0f, 9.0f, 0.0f, 3.2 },    // iron high above the cloth: wrinkles pass
                           { "1_pressing", -14.0f, 9.0f, 0.0f, 3.2 },  // caught in the middle of an "s"
                           { "2_flat", -30.0f, 20.0f, 100.0f, 3.2 },   // everything ironed flat, smacks zapped
                           { "3_smack", -14.0f, 9.0f, 100.0f, 3.52 } };// right after the lip smack

    // --stages: the iron lowered step by step (threshold 0 … -42 dB), each frame caught in the middle of an "s"
    if (argc > 2 && juce::String (argv[2]) == "--stages")
    {
        proc.apvts.state.setProperty ("uiScale", 2, nullptr);
        set (params::range, 18.0f);
        set (params::smack, 0.0f);
        for (int i = 0; i < 8; ++i)
        {
            set (params::thresh, -6.0f * (float) i);
            std::unique_ptr<IronSEditor> ed (static_cast<IronSEditor*> (proc.createEditor()));
            Voice voice;
            play (proc, *ed, voice, 3.2);
            writePng (nativeFrame (*ed), outDir.getChildFile (juce::String::formatted ("stage_%d.png", i)));
        }
        return 0;
    }

    // --anim: native 336x192 frame sequences (30 fps, two loops of the 0.9 s voice) for the n-f.pro animations
    if (argc > 2 && juce::String (argv[2]) == "--anim")
    {
        proc.apvts.state.setProperty ("uiScale", 2, nullptr);
        struct Anim { const char* name; float thresh, range, smack; };
        const Anim anims[] = { { "press", -14.0f, 9.0f, 0.0f },     // the iron catches every "s"
                               { "smack", -14.0f, 9.0f, 100.0f },   // plus the lip smack being zapped
                               { "flat", -30.0f, 20.0f, 100.0f },   // everything ironed flat
                               { "hover", 0.0f, 9.0f, 0.0f } };     // iron lifted: the wrinkles pass untouched
        for (auto& a : anims)
        {
            set (params::thresh, a.thresh);
            set (params::range, a.range);
            set (params::smack, a.smack);
            std::unique_ptr<IronSEditor> ed (static_cast<IronSEditor*> (proc.createEditor()));
            Voice voice;
            play (proc, *ed, voice, 2.7); // settle: three full loops
            const auto dir = outDir.getChildFile (juce::String ("anim_") + a.name);
            dir.createDirectory();
            for (int f = 0; f < 54; ++f)
            {
                play (proc, *ed, voice, 1.0 / 30.0);
                writePng (nativeFrame (*ed), dir.getChildFile (juce::String::formatted ("f_%03d.png", f)));
            }
        }
        return 0;
    }

    if (web) proc.apvts.state.setProperty ("uiScale", 2, nullptr);
    for (auto& s : shots)
    {
        if (web && juce::String (s.name) != "1_pressing") continue;
        set (params::thresh, s.thresh);
        set (params::range, s.range);
        set (params::smack, s.smack);
        std::unique_ptr<IronSEditor> ed (static_cast<IronSEditor*> (proc.createEditor()));
        Voice voice;
        play (proc, *ed, voice, s.seconds);
        if (web) writePng (nativeFrame (*ed), outDir.getChildFile ("preview.png"));
        else     writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f),
                           outDir.getChildFile (juce::String ("irons_") + s.name + ".png"));
    }
    return 0;
}
