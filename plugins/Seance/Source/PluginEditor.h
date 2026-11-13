#pragma once
#include <pixelui/PixelEditor.h>
#include <pixelui/Widgets.h>
#include "PluginProcessor.h"
#include "SeanceWidgets.h"

/** Séance: a talking board on a dark table between two candles. Every time a repeat comes back out of the delay
    (an onset on the wet signal), the planchette jumps to the next letter of a word and leaves a ghost behind that
    fades - the ghosts fade one after another like the echoes. Feedback = how long the ghosts linger,
    wow/flutter = the trembling hand (and the letters waver), the filters = how sharp or smoky the ghosts are,
    ping-pong = which candle flares, the wet level = the ectoplasm fog rising from the board's bottom.
    When the echo train has died away, the planchette slides to GOOD BYE. Above 100 % feedback the board shakes. */
class SeanceEditor : public px::PixelEditor
{
public:
    explicit SeanceEditor (SeanceProcessor&);

    static constexpr int W = 320, H = 184;

    /** Test hook for the snapshot tool: advances widgets and animation without vsync. */
    void advanceForTest (float seconds);

private:
    void tick (float dt) override;
    void paintScene (px::Canvas&) override;
    void paintOverlay (px::Canvas&) override;
    void showPresetMenu();
    std::array<px::Widget*, 11> allWidgets();

    // scene parts
    void drawTable (px::Canvas&);
    void drawBoard (px::Canvas&);
    void drawLetters (px::Canvas&);
    void drawCandles (px::Canvas&);
    void drawFog (px::Canvas&);
    void drawGhosts (px::Canvas&);
    void drawPlanchette (px::Canvas&);
    void drawMotes (px::Canvas&);
    void drawPanel (px::Canvas&);
    void drawRepeats (px::Canvas&);

    void onEcho (int sides, float level);
    void hopTo (int spot, float height, float duration);
    int nextTarget();
    float rnd() noexcept; // 0..1, cheap xorshift

    // board geometry (logical pixels)
    static constexpr int boardX = 6, boardY = 13, boardW = 308, boardH = 112;
    static constexpr int numSpots = 39; // A-Z, 1-9 0, YES, NO, GOOD BYE
    static constexpr int spotYes = 36, spotNo = 37, spotBye = 38;
    static constexpr int pW = 17, pH = 17, lensX = 8, lensY = 6; // planchette glyph and the lens centre in it

    struct Spot { std::string text; int cx = 0, cy = 0, scale = 2; };
    std::array<Spot, numSpots> spots;

    SeanceProcessor& proc;

    BoardHandle board;
    px::Selector presetSel;
    px::LedToggle syncT, pingT, bypass;
    px::Knob kTime, kDiv, kFb, kWow, kLow, kHigh, kMix;
    SwapWidget timeSlot;

    // eased parameter / meter state
    float fb = 0.45f, wow = 0.25f, lowN = 0.0f, highN = 1.0f, delayMs = 375.0f;
    float wetLevel = 0.0f, inLevel = 0.0f;
    bool ping = false;

    // planchette
    float pxNow = 160.0f, pyNow = 74.0f;   // lens position on the board (no wobble)
    float fromX = 160.0f, fromY = 74.0f, toX = 160.0f, toY = 74.0f;
    float hopT = 1.0f, hopDur = 0.15f, hopH = 0.0f, lift = 0.0f;
    bool gliding = false;
    int atSpot = -1;

    // echo train
    uint32_t seenEchoes[2] { 0, 0 }, seenOnsets = 0;
    int repeatIdx = 0;       // repeats in the current train (0 = idle)
    float sinceEcho = 99.0f, atByeFor = 0.0f, lastLevel = 0.0f;
    int wordIdx = 0, letterIdx = 0;

    struct Ghost { float x = 0, y = 0, a = 0, life = 0, maxLife = 1, phase = 0; };
    std::array<Ghost, 32> ghosts {};
    std::array<float, numSpots> spotGlow {};
    std::array<float, 12> barFlash {};
    float flare[2] { 0.0f, 0.0f };
    float shake = 0.0f;
    int shakeX = 0, shakeY = 0;
    float shakeAcc = 0.0f;

    struct Mote { float x = 0, y = 0, phase = 0, side = 0; };
    std::array<Mote, 14> motes {};

    // static art, built once
    std::vector<px::RGB> wood;          // board base colours (boardW x boardH)
    std::vector<float> candleLight[2];  // light falloff per candle over the board
    std::array<std::array<char, pW>, pH> pMask {};

    uint32_t rngState = 0x6d2b79f5u;
    juce::Random random;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeanceEditor)
};
