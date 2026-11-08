// Renders the CUtTA editor for a few factory presets into PNGs (visual check without a host).
// Usage: cutta_snapshot <outDir>         -> cutta_<preset>.png at 3x (with scanlines, like in a DAW)
//        cutta_snapshot <outDir> --web   -> preview.png (native 256x160) for the n-f.pro web preview
#include "../plugins/CUtTA/Source/PluginProcessor.h"
#include "../plugins/CUtTA/Source/PluginEditor.h"
#include "../plugins/CUtTA/Source/Presets.h"

namespace
{
void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s\n", f.getFullPathName().toRawUTF8());
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();
    const bool web = argc > 2 && juce::String (argv[2]) == "--web";

    CuttaProcessor proc;

    if (web)
    {
        proc.apvts.state.setProperty ("uiScale", 2, nullptr);
        std::unique_ptr<CuttaEditor> ed (static_cast<CuttaEditor*> (proc.createEditor()));
        ed->showScanlines = false;
        const auto big = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const int s = ed->getPixelScale();
        juce::Image out (juce::Image::RGB, CuttaEditor::W, CuttaEditor::H, false);
        for (int y = 0; y < CuttaEditor::H; ++y)
            for (int x = 0; x < CuttaEditor::W; ++x)
                out.setPixelAt (x, y, big.getPixelAt (x * s + s / 2, y * s + s / 2));
        writePng (out, outDir.getChildFile ("preview.png"));
        return 0;
    }

    const int shots[] = { 0, 1, 11 }; // INIT, KICK TIGHT, CRUSHER
    for (int idx : shots)
    {
        proc.setCurrentProgram (idx);
        std::unique_ptr<juce::Component> ed (proc.createEditor());
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f),
                  outDir.getChildFile (juce::String ("cutta_") + juce::String (idx) + ".png"));
    }
    return 0;
}
