#pragma once
#include <pixelui/PixelEditor.h>
#include <pixelui/Widgets.h>
#include "PluginProcessor.h"
#include "IronWidgets.h"

/** Iron S: a steam iron on an ironing board. The cloth's wrinkles are the detected sibilance, the height of
    the iron above the board is the threshold - whatever sticks out gets pressed flat (and steams). */
class IronSEditor : public px::PixelEditor
{
public:
    explicit IronSEditor (IronSProcessor&);

    static constexpr int W = 336, H = 192;
    static constexpr int stageW = 152;

    /** Test hook for the snapshot tool: advances widgets and animation without vsync. */
    void advanceForTest (float seconds);

private:
    void tick (float dt) override;
    void paintScene (px::Canvas&) override;
    void paintOverlay (px::Canvas&) override;
    void showPresetMenu();

    // scene parts
    void drawRoom (px::Canvas&);
    void drawCord (px::Canvas&);
    void drawBoard (px::Canvas&);
    void drawCloth (px::Canvas&);
    void drawIron (px::Canvas&);
    void drawSteam (px::Canvas&);
    void drawRuler (px::Canvas&);
    void drawPlaque (px::Canvas&);
    void drawTitle (px::Canvas&);
    void drawPanel (px::Canvas&);
    void drawMeters (px::Canvas&);

    // scene geometry (logical pixels)
    static constexpr int boardY = 140, floorY = 178, clothThick = 3;
    static constexpr int ironW = 58, ironH = 32, ironRestX = 62;
    static constexpr float maxWrinkle = 24.0f; // wrinkle height in pixels at 0 dB
    static constexpr int histLen = 64;
    static constexpr float scrollSpeed = 24.0f; // cloth pixels per second

    /** Wrinkle height of a detector level: 0 dB = maxWrinkle, levelFloor = flat. */
    float heightPx (float levelDb) const noexcept
    {
        return juce::jlimit (0.0f, 1.0f, 1.0f - levelDb / levelFloor) * maxWrinkle;
    }
    static int ironMaterial (int u, int v) noexcept;
    float ridgeAt (int x) const noexcept;
    float clothHeight (int x) const noexcept;
    float ghostHeight (int x) const noexcept;
    int ironX() const noexcept { return ironRestX + ironDx; }
    int ironY() const noexcept { return boardY - clothThick - (int) std::lround (heightPx (thr)) - ironH; }
    void spawnSteam (int count, bool burst);

    IronSProcessor& proc;

    IronHandle iron;
    SpectrumView spectrum;
    px::Selector presetSel;
    px::Choice cMode, cDetect, cCut, cListen;
    px::LedToggle bypass;
    px::Knob kThr, kRng, kFrq, kWid, kSpd, kSmk, kLook, kLink, kOut;

    // eased state
    float thr = -14.0f, rangeShown = 9.0f;
    float levelFloor = -36.0f; // bottom of the wrinkle scale: -36 dB in relative, -60 dB in absolute mode
    float live = -60.0f;       // detector level, dB
    float grNow = 0.0f;        // de-ess reduction, dB
    float smackNow = 0.0f;     // smack reduction, dB
    float act = 0.0f;          // 0..1 how hard the iron is working
    float heat = 0.0f;         // slower: glow of the sole
    float hover = 0.0f, poke = 0.0f;
    int   ironDx = 0;

    struct Meter
    {
        float shown = 0.0f, peak = 0.0f, hold = 0.0f, peakValue = 0.0f;
        void feed (float norm, float value, float dt, float attack, float release) noexcept;
    };
    Meter mIn, mGr, mSmack, mOut;

    struct Column { float level = -60.0f, red = 0.0f; };
    std::array<Column, histLen> hist {};
    float scroll = 0.0f, pendLevel = -60.0f, pendRed = 0.0f;
    int scrollInt = 0;

    struct Puff { float x = 0, y = 0, vx = 0, vy = 0, life = 0; };
    std::array<Puff, 56> steam {};
    std::array<Puff, 16> sparks {};
    float steamAcc = 0.0f, sparkCooldown = 0.0f;

    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IronSEditor)
};
