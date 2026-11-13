#include "PluginEditor.h"
#include <cstring>

using namespace px;

namespace
{
// planchette: heart shape pointing up, '#' wood, 'r' rim, 'O' glass lens
const char* const planchette[] = {
    "........#........",
    ".......###.......",
    "......#####......",
    ".....##rrr##.....",
    "....##rOOOr##....",
    "...##rOOOOOr##...",
    "...##rOOOOOr##...",
    "..###rOOOOOr###..",
    "..####rOOOr####..",
    ".######rrr######.",
    ".###############.",
    "#################",
    "#################",
    "#################",
    ".#######.#######.",
    "..#####...#####..",
    "...###.....###...",
};

const char* const words[] = { "ECHO", "HELLO", "AGAIN", "BEYOND", "REPEAT", "RETURN", "LISTEN",
                              "HAUNT", "SPIRIT", "FADE", "REMAIN", "ANSWER", "WHO", "STAY" };
constexpr int numWords = (int) (sizeof (words) / sizeof (words[0]));

constexpr int candleX[2] = { 22, 297 };
constexpr int candleTop = 104, candleBottom = 115;
constexpr int restX = 160, restY = 76;

constexpr int panelY = 127;
constexpr int rpX = 242, rpY = 130, rpW = 74, rpH = 35;
constexpr int readoutY = 175;

constexpr RGB gilt = 0x7d6c4e, giltLit = 0xc8b388, carveShadow = 0x070504;
constexpr RGB warm = 0x3e2817;
constexpr RGB plBody = 0x9c8a6c, plGrain = 0x8c7a5e, plHi = 0xcdbd9c, plLo = 0x5a4c3a, plRim = 0x2b241c;

float gainToDb (float g) noexcept { return juce::Decibels::gainToDecibels (g, -100.0f); }

uint32_t hash (int x, int y) noexcept
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float smooth01 (float x) noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    return x * x * (3.0f - 2.0f * x);
}

bool boardCorner (int x, int y) noexcept // cut corners (local board coordinates)
{
    constexpr int w = 308, h = 112;
    const int cx = std::min (x, w - 1 - x), cy = std::min (y, h - 1 - y);
    return cx + cy < 3;
}
} // namespace

