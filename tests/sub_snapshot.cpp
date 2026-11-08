// Renders the Sub editor at several darkness levels into PNGs (visual check without a host).
// Usage: sub_snapshot <outDir>
#include "../plugins/Sub/Source/PluginProcessor.h"
#include "../plugins/Sub/Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    struct Shot { float cut; int layers; float reso; const char* name; };
    const Shot shots[] = { { 20000.0f, 1, 0.0f, "0_serene" }, { 3000.0f, 2, 10.0f, "1_solemn" }, { 600.0f, 2, 30.0f, "2_faltering" },
                           { 150.0f, 3, 50.0f, "3_kneeling" }, { 20.0f, 4, 80.0f, "4_damned" } };

    SubProcessor proc;
    auto set = [&] (const char* id, float v)
    {
        auto* p = proc.apvts.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };

    for (auto& s : shots)
    {
        set (params::cutoff, s.cut);
        set (params::layers, (float) s.layers);
        set (params::reso, s.reso);

        std::unique_ptr<SubEditor> ed (static_cast<SubEditor*> (proc.createEditor()));
        ed->advanceForTest (6.0f);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto f = outDir.getChildFile (juce::String ("sub_") + s.name + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%s\n", f.getFullPathName().toRawUTF8());
    }
    return 0;
}
