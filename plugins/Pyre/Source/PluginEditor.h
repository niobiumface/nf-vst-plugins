#pragma once
#include <pixelui/PixelEditor.h>
#include <pixelui/Widgets.h>
#include "PluginProcessor.h"
#include "PyreWidgets.h"

/** Pyre: a pyre of logs around a stake with a skull. The flame height is the level hitting the shaper
    (how far into the curve you drive), the bias is the wind, the tone colours the fire, the mode changes
    how it burns (tube: licking, tape: low and smoky, fuzz: jagged and spitting sparks, fold: tearing into blobs). */
class PyreEditor : public px::PixelEditor
{
public:
    explicit PyreEditor (PyreProcessor&);

    static constexpr int W = 320, H = 184;
    static constexpr int stageW = 152;

    /** Test hook for the snapshot tool: advances widgets and animation without vsync. */
    void advanceForTest (float seconds);

private:
    void tick (float dt) override;
    void paintScene (px::Canvas&) override;
    void paintOverlay (px::Canvas&) override;
    void showPresetMenu();
    std::array<px::Widget*, 10> allWidgets();

    // scene parts
    void drawSky (px::Canvas&);
    void drawSmoke (px::Canvas&);
    void drawGround (px::Canvas&);
    void drawStake (px::Canvas&);
    void drawFire (px::Canvas&);
    void drawLogs (px::Canvas&);
    void drawSparks (px::Canvas&);
    void drawTitle (px::Canvas&);
    void drawPanel (px::Canvas&);
    void drawCurve (px::Canvas&);
    void drawHarmonics (px::Canvas&);
    void drawMeters (px::Canvas&);

    void stepFire();
    void spawnSparks (int count, bool burst);
    void computeHarmonics();
    float rnd() noexcept; // 0..1, cheap xorshift (fire sim)

    // fire grid (logical pixels)
    static constexpr int fireW = 100, fireH = 124, fireX = 26, fireBottom = 150;
    static constexpr int centreX = 76;
    static constexpr float fireRate = 75.0f; // simulation steps per second
    static constexpr int numHarm = 9;         // H2..H10

    PyreProcessor& proc;

    FireHandle fireHandle;
    px::Selector presetSel;
    px::Choice cMode;
    px::LedToggle autoT, bypass;
    px::Knob kDrv, kBias, kTone, kMix, kOut;

    // eased state
    float heat = 0.0f;       // 0..1 level into the shaper (-30..+18 dB re. the knee)
    float slowHeat = 0.0f;   // for transient flares
    float charAmt = 0.0f;    // logs blacken under sustained fire
    float inLevel = 0.0f;    // linear input peak (curve marker, harmonics)
    float driveDb = 9.0f, bias = 0.0f, tone = 0.0f, mixAmt = 1.0f;
    float poke = 0.0f;
    int mode = 0;

    struct Meter
    {
        float shown = 0.0f, peak = 0.0f, hold = 0.0f, value = -100.0f;
        void feed (float norm, float v, float dt, float attack, float release) noexcept;
    };
    Meter mIn, mHeat, mOut;

    std::array<float, (size_t) (fireW * fireH)> cells {};
    static constexpr int coolH = 128; // power of two
    std::array<float, (size_t) (fireW * coolH)> coolMap {};
    int coolScroll = 0, coolShift = 0;
    float fireAcc = 0.0f;
    uint32_t rngState = 0x2545f491u;

    struct Spark { float x = 0, y = 0, vx = 0, vy = 0, life = 0, maxLife = 1; };
    std::array<Spark, 64> sparks {};
    struct Puff { float x = 0, y = 0, vx = 0, vy = 0, r = 0, life = 0, maxLife = 1; };
    std::array<Puff, 40> smoke {};
    struct Flake { float x = 0, y = 0, phase = 0; };
    std::array<Flake, 18> ash {};
    float sparkAcc = 0.0f, smokeAcc = 0.0f;

    std::array<float, numHarm> harmDb {}, harmShown {};
    bool harmPreview = true;

    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PyreEditor)
};