//==============================================================================
SeanceEditor::SeanceEditor (SeanceProcessor& p)
    : PixelEditor (p, W, H, (int) p.apvts.state.getProperty ("uiScale", 3)),
      proc (p),
      board (*p.apvts.getParameter (params::time), *p.apvts.getParameter (params::division),
             *p.apvts.getParameter (params::feedback), *p.apvts.getParameter (params::sync)),
      syncT  (*p.apvts.getParameter (params::sync), "SYNC", seancepal::ecto),
      pingT  (*p.apvts.getParameter (params::pingPong), "PING", seancepal::ecto),
      bypass (*p.apvts.getParameter (params::bypass), "BYP", seancepal::ecto),
      kTime (*p.apvts.getParameter (params::time),     "TIME",  "TIME",        "MS", seancepal::ecto),
      kDiv  (*p.apvts.getParameter (params::division), "NOTE",  "NOTE",        "",   seancepal::ecto),
      kFb   (*p.apvts.getParameter (params::feedback), "FDBK",  "FEEDBACK",    "%",  seancepal::ecto),
      kWow  (*p.apvts.getParameter (params::wow),      "WOW",   "WOW/FLUTTER", "%",  seancepal::ecto),
      kLow  (*p.apvts.getParameter (params::lowCut),   "LOCUT", "LOW CUT",     "HZ", seancepal::ecto),
      kHigh (*p.apvts.getParameter (params::highCut),  "HICUT", "HIGH CUT",    "HZ", seancepal::ecto),
      kMix  (*p.apvts.getParameter (params::mix),      "MIX",   "MIX",         "%",  seancepal::ecto),
      timeSlot (kTime, kDiv, [this] { return syncT.isOn(); })
{
    board.bounds     = { boardX, boardY, boardW, boardH };
    presetSel.bounds = { 172, 3, 128, 7 };
    timeSlot.bounds  = { 4, 130, 32, 32 };
    syncT.bounds  = { 206, 134, 26, 5 };
    pingT.bounds  = { 206, 145, 26, 5 };
    bypass.bounds = { 206, 156, 22, 5 };

    px::Knob* knobs[] = { &kTime, &kDiv, &kFb, &kWow, &kLow, &kHigh, &kMix };
    for (int i = 0; i < 7; ++i)
    {
        knobs[i]->hot = seancepal::hot;
        knobs[i]->unlit = seancepal::dim;
        if (i >= 2) knobs[i]->bounds = { 4 + (i - 1) * 33, 130, 32, 32 };
    }

    presetSel.accent = seancepal::ecto;
    presetSel.getText = [this]
    {
        return proc.getProgramName (proc.getCurrentProgram()) + (proc.isPresetModified() ? "*" : "");
    };
    presetSel.onStep = [this] (int dir)
    {
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + presets::count) % presets::count);
    };
    presetSel.onClick = [this] { showPresetMenu(); };

    board.onPoke = [this] { flare[0] = flare[1] = std::max (flare[0], 0.6f); };

    for (auto* w : allWidgets())
        addWidget (*w);

    // --- spots on the board
    for (int i = 0; i < 13; ++i)
    {
        const float dx = (float) ((i - 6) * 20);
        const float k = (dx / 120.0f) * (dx / 120.0f);
        spots[(size_t) i]      = { std::string (1, (char) ('A' + i)),      160 + (int) dx, 41 + (int) std::lround (9.0f * k), 2 };
        spots[(size_t) i + 13] = { std::string (1, (char) ('N' + i)),      160 + (int) dx, 62 + (int) std::lround (7.0f * k), 2 };
    }
    for (int i = 0; i < 10; ++i)
        spots[(size_t) (26 + i)] = { std::string (1, (char) (i == 9 ? '0' : '1' + i)), 160 + (int) std::lround (((float) i - 4.5f) * 15.0f), 87, 2 };
    spots[spotYes] = { "YES", 36, 25, 2 };
    spots[spotNo]  = { "NO", 284, 25, 2 };
    spots[spotBye] = { "GOOD BYE", 160, 109, 2 };

    // --- static art: wood grain
    wood.resize ((size_t) (boardW * boardH));
    for (int y = 0; y < boardH; ++y)
        for (int x = 0; x < boardW; ++x)
        {
            const float g = std::sin (((float) y + 2.5f * std::sin ((float) x * 0.021f + (float) y * 0.13f)) * 0.85f
                                      + std::sin ((float) x * 0.004f) * 3.0f);
            RGB col = mix (0x150e0b, 0x21160f, 0.5f + 0.5f * g);
            const auto h = hash (x, y);
            if (h % 11u == 0u) col = scale (col, 0.8f);
            else if (h % 37u == 0u) col = mix (col, 0x2c1e14, 0.6f);
            wood[(size_t) (y * boardW + x)] = col;
        }
    for (int k = 0; k < 2; ++k)
    {
        candleLight[k].resize ((size_t) (boardW * boardH));
        for (int y = 0; y < boardH; ++y)
            for (int x = 0; x < boardW; ++x)
            {
                const float dx = (float) (boardX + x - candleX[k]), dy = (float) (boardY + y - (candleTop - 4)) * 1.15f;
                const float d = std::sqrt (dx * dx + dy * dy) / 120.0f;
                candleLight[k][(size_t) (y * boardW + x)] = d < 1.0f ? (1.0f - d) * (1.0f - d) : 0.0f;
            }
    }
    for (int j = 0; j < pH; ++j)
        for (int i = 0; i < pW; ++i)
            pMask[(size_t) j][(size_t) i] = planchette[j][i];

    for (auto& m : motes)
        m = { 0.0f, 0.0f, random.nextFloat() * 6.28f, random.nextBool() ? 1.0f : 0.0f };
    for (auto& m : motes)
    {
        m.x = (float) candleX[(int) m.side] + (random.nextFloat() - 0.5f) * 50.0f;
        m.y = 40.0f + random.nextFloat() * 70.0f;
    }

    auto& ap = p.apvts;
    fb = ap.getRawParameterValue (params::feedback)->load() * 0.01f;
    wow = ap.getRawParameterValue (params::wow)->load() * 0.01f;
    delayMs = p.targetDelayMs();
    for (int c = 0; c < 2; ++c) seenEchoes[c] = p.meters.echoes[c].load();
    seenOnsets = p.meters.inOnsets.load();
    wordIdx = random.nextInt (numWords);

    onScaleChanged = [this] (int s) { proc.apvts.state.setProperty ("uiScale", s, nullptr); };
}

std::array<px::Widget*, 11> SeanceEditor::allWidgets()
{
    return { &board, &presetSel, &syncT, &pingT, &bypass, &timeSlot, &kFb, &kWow, &kLow, &kHigh, &kMix };
}

void SeanceEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("PRESETS");
    for (int i = 0; i < presets::count; ++i)
        menu.addItem (i + 1, presets::factory[i].name, true, i == proc.getCurrentProgram());

    const auto sb = presetSel.bounds * getPixelScale();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (sb)),
                        [safe = juce::Component::SafePointer<SeanceEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                            {
                                safe->proc.setCurrentProgram (result - 1);
                                safe->presetSel.flashNow();
                            }
                        });
}

void SeanceEditor::advanceForTest (float seconds)
{
    constexpr float dt = 1.0f / 60.0f;
    for (float t = 0.0f; t < seconds; t += dt)
    {
        time += dt;
        for (auto* w : allWidgets()) w->update (dt);
        tick (dt);
    }
}

float SeanceEditor::rnd() noexcept
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (float) (rngState >> 8) * (1.0f / 16777216.0f);
}

//==============================================================================
int SeanceEditor::nextTarget()
{
    const char* w = words[wordIdx];
    if (letterIdx < (int) std::strlen (w))
        return w[letterIdx++] - 'A';
    // the word is spelled: count the repeats on the numbers row
    const int d = repeatIdx % 10;
    return d == 0 ? 35 : 26 + d - 1;
}

void SeanceEditor::hopTo (int spot, float height, float duration)
{
    fromX = pxNow;
    fromY = pyNow;
    if (spot >= 0)
    {
        toX = (float) spots[(size_t) spot].cx;
        toY = (float) spots[(size_t) spot].cy;
    }
    else
    {
        toX = (float) restX;
        toY = (float) restY;
    }
    atSpot = spot;
    hopT = 0.0f;
    hopH = height;
    hopDur = std::max (0.02f, duration);
    gliding = height <= 0.0f;
}

void SeanceEditor::onEcho (int sides, float level)
{
    const float levelN = juce::jlimit (0.0f, 1.0f, (gainToDb (level) + 48.0f) / 42.0f);
    const float T = delayMs * 0.001f;

    // the repeat the planchette is leaving stays behind as a ghost and fades
    if (repeatIdx > 0)
        for (auto& g : ghosts)
            if (g.life <= 0.0f)
            {
                g.x = pxNow; g.y = pyNow;
                g.a = 0.35f + 0.65f * lastLevel;
                g.maxLife = g.life = juce::jlimit (0.5f, 5.0f, T * (1.5f + 7.0f * std::min (fb, 1.0f)) + 0.3f);
                g.phase = rnd() * 6.28f;
                break;
            }

    ++repeatIdx;
    const int target = nextTarget();
    hopTo (target, 2.0f + 8.0f * levelN, juce::jlimit (0.05f, 0.2f, T * 0.5f));
    spotGlow[(size_t) target] = 0.4f + 0.6f * levelN;
    barFlash[(size_t) std::min (repeatIdx, 12) - 1] = 1.0f;
    for (int s = 0; s < 2; ++s)
        if (sides & (1 << s)) flare[s] = std::max (flare[s], 0.4f + 0.6f * levelN);
    sinceEcho = 0.0f;
    lastLevel = levelN;
}

