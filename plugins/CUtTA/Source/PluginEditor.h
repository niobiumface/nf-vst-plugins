#pragma once
#include <pixelui/PixelEditor.h>
#include <pixelui/Widgets.h>
#include "PluginProcessor.h"

class CuttaEditor : public px::PixelEditor
{
public:
    explicit CuttaEditor (CuttaProcessor&);
    ~CuttaEditor() override { juce::PopupMenu::dismissAllActiveMenus(); }

    static constexpr int W = 256, H = 160;

private:
    void tick (float dt) override;
    void paintScene (px::Canvas&) override;
    void paintOverlay (px::Canvas&) override;

    void drawPanel (px::Canvas&, int x, int y, int w, int h, float enabledLevel);
    void drawTitle (px::Canvas&);
    void drawScope (px::Canvas&);
    void drawParticles (px::Canvas&);
    void drawDrip (px::Canvas&);

    CuttaProcessor& proc;

    px::LedToggle gateToggle, limToggle;
    px::Knob kThr, kRng, kAtk, kHld, kRel, kDrv, kCeil, kLRel;
    px::VMeter inMeter, grMeter, outMeter;
    px::Selector presetSel;
    juce::LookAndFeel_V4 menuLnf;
    void showPresetMenu();

    // scope history (fixed rate, independent of frame rate)
    static constexpr int scopeX = 11, scopeY = 62, scopeW = 136, scopeH = 44;
    std::array<float, scopeW - 2> histLevel {}, histGate {};
    int histPos = 0;
    float histAccum = 0.0f, pendingPeak = 0.0f;

    // animated state
    float gateShown = 0.0f, gateOpenShown = 0.0f, limActivity = 0.0f, flicker = 1.0f, flickerTarget = 1.0f, flickerTimer = 0.0f;
    float grDbShown = 0.0f;
    float sweep = -40.0f, sweepTimer = 3.0f;

    struct Particle { float x, y, vx, vy, life, phase; bool ember; };
    std::array<Particle, 40> particles {};

    struct Drip { enum State { idle, forming, falling, splash } state = idle; float x = 0, y = 0, vy = 0, t = 0, timer = 4.0f; };
    Drip drip;
    struct Spark { float x, y, vx, vy, life; };
    std::array<Spark, 6> sparks {};

    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CuttaEditor)
};
