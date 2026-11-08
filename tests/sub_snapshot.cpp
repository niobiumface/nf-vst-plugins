// Renders the Sub editor at several darkness levels into PNGs (visual check without a host).
// Usage: sub_snapshot <outDir>          -> sub_<mood>.png at 3x (with scanlines, like in a DAW)
//        sub_snapshot <outDir> --web    -> preview.png (native 288x176) and stages.png (11 darkness stages,
//                                          stage area 138x176 each, native) for the n-f.pro web preview
#include "../plugins/Sub/Source/PluginProcessor.h"
#include "../plugins/Sub/Source/PluginEditor.h"

namespace
{
void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s\n", f.getFullPathName().toRawUTF8());
}

/** Snapshot without scanlines, then sample the centre of every scaled pixel = the exact logical canvas. */
juce::Image nativeFrame (SubEditor& ed)
{
    ed.showScanlines = false;
    const auto big = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
    const int s = ed.getPixelScale();
    juce::Image out (juce::Image::RGB, SubEditor::W, SubEditor::H, false);
    for (int y = 0; y < SubEditor::H; ++y)
        for (int x = 0; x < SubEditor::W; ++x)
            out.setPixelAt (x, y, big.getPixelAt (x * s + s / 2, y * s + s / 2));
    return out;
}

/** Parameters that give a target darkness t (same formula as SubEditor::tick, reso 0, drive 0, mix 100). */
std::pair<float, int> paramsForDarkness (float t)
{
    const int layers = 1 + juce::roundToInt (3.0f * t);
    const float cutDark = juce::jlimit (0.0f, 1.0f, (0.85f * t - 0.22f * (float) (layers - 1) / 3.0f) / 0.6f);
    return { 20.0f * std::pow (1000.0f, 1.0f - cutDark), layers };
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const bool web = argc > 2 && juce::String (argv[2]) == "--web";

    SubProcessor proc;
    auto set = [&] (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };

    if (web)
    {
        proc.apvts.state.setProperty ("uiScale", 2, nullptr);
        {
            std::unique_ptr<SubEditor> ed (static_cast<SubEditor*> (proc.createEditor()));
            ed->advanceForTest (6.0f);
            writePng (nativeFrame (*ed), outDir.getChildFile ("preview.png"));
        }

        constexpr int stages = 11;
        juce::Image strip (juce::Image::RGB, SubEditor::stageW * stages, SubEditor::H, true);
        for (int i = 0; i < stages; ++i)
        {
            const auto [cut, layers] = paramsForDarkness ((float) i / 10.0f);
            set (params::cutoff, cut);
            set (params::layers, (float) layers);
            set (params::reso, 0.0f);
            std::unique_ptr<SubEditor> ed (static_cast<SubEditor*> (proc.createEditor()));
            ed->advanceForTest (8.0f);
            const auto frame = nativeFrame (*ed);
            juce::Graphics g (strip);
            g.drawImage (frame, i * SubEditor::stageW, 0, SubEditor::stageW, SubEditor::H, 0, 0, SubEditor::stageW, SubEditor::H);
        }
        writePng (strip, outDir.getChildFile ("stages.png"));
        return 0;
    }

    struct Shot { float cut; int layers; float reso; const char* name; };
    const Shot shots[] = { { 20000.0f, 1, 0.0f, "0_serene" }, { 3000.0f, 2, 10.0f, "1_solemn" }, { 600.0f, 2, 30.0f, "2_faltering" },
                           { 150.0f, 3, 50.0f, "3_kneeling" }, { 20.0f, 4, 80.0f, "4_damned" } };

    for (auto& s : shots)
    {
        set (params::cutoff, s.cut);
        set (params::layers, (float) s.layers);
        set (params::reso, s.reso);

        std::unique_ptr<SubEditor> ed (static_cast<SubEditor*> (proc.createEditor()));
        ed->advanceForTest (6.0f);
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f),
                  outDir.getChildFile (juce::String ("sub_") + s.name + ".png"));
    }
    return 0;
}