void SeanceEditor::tick (float dt)
{
    auto& ap = proc.apvts;
    fb   += (ap.getRawParameterValue (params::feedback)->load() * 0.01f - fb) * ease (dt, 10.0f);
    wow  += (ap.getRawParameterValue (params::wow)->load() * 0.01f - wow) * ease (dt, 8.0f);
    lowN += (ap.getParameter (params::lowCut)->getValue() - lowN) * ease (dt, 8.0f);
    highN += (ap.getParameter (params::highCut)->getValue() - highN) * ease (dt, 8.0f);
    ping = pingT.isOn();
    delayMs = proc.meters.delayMs.load (std::memory_order_relaxed);
    const float T = delayMs * 0.001f;

    // --- audio
    const float inLin  = proc.meters.inPeak.exchange (0.0f);
    const float wetLin = proc.meters.wetPeak.exchange (0.0f);
    const float wetN = juce::jlimit (0.0f, 1.0f, (gainToDb (wetLin) + 54.0f) / 54.0f);
    wetLevel += (wetN - wetLevel) * ease (dt, wetN > wetLevel ? 12.0f : 2.0f);
    inLevel += (juce::jlimit (0.0f, 1.0f, (gainToDb (inLin) + 54.0f) / 54.0f) - inLevel) * ease (dt, 10.0f);

    int sides = 0;
    float level = 0.0f;
    for (int c = 0; c < 2; ++c)
    {
        const uint32_t n = proc.meters.echoes[c].load (std::memory_order_relaxed);
        const float lv = proc.meters.echoLevel[c].exchange (0.0f);
        if (n != seenEchoes[c]) { sides |= 1 << c; level = std::max (level, lv); }
        seenEchoes[c] = n;
    }
    if (! ping && sides != 0) sides = 3; // without ping-pong both candles answer
    if (sides != 0) onEcho (sides, level);

    const uint32_t ons = proc.meters.inOnsets.load (std::memory_order_relaxed);
    if (ons != seenOnsets && repeatIdx == 0 && hopT >= 1.0f)
    {
        // somebody asked a question: the planchette twitches in place
        const int keep = atSpot;
        hopTo (keep, 2.0f, 0.12f);
        toX = fromX; toY = fromY;
    }
    seenOnsets = ons;

    // --- the train has died away: say good bye, later drift back to the centre
    sinceEcho += dt;
    if (repeatIdx > 0 && sinceEcho > std::max (0.7f, 2.5f * T + 0.25f))
    {
        for (auto& g : ghosts)
            if (g.life <= 0.0f)
            {
                g.x = pxNow; g.y = pyNow; g.a = 0.35f + 0.65f * lastLevel;
                g.maxLife = g.life = 1.2f; g.phase = rnd() * 6.28f;
                break;
            }
        repeatIdx = 0;
        letterIdx = 0;
        wordIdx = (wordIdx + 1 + (int) (rnd() * (float) (numWords - 1))) % numWords;
        hopTo (spotBye, 0.0f, 1.1f);
        atByeFor = 0.0f;
    }
    if (repeatIdx == 0 && atSpot == spotBye && hopT >= 1.0f)
        if ((atByeFor += dt) > 2.5f)
            hopTo (-1, 0.0f, 1.6f);

    // --- planchette motion
    if (hopT < 1.0f)
    {
        hopT = std::min (1.0f, hopT + dt / hopDur);
        const float e = gliding ? smooth01 (hopT) : 1.0f - (1.0f - hopT) * (1.0f - hopT) * (1.0f - hopT);
        pxNow = fromX + (toX - fromX) * e;
        pyNow = fromY + (toY - fromY) * e;
        lift = gliding ? 0.0f : std::sin (3.14159265f * hopT) * hopH;
    }
    else
    {
        lift = 0.0f;
    }

    // --- ghosts, glows, candles
    for (auto& g : ghosts)
        if (g.life > 0.0f) g.life -= dt;
    const float glowDecay = 0.5f + 3.0f * (1.0f - std::min (fb, 1.0f));
    for (int i = 0; i < numSpots; ++i)
    {
        auto& gl = spotGlow[(size_t) i];
        const float floorGlow = (i == atSpot && repeatIdx > 0) ? 0.25f : 0.0f;
        gl = std::max (floorGlow, gl - dt * glowDecay * gl - dt * 0.05f);
    }
    for (auto& b : barFlash) b = std::max (0.0f, b - dt * 2.5f);
    for (auto& f : flare) f = std::max (0.0f, f - dt * 3.0f);

    // above 100 % feedback the board is possessed: it shakes and letters flare up by themselves
    shake = juce::jlimit (0.0f, 1.0f, (fb - 1.0f) / 0.08f) * (0.4f + 0.6f * wetLevel);
    shakeAcc += dt;
    if (shakeAcc > 1.0f / 24.0f)
    {
        shakeAcc = 0.0f;
        shakeX = rnd() < shake * 0.7f ? (rnd() < 0.5f ? -1 : 1) : 0;
        shakeY = rnd() < shake * 0.5f ? (rnd() < 0.5f ? -1 : 1) : 0;
        if (rnd() < shake * 0.5f) spotGlow[(size_t) (rnd() * 26.0f) % 26] = 0.6f;
    }

    for (auto& m : motes)
    {
        const float cx = (float) candleX[(int) m.side];
        m.y -= (2.0f + std::sin (time * 0.7f + m.phase)) * dt;
        m.x += std::sin (time * 0.9f + m.phase * 3.0f) * 3.0f * dt;
        if (m.y < 30.0f || std::abs (m.x - cx) > 40.0f)
        {
            m.side = rnd() < 0.5f ? 0.0f : 1.0f;
            m.x = (float) candleX[(int) m.side] + (rnd() - 0.5f) * 40.0f;
            m.y = 95.0f + rnd() * 15.0f;
        }
    }
}

