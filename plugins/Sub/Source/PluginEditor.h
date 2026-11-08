#pragma once
#include <pixelui/PixelEditor.h>
#include <pixelui/Widgets.h>
#include "PluginProcessor.h"
#include "SubWidgets.h"

/** Sub: a demon nun that sinks from standing to kneeling, head bowed, the darker the filter gets. */
class SubEditor : public px::PixelEditor
{
public:
    explicit SubEditor (SubProcessor&);

    static constexpr int W = 288, H = 176;
    static constexpr int stageW = 138, floorY = 160, figX = 69;

    /** Test hook for the snapshot tool: advances the animation state without vsync. */
    void advanceForTest (float seconds)
    {
        for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) { time += 1.0f / 60.0f; tick (1.0f / 60.0f); }
    }

private:
    void tick (float dt) override;
    void paintScene (px::Canvas&) override;
    void paintOverlay (px::Canvas&) override;

    // scene parts
    void drawWindow (px::Canvas&);
    void drawFloor (px::Canvas&);
    void drawCandle (px::Canvas&, int x, float flick);
    void drawHalo (px::Canvas&);
    void drawBody (px::Canvas&);
    void drawHands (px::Canvas&);
    void drawRosary (px::Canvas&);
    void drawHead (px::Canvas&);
    void drawHorns (px::Canvas&);
    void drawEyeGlow (px::Canvas&);
    void drawParticles (px::Canvas&);
    void drawPlaque (px::Canvas&);
    void drawTitle (px::Canvas&);
    void drawPanel (px::Canvas&);

    // per-frame pose of the figure (logical pixels)
    struct Pose
    {
        float headX = 69, headY = 36, tilt = 0, shY = 54, shW = 15, waistY = 92, waistW = 13, hemW = 24, handsY = 70, lower = 0, hunch = 0;
    };
    float bodyHalfWidth (float y) const noexcept;
    bool  insideBody (float x, float y) const noexcept;
    px::RGB skinColour() const noexcept;
    float armsDown() const noexcept;           // 0 = praying at the chest, 1 = palms on the floor
    float handX (float side) const noexcept;
    float handY() const noexcept;

    SubProcessor& proc;

    NunWidget nun;
    ResponseView response;
    LayerSelector layerSel;
    px::Knob kCut, kRes, kDrv, kMix, kOut;

    Pose pose;
    float dark = 0.0f;          // eased darkness 0..1 (drives everything)
    float level = 0.0f;         // eased output level 0..1
    float inLevel = 0.0f;
    float lookUp = 0.0f;        // she raises her head when you hover / poke her
    float poke = 0.0f;          // click jolt 1 -> 0
    float shake = 0.0f;         // transient tremble 1 -> 0
    float gazeX = 0.0f, gazeTarget = 0.0f, wanderTimer = 1.0f;
    float blinkTimer = 3.0f, blink = 0.0f;
    float flickL = 1.0f, flickR = 1.0f, flickTimer = 0.0f, flickTargetL = 1.0f, flickTargetR = 1.0f;
    float layerShown = 2.0f;
    float sweep = -40.0f, sweepTimer = 3.0f;
    int   mood = -1; float moodFlash = 0.0f;
    float shakeX = 0.0f;

    struct Particle { float x, y, vy, phase; };
    std::array<Particle, 36> particles {};
    struct Wisp { float x, y, life; };
    std::array<Wisp, 10> wisps {};
    float wispTimer = 0.0f;

    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SubEditor)
};
