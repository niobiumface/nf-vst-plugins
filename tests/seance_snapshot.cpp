// Plays sparse plucks through the real SeanceProcessor while the editor animates, and renders the editor
// into PNGs (visual check without a host).
// Usage: seance_snapshot <outDir>          -> seance_<shot>.png at 3x (with scanlines, like in a DAW)
//        seance_snapshot <outDir> --web    -> preview.png (native 320x184)
//        seance_snapshot <outDir> --anim   -> anim_<name>/f_000.png ... native frames at 30 fps
#include "../plugins/Seance/Source/PluginProcessor.h"
#include "../plugins/Seance/Source/PluginEditor.h"

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

juce::Image nativeFrame (SeanceEditor& ed)
{
    ed.showScanlines = false;
    const auto big = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    const int s = ed.getPixelScale();
    juce::Image out (juce::Image::RGB, SeanceEditor::W, SeanceEditor::H, false);
    for (int y = 0; y < SeanceEditor::H; ++y)
        for (int x = 0; x < SeanceEditor::W; ++x)
            out.setPixelAt (x, y, big.getPixelAt (x * s + s / 2, y * s + s / 2));
    return out;
}

/** A plucked note every `every` seconds (a short phrase of three pitches), silence in between. */
struct Plucks
{
    double every = 2.0, t = 0.0;
    float gain = 0.6f;

    float next()
    {
        const double tp = std::fmod (t, every);
        const int n = (int) (t / every);
        t += 1.0 / sr;
        if (tp > 0.6) return 0.0f;
        static constexpr double freqs[] = { 220.0, 329.63, 261.63, 196.0 };
        const double f = freqs[n % 4];
        const double env = std::exp (-tp * 9.0) * std::min (1.0, tp * 2000.0);
        return gain * (float) (env * (std::sin (6.283185307 * f * tp) + 0.4 * std::sin (6.283185307 * 2.0 * f * tp)
                                      + 0.15 * std::sin (6.283185307 * 3.0 * f * tp)));
    }
};

void play (SeanceProcessor& proc, SeanceEditor& ed, Plucks& src, double seconds)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    for (int b = 0; b < (int) (seconds * 60.0); ++b)
    {
        for (int i = 0; i < block; ++i)
        {
            const float s = src.next();
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
    const juce::String mode = argc > 2 ? juce::String (argv[2]) : juce::String();

    SeanceProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);

    if (mode == "--anim")
    {
        proc.apvts.state.setProperty ("uiScale", 2, nullptr);
        struct Anim { const char* name; int preset; double every; };
        const Anim anims[] = { { "init", 0, 3.0 }, { "pingpong", 4, 3.0 }, { "warped", 6, 3.0 }, { "possessed", 11, 4.0 } };
        for (auto& a : anims)
        {
            proc.setCurrentProgram (a.preset);
            proc.prepareToPlay (sr, block);
            std::unique_ptr<SeanceEditor> ed (static_cast<SeanceEditor*> (proc.createEditor()));
            Plucks src; src.every = a.every;
            play (proc, *ed, src, a.every * 2.0);
            const auto dir = outDir.getChildFile (juce::String ("anim_") + a.name);
            dir.createDirectory();
            for (int f = 0; f < (int) (a.every * 30.0); ++f)
            {
                play (proc, *ed, src, 1.0 / 30.0);
                writePng (nativeFrame (*ed), dir.getChildFile (juce::String::formatted ("f_%03d.png", f)));
            }
        }
        return 0;
    }

    if (mode == "--web")
    {
        proc.apvts.state.setProperty ("uiScale", 2, nullptr);
        proc.setCurrentProgram (4);
        proc.prepareToPlay (sr, block);
        std::unique_ptr<SeanceEditor> ed (static_cast<SeanceEditor*> (proc.createEditor()));
        Plucks src; src.every = 3.0;
        play (proc, *ed, src, 4.6);
        writePng (nativeFrame (*ed), outDir.getChildFile ("preview.png"));
        return 0;
    }

    struct Shot { const char* name; int preset; double seconds; bool audio; };
    const Shot shots[] = { { "0_idle", 0, 2.0, false },
                           { "1_init_echoes", 0, 2.9, true },     // mid echo train
                           { "2_pingpong", 4, 4.7, true },
                           { "3_warped_tape", 6, 2.8, true },
                           { "4_possessed", 11, 3.5, true },
                           { "5_good_bye", 1, 3.9, true } };      // slapback: the train is over, the planchette says good bye

    for (auto& s : shots)
    {
        proc.setCurrentProgram (s.preset);
        proc.prepareToPlay (sr, block);
        std::unique_ptr<SeanceEditor> ed (static_cast<SeanceEditor*> (proc.createEditor()));
        Plucks src; src.every = 5.0;
        if (s.audio) play (proc, *ed, src, s.seconds);
        else         ed->advanceForTest ((float) s.seconds);
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f),
                  outDir.getChildFile (juce::String ("seance_") + s.name + ".png"));
    }
    return 0;
}