//==============================================================================
void SeanceEditor::paintScene (Canvas& c)
{
    drawTable (c);
    drawBoard (c);
    drawLetters (c);
    drawFog (c);
    drawCandles (c);
    drawMotes (c);
    drawGhosts (c);
    drawPlanchette (c);
    drawPanel (c);
    drawRepeats (c);
}

void SeanceEditor::paintOverlay (Canvas& c)
{
    std::string txt;
    RGB col = pal::bone;
    if (auto* w = getActiveWidget())
        txt = w->getReadout().toUpperCase().toStdString();
    if (txt.empty())
    {
        txt = bypass.isOn() ? "BYPASSED" : "DRAG THE BOARD: TIME / FEEDBACK";
        col = pal::smoke;
    }
    font::drawRight (c, W - 4, readoutY, txt, col);
}

void SeanceEditor::drawTable (Canvas& c)
{
    for (int y = 0; y < panelY; ++y)
        for (int x = 0; x < W; ++x)
            c.set (x, y, hash (x, y) % 9u == 0u ? (RGB) 0x0a090c : pal::voidC);
    font::drawText (c, 8, 4, "TAPE DELAY", pal::smoke);
}

void SeanceEditor::drawBoard (Canvas& c)
{
    const float fl[2] = {
        (0.8f + 0.2f * std::sin (time * 9.0f) * std::sin (time * 3.7f)) * (1.0f + flare[0] * 0.8f) * (0.7f + 0.3f * inLevel),
        (0.8f + 0.2f * std::sin (time * 8.1f + 1.0f) * std::sin (time * 4.3f)) * (1.0f + flare[1] * 0.8f) * (0.7f + 0.3f * inLevel)
    };
    const float plx = pxNow, ply = pyNow;
    const float ectoGlow = 0.25f + 0.6f * lastLevel * std::max (0.0f, 1.0f - sinceEcho * 1.5f);

    for (int y = 0; y < boardH; ++y)
        for (int x = 0; x < boardW; ++x)
        {
            if (boardCorner (x, y)) continue;
            const int ax = boardX + x + shakeX, ay = boardY + y + shakeY;
            const auto idx = (size_t) (y * boardW + x);
            RGB col = wood[idx];

            // frame: bevel and two inlay lines
            const int edge = std::min ({ x, y, boardW - 1 - x, boardH - 1 - y });
            if (edge == 0)      col = (x == 0 || y == 0) ? (RGB) 0x3a2a1e : (RGB) 0x0b0706;
            else if (edge == 1) col = mix (col, 0x0b0706, 0.4f);
            else if (edge == 4) col = ((x + y) & 1) ? (RGB) 0x4a3b28 : (RGB) 0x3a2e20;
            else if (edge == 6) col = 0x281d14;

            // candle light, dithered
            const float light = candleLight[0][idx] * fl[0] + candleLight[1][idx] * fl[1];
            if (Canvas::bayer (ax, ay, light * 0.75f)) col = mix (col, warm, 0.55f + 0.3f * std::min (1.0f, light));

            // ectoplasmic glow around the planchette
            const float dx = (float) ax - plx, dy = ((float) ay - ply) * 1.2f;
            const float d2 = dx * dx + dy * dy;
            if (d2 < 26.0f * 26.0f)
            {
                const float g = 1.0f - std::sqrt (d2) / 26.0f;
                if (Canvas::bayer (ax, ay, g * g * ectoGlow)) col = mix (col, seancepal::dim, 0.75f);
            }
            c.set (ax, ay, col);
        }

    // title, carved: SÉANCE
    const int tx = 160 - font::textWidth ("SEANCE", 2) / 2 + shakeX, ty = 21 + shakeY;
    font::drawText (c, tx + 1, ty + 1, "SEANCE", carveShadow, 2);
    font::drawText (c, tx, ty, "SEANCE", mix (gilt, giltLit, 0.55f), 2);
    const int ex = tx + 8; // the E
    for (auto [ax, ay] : { std::pair { 2, -2 }, { 3, -2 }, { 3, -3 }, { 4, -3 } })
        c.set (ex + ax, ty + ay, seancepal::ecto);

    // sun (YES side) and moon (NO side)
    {
        const int sx = 34 + shakeX, sy = 37 + shakeY;
        c.disc (sx, sy, 2.2f, mix (gilt, giltLit, 0.4f));
        for (auto [dx, dy] : { std::pair { 0, -4 }, { 0, 4 }, { -4, 0 }, { 4, 0 }, { 3, 3 }, { -3, -3 }, { 3, -3 }, { -3, 3 } })
            c.set (sx + dx, sy + dy, gilt);
        const int mx = 286 + shakeX, my = 37 + shakeY;
        for (int y = -3; y <= 3; ++y)
            for (int x = -3; x <= 3; ++x)
            {
                const bool in = x * x + y * y <= 10, cut = (x - 2) * (x - 2) + (y + 1) * (y + 1) <= 7;
                if (in && ! cut) c.set (mx + x, my + y, mix (gilt, giltLit, 0.3f));
            }
    }
}

