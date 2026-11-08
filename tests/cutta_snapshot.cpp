// Renders the CUtTA editor for a few factory presets into PNGs (visual check without a host).
// Usage: cutta_snapshot <outDir>
#include "../plugins/CUtTA/Source/PluginProcessor.h"
#include "../plugins/CUtTA/Source/PluginEditor.h"
#include "../plugins/CUtTA/Source/Presets.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    CuttaProcessor proc;
    const int shots[] = { 0, 1, 11 }; // INIT, KICK TIGHT, CRUSHER

    for (int idx : shots)
    {
        for (auto& [id, v] : presets::values (presets::factory[idx]))
        {
            auto* p = proc.apvts.getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (v));
        }

        std::unique_ptr<juce::Component> ed (proc.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile (juce::String ("cutta_") + juce::String (idx) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