void SeanceEditor::drawLetters (Canvas& c)
{
    const float wob = std::max (0.0f, wow - 0.3f) * 2.0f; // warped tape: the letters waver
    for (int i = 0; i < numSpots; ++i)
    {
        const auto& s = spots[(size_t) i];
        const int w = font::textWidth (s.text, s.scale), h = 5 * s.scale;
        const int oy = (int) std::lround (wob * std::sin (time * 2.3f + (float) i * 0.8f));
        const int x = s.cx - w / 2 + shakeX, y = s.cy - h / 2 + oy + shakeY;

        const int lx = juce::jlimit (0, boardW - 1, s.cx - boardX), ly = juce::jlimit (0, boardH - 1, s.cy - boardY);
        const auto li = (size_t) (ly * boardW + lx);
        const float light = std::min (1.0f, candleLight[0][li] + candleLight[1][li]);
        const float g = spotGlow[(size_t) i];

        if (g > 0.02f) // halo
            for (int j = -3; j < h + 3; ++j)
                for (int k = -3; k < w + 3; ++k)
                    if (Canvas::bayer (x + k, y + j, g * 0.35f)) c.blend (x + k, y + j, seancepal::ecto, 0.25f * g);

        font::drawText (c, x + 1, y + 1, s.text, carveShadow, s.scale);
        const RGB base = mix (gilt, giltLit, 0.25f + 0.6f * light);
        font::drawText (c, x, y, s.text, mix (base, g > 0.6f ? seancepal::hot : seancepal::ecto, std::min (1.0f, g * 1.3f)), s.scale);
    }
}

void SeanceEditor::drawFog (Canvas& c)
{
    // ectoplasm rising from the bottom of the board: its height is the level of the repeats
    const float hgt = wetLevel * 40.0f;
    if (hgt < 1.0f) return;
    const int bottom = boardY + boardH - 3, top = bottom - (int) hgt;
    for (int y = top; y <= bottom; ++y)
        for (int x = boardX + 3; x < boardX + boardW - 3; ++x)
        {
            const float v = ((float) (y - top) / hgt);
            const float n = 0.55f + 0.45f * std::sin ((float) x * 0.07f + time * 0.6f + std::sin ((float) y * 0.2f + time * 0.4f) * 2.0f)
                                          * std::sin ((float) x * 0.031f - time * 0.35f);
            if (Canvas::bayer (x, y, v * v * n * 0.5f)) c.blend (x + shakeX, y + shakeY, seancepal::ecto, 0.22f);
        }
}

void SeanceEditor::drawCandles (Canvas& c)
{
    const float possessed = juce::jlimit (0.0f, 1.0f, (fb - 1.0f) / 0.08f);
    for (int k = 0; k < 2; ++k)
    {
        const int cx = candleX[k] + shakeX, oy = shakeY;
        // dish and wax
        c.fillRect (cx - 4, candleBottom + 1 + oy, 9, 2, 0x2a2622);
        c.hline (cx - 4, cx + 4, candleBottom + 1 + oy, 0x46403a);
        for (int y = candleTop; y <= candleBottom; ++y)
            for (int x = -2; x <= 1; ++x)
            {
                RGB col = x == -2 ? (RGB) 0xa89c88 : x == 1 ? (RGB) 0x4e463c : (RGB) 0x7e7464;
                if (y == candleTop) col = mix (col, 0xe8dcc4, 0.5f);
                c.set (cx + x, y + oy, col);
            }
        c.set (cx - 1, candleTop + 2 + oy, 0xb8ac96); // a drip
        c.set (cx - 1, candleTop + 3 + oy, 0x8c8272);
        c.set (cx, candleTop - 1 + oy, 0x1a1410); // wick

        // flame: flares on every echo that comes out of this side
        const float fl = flare[k];
        const float flick = std::sin (time * (11.0f + (float) k * 2.0f)) * std::sin (time * (5.3f + (float) k));
        const int h = 5 + (int) std::lround (flick * 0.8f + fl * 5.0f);
        const int sway = std::sin (time * 1.7f + (float) k * 2.0f) > 0.6f ? 1 : 0;
        const float ecto = std::max (fl * 0.85f, possessed);
        for (int r = 0; r < h; ++r)
        {
            const float t = (float) r / (float) h; // 0 base .. 1 tip
            const int half = t < 0.6f ? 1 : 0;
            const int xo = t > 0.5f ? sway : 0;
            for (int x = -half; x <= half; ++x)
            {
                RGB col = t < 0.35f && x == 0 ? (RGB) 0xfff4d0 : t < 0.7f ? (RGB) 0xf0b050 : (RGB) 0xc05a20;
                if (std::abs (x) == 1) col = 0xd07a30;
                col = mix (col, t < 0.4f ? seancepal::hot : seancepal::ecto, ecto);
                c.set (cx + x + xo, candleTop - 2 - r + oy, col);
            }
        }
        // halo
        const RGB haloCol = mix (0x6a3a14, seancepal::dim, ecto);
        for (int y = -9; y <= 5; ++y)
            for (int x = -7; x <= 7; ++x)
            {
                const float d = std::sqrt ((float) (x * x + y * y)) / (7.0f + fl * 4.0f);
                if (d < 1.0f && (std::abs (x) > 1 || y < -h - 2 || y > 0))
                    c.add (cx + x, candleTop - 4 + y + oy, haloCol, 0.18f * (1.0f - d) * (1.0f + fl));
            }
    }
}

void SeanceEditor::drawMotes (Canvas& c)
{
    for (const auto& m : motes)
    {
        const float a = 0.5f + 0.5f * std::sin (time * 1.3f + m.phase);
        c.blend ((int) m.x + shakeX, (int) m.y + shakeY, 0x8a7458, 0.35f + 0.4f * a);
    }
}

void SeanceEditor::drawGhosts (Canvas& c)
{
    // high cut = how sharp the ghosts are, low cut = how much body they have
    const RGB edgeCol = mix (mix (seancepal::dim, seancepal::ecto, 0.6f), seancepal::hot, 0.45f * highN);
    for (const auto& g : ghosts)
    {
        if (g.life <= 0.0f) continue;
        const float f = g.life / g.maxLife;
        const float a = g.a * std::pow (f, 1.2f);
        const int gx = (int) std::lround (g.x + std::sin (g.phase + time * 2.0f) * (1.0f - f) * 2.0f) - lensX + shakeX;
        const int gy = (int) std::lround (g.y - (1.0f - f) * 7.0f) - lensY + shakeY;
        for (int j = 0; j < pH; ++j)
            for (int i = 0; i < pW; ++i)
            {
                const char m = pMask[(size_t) j][(size_t) i];
                if (m == '.' || m == 'O') continue;
                auto solid = [&] (int ii, int jj)
                {
                    if (ii < 0 || jj < 0 || ii >= pW || jj >= pH) return false;
                    const char q = pMask[(size_t) jj][(size_t) ii];
                    return q != '.' && q != 'O';
                };
                const bool edge = ! solid (i - 1, j) || ! solid (i + 1, j) || ! solid (i, j - 1) || ! solid (i, j + 1);
                const int x = gx + i, y = gy + j;
                if (edge)
                {
                    // bright highs: a crisp outline; dark highs: only a smoky, broken one
                    if (Canvas::bayer (x, y, 0.3f + 0.7f * highN)) c.blend (x, y, edgeCol, std::min (1.0f, a * 1.1f));
                }
                else if (Canvas::bayer (x, y, a * 0.6f * (1.0f - 0.85f * lowN)))
                {
                    c.blend (x, y, seancepal::ecto, 0.3f + 0.3f * a);
                }
            }
    }
}

void SeanceEditor::drawPlanchette (Canvas& c)
{
    // the trembling hand: wow (slow sway) and flutter (fast jitter)
    const float wobX = wow * (1.6f * std::sin (time * 3.46f) + 0.7f * std::sin (time * 42.7f));
    const float wobY = wow * (1.0f * std::sin (time * 2.9f + 1.0f) + 0.5f * std::sin (time * 37.1f));
    const int baseX = (int) std::lround (pxNow + wobX) - lensX + shakeX;
    const int groundY = (int) std::lround (pyNow + wobY) - lensY + shakeY;
    const int topY = groundY - (int) std::lround (lift);
    const float ectoLens = 0.15f + 0.5f * lastLevel * std::max (0.0f, 1.0f - sinceEcho * 1.2f);

    auto at = [this] (int i, int j) -> char
    {
        if (i < 0 || j < 0 || i >= pW || j >= pH) return '.';
        return pMask[(size_t) j][(size_t) i];
    };

    // shadow on the board (stays down while the planchette hops)
    for (int j = 0; j < pH; ++j)
        for (int i = 0; i < pW; ++i)
            if (at (i, j) != '.')
                c.blend (baseX + i + 1, groundY + j + 2, 0x000000, 0.5f);

    for (int j = 0; j < pH; ++j)
        for (int i = 0; i < pW; ++i)
        {
            const char m = at (i, j);
            const int x = baseX + i, y = topY + j;
            if (m == '.') continue;
            if (m == 'r') { c.set (x, y, plRim); continue; }
            if (m == 'O')
            {
                RGB under = c.get (x, y);
                under = mix (under, seancepal::hot, 0.18f);
                c.set (x, y, mix (under, seancepal::ecto, ectoLens));
                continue;
            }
            RGB col = hash (i, j + 99) % 5u == 0u ? plGrain : plBody;
            if (at (i - 1, j) == '.' || at (i, j - 1) == '.') col = plHi;
            else if (at (i + 1, j) == '.' || at (i, j + 1) == '.') col = plLo;
            c.set (x, y, col);
        }
    c.set (baseX + lensX - 1, topY + lensY - 1, seancepal::hot); // glint
    // felt feet
    c.set (baseX + 3, topY + 15, plLo);
    c.set (baseX + 13, topY + 15, plLo);
}

//==============================================================================
void SeanceEditor::drawPanel (Canvas& c)
{
    c.fillRect (0, panelY, W, H - panelY, pal::voidC);
    c.fillRect (2, panelY + 1, W - 4, 40, pal::ink);
    const RGB border = mix (pal::iron, seancepal::dim, 0.3f + 0.5f * wetLevel);
    c.hline (3, W - 4, panelY, border);
    c.hline (3, W - 4, panelY + 41, border);
    c.vline (2, panelY + 1, panelY + 40, border);
    c.vline (W - 3, panelY + 1, panelY + 40, border);
}

void SeanceEditor::drawRepeats (Canvas& c)
{
    c.fillRect (rpX, rpY, rpW, rpH, 0x08070a);
    c.rect (rpX - 1, rpY - 1, rpW + 2, rpH + 2, mix (pal::iron, seancepal::dim, 0.3f + 0.6f * wetLevel));
    font::drawText (c, rpX + 2, rpY + 2, "REPEATS", pal::ash);

    // predicted level of every repeat (feedback^n), the arriving ones flash
    const int top = rpY + 9, bottom = rpY + 25, area = bottom - top;
    const float fbClamped = std::max (fb, 1.0e-4f);
    for (int k = 0; k < 12; ++k)
    {
        const float db = std::min (0.0f, 20.0f * std::log10 (fbClamped) * (float) k);
        const float norm = juce::jlimit (0.0f, 1.0f, 1.0f + db / 48.0f);
        const int bh = std::max (norm > 0.0f ? 1 : 0, (int) std::lround (norm * (float) area));
        const int bx = rpX + 2 + k * 6;
        const bool right = ping && (k & 1);
        RGB col = mix (seancepal::dim, seancepal::ecto, right ? 0.45f : 0.65f);
        col = mix (col, seancepal::hot, barFlash[(size_t) k]);
        for (int r = 0; r < bh; ++r)
            c.hline (bx, bx + 3, bottom - r, (r & 1) == 0 ? col : scale (col, 0.6f));
        if (repeatIdx == k + 1) c.hline (bx, bx + 3, bottom + 2, seancepal::ecto);
    }

    // the time it takes
    std::string t = juce::String (juce::roundToInt (delayMs)).toStdString() + "MS";
    font::drawText (c, rpX + 2, rpY + rpH - 7, t, pal::smoke);
    if (syncT.isOn())
        font::drawRight (c, rpX + rpW - 2, rpY + rpH - 7,
                         juce::String (juce::roundToInt (proc.meters.bpm.load())).toStdString() + "BPM", pal::smoke);
}
