#include "PluginEditor.h"

using namespace px;

namespace
{
// 7x9 title glyphs, drawn at 2x
const char* const glyphP[] = { "######.", "##...##", "##...##", "##...##", "######.", "##.....", "##.....", "##.....", "##....." };
const char* const glyphY[] = { "##...##", "##...##", "##...##", ".##.##.", "..###..", "..###..", "..###..", "..###..", "..###.." };
const char* const glyphR[] = { "######.", "##...##", "##...##", "##...##", "######.", "##.##..", "##..##.", "##...##", "##...##" };
const char* const glyphE[] = { "#######", "##.....", "##.....", "##.....", "######.", "##.....", "##.....", "##.....", "#######" };

struct TitleGlyph { const char* const* rows; int x; bool accent; };
constexpr TitleGlyph title[] = { { glyphP, 8, false }, { glyphY, 25, false }, { glyphR, 42, false }, { glyphE, 59, true } };
constexpr int titleY = 5, titleScale = 2, glyphH = 9;

const char* const skull[] = {
    "..#######..",
    ".#########.",
    "###########",
    "###########",
    "##..###..##",
    "##..###..##",
    "###########",
    ".####.####.",
    "..#######..",
    "..#.#.#.#..",
    "...#####...",
};
constexpr int skullX = 71, skullY = 44, skullW = 11, skullH = 11;
constexpr int stakeX = 74, stakeTop = 55;

constexpr int panelX = 154, panelY = 74, panelW = 164, panelH = 98;
constexpr int cvX = 156, cvY = 14, cvW = 70, cvH = 58;
constexpr int hmX = 230, hmY = 14, hmW = 88, hmH = 58;
constexpr int meterRowY[3] = { 129, 141, 153 };
constexpr int meterBarX = 178, meterSegs = 36;
constexpr float heatFloorDb = -30.0f, heatSpanDb = 48.0f; // heat scale: -30..+18 dB re. the shaper's knee
constexpr int readoutY = 176;
constexpr int groundY = 160;

// black -> blood -> orange -> gold -> white-hot
constexpr RGB fireRamp[16] = { 0x060608, 0x14070a, 0x24080b, 0x3c0a0c, 0x5c0f0c, 0x7e170b, 0x9e240b, 0xbc340c,
                               0xd4480e, 0xe65e12, 0xf27a18, 0xf89a22, 0xffb42e, 0xfecd5a, 0xffe398, 0xfff6d8 };
constexpr RGB bark = 0x2a1a12, barkLit = 0x6a3c1a, grain = 0x6e4a2a, grainDark = 0x3a2416, charcoal = 0x121010;
constexpr RGB boneDim = 0x5e584f, boneLit = 0xb8ad98;

struct SideLog { int x0, x1, y; };
struct EndRow  { int x0, x1, y, step; };
constexpr SideLog sideLogs[] = { { 28, 124, 156 }, { 40, 112, 144 }, { 54, 98, 132 } };
constexpr EndRow  endRows[]  = { { 34, 118, 150, 8 }, { 50, 102, 138, 8 } };

float gainToDb (float g) noexcept { return juce::Decibels::gainToDecibels (g, -100.0f); }

uint32_t hash (int x, int y) noexcept
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/** Flame height in pixels for a heat 0..1 (idle smoulder at 0). */
float flameHeight (float heat, int mode) noexcept
{
    static constexpr float modeH[] = { 0.92f, 0.7f, 1.0f, 0.9f };
    return (24.0f + 96.0f * std::pow (juce::jlimit (0.0f, 1.2f, heat), 0.85f)) * modeH[juce::jlimit (0, 3, mode)];
}

float smooth01 (float a, float b, float x) noexcept
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

//==============================================================================
void PyreEditor::Meter::feed (float norm, float v, float dt, float attack, float release) noexcept
{
    norm = juce::jlimit (0.0f, 1.0f, norm);
    shown += (norm - shown) * ease (dt, norm > shown ? attack : release);
    if (norm >= peak) { peak = norm; value = v; hold = 1.2f; }
    else if ((hold -= dt) < 0.0f)
    {
        peak = std::max (shown, peak - dt * 0.6f);
        if (peak <= shown + 0.001f) value = v;
    }
}

//==============================================================================
PyreEditor::PyreEditor (PyreProcessor& p)
    : PixelEditor (p, W, H, (int) p.apvts.state.getProperty ("uiScale", 3)),
      proc (p),
      fireHandle (*p.apvts.getParameter (params::drive)),
      cMode  (*p.apvts.getParameter (params::mode), "MODE", { "TUBE", "TAPE", "FUZZ", "FOLD" }, pyrepal::flame),
      autoT  (*p.apvts.getParameter (params::autoGain), "AUTO", pyrepal::flame),
      bypass (*p.apvts.getParameter (params::bypass), "BYP", pyrepal::flame),
      kDrv  (*p.apvts.getParameter (params::drive),  "DRIVE", "DRIVE",       "DB", pyrepal::flame),
      kBias (*p.apvts.getParameter (params::bias),   "BIAS",  "BIAS / WIND", "%",  pyrepal::flame),
      kTone (*p.apvts.getParameter (params::tone),   "TONE",  "TONE",        "%",  pyrepal::flame),
      kMix  (*p.apvts.getParameter (params::mix),    "MIX",   "MIX",         "%",  pyrepal::flame),
      kOut  (*p.apvts.getParameter (params::output), "OUT",   "OUTPUT",      "DB", pyrepal::flame)
{
    fireHandle.bounds = { 0, 28, stageW, groundY - 28 };
    presetSel.bounds  = { 172, 3, 128, 7 };
    cMode.bounds      = { 158, 78, cMode.getPreferredWidth(), 7 };
    cMode.hot = pyrepal::hot;
    cMode.dim = pyrepal::dim;
    autoT.bounds  = { 264, 80, 24, 5 };
    bypass.bounds = { 294, 80, 22, 5 };

    px::Knob* knobs[] = { &kDrv, &kBias, &kTone, &kMix, &kOut };
    for (int i = 0; i < 5; ++i)
    {
        knobs[i]->bounds = { 157 + i * 32, 90, 32, 32 };
        knobs[i]->hot = pyrepal::hot;
        knobs[i]->unlit = pyrepal::dim;
    }

    presetSel.accent = pyrepal::flame;
    presetSel.getText = [this]
    {
        return proc.getProgramName (proc.getCurrentProgram()) + (proc.isPresetModified() ? "*" : "");
    };
    presetSel.onStep = [this] (int dir)
    {
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + presets::count) % presets::count);
    };
    presetSel.onClick = [this] { showPresetMenu(); };

    fireHandle.onPoke = [this] { poke = 1.0f; spawnSparks (18, true); };

    for (auto* w : allWidgets())
        addWidget (*w);

    auto& ap = p.apvts;
    driveDb = ap.getRawParameterValue (params::drive)->load();
    bias = ap.getRawParameterValue (params::bias)->load() * 0.01f;
    tone = ap.getRawParameterValue (params::tone)->load() * 0.01f;
    mixAmt = ap.getRawParameterValue (params::mix)->load() * 0.01f;
    mode = cMode.getIndex();

    for (auto& f : ash)
        f = { random.nextFloat() * (float) stageW, random.nextFloat() * (float) groundY, random.nextFloat() * 6.28f };

    // cooling map: blurred noise (wraps in both directions), normalised to 0..1
    for (auto& v : coolMap) v = rnd();
    auto tmp = coolMap;
    for (int pass = 0; pass < 3; ++pass)
    {
        for (int y = 0; y < coolH; ++y)
            for (int x = 0; x < fireW; ++x)
            {
                float s = 0.0f;
                for (int dy = -2; dy <= 2; ++dy)
                    for (int dx = -2; dx <= 2; ++dx)
                        s += coolMap[(size_t) (((y + dy + coolH) % coolH) * fireW + (x + dx + fireW) % fireW)];
                tmp[(size_t) (y * fireW + x)] = s / 25.0f;
            }
        coolMap = tmp;
    }
    const auto [lo, hi] = std::minmax_element (coolMap.begin(), coolMap.end());
    const float mn = *lo, range = std::max (1.0e-6f, *hi - *lo);
    for (auto& v : coolMap) v = (v - mn) / range;

    onScaleChanged = [this] (int s) { proc.apvts.state.setProperty ("uiScale", s, nullptr); };
}

std::array<px::Widget*, 10> PyreEditor::allWidgets()
{
    return { &fireHandle, &presetSel, &cMode, &autoT, &bypass, &kDrv, &kBias, &kTone, &kMix, &kOut };
}

void PyreEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("PRESETS");
    for (int i = 0; i < presets::count; ++i)
        menu.addItem (i + 1, presets::factory[i].name, true, i == proc.getCurrentProgram());

    const auto sb = presetSel.bounds * getPixelScale();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (sb)),
                        [safe = juce::Component::SafePointer<PyreEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                            {
                                safe->proc.setCurrentProgram (result - 1);
                                safe->presetSel.flashNow();
                            }
                        });
}

void PyreEditor::advanceForTest (float seconds)
{
    constexpr float dt = 1.0f / 60.0f;
    for (float t = 0.0f; t < seconds; t += dt)
    {
        time += dt;
        for (auto* w : allWidgets()) w->update (dt);
        tick (dt);
    }
}

float PyreEditor::rnd() noexcept
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (float) (rngState >> 8) * (1.0f / 16777216.0f);
}

//==============================================================================
void PyreEditor::spawnSparks (int count, bool burst)
{
    const float h = flameHeight (heat, mode);
    for (int n = 0; n < count; ++n)
        for (auto& s : sparks)
            if (s.life <= 0.0f)
            {
                s.x = (float) centreX + (rnd() - 0.5f) * (burst ? 50.0f : 36.0f) * (0.6f + 0.4f * heat);
                s.y = (float) fireBottom - 4.0f - rnd() * h * 0.6f;
                s.vx = (rnd() - 0.5f) * (burst ? 50.0f : 18.0f) + bias * 15.0f;
                s.vy = -(burst ? 30.0f + rnd() * 60.0f : 15.0f + rnd() * 35.0f);
                s.maxLife = s.life = 0.6f + rnd() * (burst ? 1.2f : 1.6f);
                break;
            }
}

/** One step of the fire: seed the bottom row from the heat, let every cell rise one row, cooling as it goes. */
void PyreEditor::stepFire()
{
    const float boost = juce::jmin (1.2f, heat + poke * 0.3f);
    const float meanDecay = 1.0f / std::max (3.0f, flameHeight (boost, mode));
    const float jag = mode == 2 ? 1.5f : mode == 1 ? 0.5f : 0.9f;
    const float wind = juce::jlimit (-1.0f, 1.0f, bias);
    const float windP = std::abs (wind) * 0.3f;
    const int windStep = wind > 0.0f ? -1 : 1; // pull from upwind -> flames lean downwind

    // seed
    const float half = 30.0f + 12.0f * heat;
    const float intensity = 0.7f + 0.3f * std::min (1.0f, boost);
    float* bottom = cells.data() + (fireH - 1) * fireW;
    for (int x = 0; x < fireW; ++x)
    {
        const float d = std::abs ((float) x + 0.5f - (float) fireW * 0.5f) / half;
        const float taper = d >= 1.0f ? 0.0f : 1.0f - d * d;
        const float r = rnd();
        float v = intensity * taper;
        if (mode == 2)      v *= r < 0.3f ? 0.25f : 0.85f + 0.15f * r;  // fuzz: sputtering, gappy bed
        else if (mode == 1) v *= 0.92f + 0.08f * r;                     // tape: an even glow
        else                v *= 0.75f + 0.25f * r;
        bottom[x] = v;
    }

    // rise and cool
    if (++coolScroll % coolH == 0) coolShift = (coolShift + 37) % fireW; // so the pattern never visibly loops
    for (int y = 0; y < fireH - 1; ++y)
    {
        float rowMul = 1.0f;
        if (mode == 3) // fold: bands of fast cooling travel up and tear the tongues into blobs
            rowMul = std::max (0.05f, 1.0f + 1.1f * std::sin ((float) y * 0.42f - time * 10.0f));
        if (y < 8) rowMul += (float) (8 - y) * 0.25f; // never clip at the grid's top

        float* row = cells.data() + y * fireW;
        const float* below = row + fireW;
        const float* below2 = y + 2 < fireH ? below + fireW : below;
        // the cooling map scrolls up with the fire, so a rising cell keeps meeting the same cool/warm blob -> tongues
        const float* cool = coolMap.data() + ((y + coolScroll) & (coolH - 1)) * fireW;
        auto at = [] (const float* r, int x) { return r[juce::jlimit (0, fireW - 1, x)]; };
        for (int x = 0; x < fireW; ++x)
        {
            // average the cells below (not copy one of them: copying makes neighbours share ancestors -> streaks)
            const int sx = x + (rnd() < windP ? windStep : 0);
            const float avg = (at (below, sx - 1) + at (below, sx) + at (below, sx + 1) + at (below2, sx)) * 0.25f;
            const float cm = cool[(x + coolShift) % fireW];
            const float dcy = meanDecay * rowMul * (0.1f + 1.8f * cm) * std::max (0.0f, 1.0f + jag * 0.6f * (rnd() * 2.0f - 1.0f));
            row[x] = std::max (0.0f, avg - dcy);
        }
    }
}

/** Harmonics 2..10 of a sine at the current input level (or -6 dBFS when silent) through the current curve. */
void PyreEditor::computeHarmonics()
{
    constexpr int n = 256;
    static const auto table = []
    {
        std::array<std::array<float, n>, 2> t {};
        for (int i = 0; i < n; ++i)
        {
            t[0][(size_t) i] = std::sin (6.2831853f * (float) i / (float) n);
            t[1][(size_t) i] = std::cos (6.2831853f * (float) i / (float) n);
        }
        return t;
    }();
    const auto& sinT = table[0];
    const auto& cosT = table[1];

    harmPreview = inLevel < 0.003f;
    const float a = harmPreview ? 0.5f : std::min (inLevel, 1.0f);
    const float dg = juce::Decibels::decibelsToGain (driveDb);
    const float b = bias * sdsp::Saturator::maxBias;

    std::array<float, n> y {};
    for (int i = 0; i < n; ++i)
    {
        const float x = a * sinT[(size_t) i];
        const float s = sdsp::Saturator::shapeBiased (mode, dg * x, b);
        y[(size_t) i] = x + (s - x) * mixAmt;
    }

    auto mag = [&] (int k)
    {
        float re = 0.0f, im = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const auto idx = (size_t) ((k * i) & (n - 1));
            re += y[(size_t) i] * cosT[idx];
            im += y[(size_t) i] * sinT[idx];
        }
        return std::sqrt (re * re + im * im);
    };

    const float h1 = std::max (mag (1), 1.0e-9f);
    for (int k = 0; k < numHarm; ++k)
        harmDb[(size_t) k] = std::max (-60.0f, 20.0f * std::log10 (std::max (mag (k + 2), 1.0e-9f) / h1));
}

void PyreEditor::tick (float dt)
{
    auto& ap = proc.apvts;
    const bool bypassed = bypass.isOn();
    mode = cMode.getIndex();

    driveDb += (ap.getRawParameterValue (params::drive)->load() - driveDb) * ease (dt, 14.0f);
    bias += (ap.getRawParameterValue (params::bias)->load() * 0.01f - bias) * ease (dt, 6.0f);
    tone += (ap.getRawParameterValue (params::tone)->load() * 0.01f - tone) * ease (dt, 8.0f);
    mixAmt += (ap.getRawParameterValue (params::mix)->load() * 0.01f - mixAmt) * ease (dt, 10.0f);
    poke = std::max (0.0f, poke - dt * 1.5f);

    // --- audio (meters are "max since last read"; ease them so slow audio callbacks don't flicker)
    const float inLin   = proc.meters.inPeak.exchange (0.0f);
    const float outLin  = proc.meters.outPeak.exchange (0.0f);
    const float heatLin = proc.meters.heat.exchange (0.0f);
    const float inDb = gainToDb (inLin), outDb = gainToDb (outLin), heatDb = gainToDb (heatLin);

    mIn.feed ((inDb + 60.0f) / 60.0f, inDb, dt, 60.0f, 7.0f);
    mOut.feed ((outDb + 60.0f) / 60.0f, outDb, dt, 60.0f, 7.0f);
    mHeat.feed ((heatDb - heatFloorDb) / heatSpanDb, heatDb, dt, 60.0f, 6.0f);
    inLevel += (std::min (inLin, 1.5f) - inLevel) * ease (dt, inLin > inLevel ? 40.0f : 4.0f);

    const float target = juce::jlimit (0.0f, 1.0f, (heatDb - heatFloorDb) / heatSpanDb);
    heat     += (target - heat) * ease (dt, target > heat ? 25.0f : 3.5f);
    slowHeat += (target - slowHeat) * ease (dt, 2.0f);
    if (target - slowHeat > 0.2f && ! bypassed)
    {
        spawnSparks (6 + (int) (14.0f * (target - slowHeat)), true); // transient: the fire spits
        slowHeat = target;
    }

    const float charTarget = heat > 0.35f ? heat : 0.0f;
    charAmt += (charTarget - charAmt) * ease (dt, charTarget > charAmt ? 0.12f : 0.04f);

    // --- fire simulation at a fixed rate
    fireAcc += dt;
    int steps = 0;
    while (fireAcc >= 1.0f / fireRate && steps++ < 8)
    {
        fireAcc -= 1.0f / fireRate;
        stepFire();
    }
    if (steps >= 8) fireAcc = 0.0f;

    // --- sparks
    sparkAcc += dt * (heat * heat * 14.0f + (mode == 2 ? heat * 36.0f : 0.0f) + poke * 20.0f);
    while (sparkAcc >= 1.0f) { sparkAcc -= 1.0f; spawnSparks (1, false); }
    for (auto& s : sparks)
        if (s.life > 0.0f)
        {
            s.life -= dt;
            s.vy -= 14.0f * dt;
            s.vx += (bias * 20.0f + (rnd() - 0.5f) * 60.0f) * dt;
            s.x += s.vx * dt;
            s.y += s.vy * dt;
        }

    // --- smoke from the flame tips (tape smoulders and smokes most)
    const float tipY = (float) fireBottom - flameHeight (heat, mode) * 0.85f;
    smokeAcc += dt * (2.0f + heat * 10.0f * (mode == 1 ? 2.2f : 1.0f));
    while (smokeAcc >= 1.0f)
    {
        smokeAcc -= 1.0f;
        for (auto& s : smoke)
            if (s.life <= 0.0f)
            {
                s.x = (float) centreX + (rnd() - 0.5f) * 30.0f + bias * 12.0f;
                s.y = tipY;
                s.vx = bias * 10.0f + (rnd() - 0.5f) * 4.0f;
                s.vy = -(8.0f + rnd() * 8.0f + heat * 10.0f);
                s.r = 1.5f + rnd();
                s.maxLife = s.life = 3.0f + rnd() * 2.0f;
                break;
            }
    }
    for (auto& s : smoke)
        if (s.life > 0.0f)
        {
            s.life -= dt;
            s.x += s.vx * dt;
            s.y += s.vy * dt;
            s.vx += bias * 6.0f * dt;
            s.r += dt * 1.6f;
        }

    // --- ash drifting down
    for (auto& f : ash)
    {
        f.y += (5.0f + 2.0f * std::sin (time * 1.3f + f.phase)) * dt;
        f.x += (std::sin (time * 0.9f + f.phase) * 5.0f + bias * 6.0f) * dt;
        if (f.y >= (float) groundY || f.x < 0.0f || f.x >= (float) stageW)
            f = { rnd() * (float) stageW, 26.0f + rnd() * 10.0f, rnd() * 6.28f };
    }

    computeHarmonics();
    for (int k = 0; k < numHarm; ++k)
        harmShown[(size_t) k] += (harmDb[(size_t) k] - harmShown[(size_t) k]) * ease (dt, 12.0f);
}

//==============================================================================
void PyreEditor::paintScene (Canvas& c)
{
    c.clear (pal::voidC);
    drawSky (c);
    drawSmoke (c);
    drawGround (c);
    drawStake (c);
    drawFire (c);
    drawLogs (c);
    drawSparks (c);
    drawTitle (c);

    drawPanel (c);
    drawCurve (c);
    drawHarmonics (c);
    drawMeters (c);
}

void PyreEditor::paintOverlay (Canvas& c)
{
    std::string txt;
    RGB col = pal::bone;
    if (auto* w = getActiveWidget())
        txt = w->getReadout().toUpperCase().toStdString();
    if (txt.empty())
    {
        txt = bypass.isOn() ? "BYPASSED" : "DRAG THE FIRE TO BURN HOTTER";
        col = pal::smoke;
    }
    font::drawRight (c, W - 4, readoutY, txt, col);
}

void PyreEditor::drawSky (Canvas& c)
{
    const float flick = 0.9f + 0.1f * std::sin (time * 13.0f) * std::sin (time * 7.3f);
    for (int y = 0; y < groundY; ++y)
        for (int x = 0; x < stageW; ++x)
        {
            RGB col = mix (0x060609, 0x0e0a10, (float) y / (float) groundY);
            const float dx = (float) (x - centreX), dy = (float) (y - 120) * 1.2f;
            const float d = std::sqrt (dx * dx + dy * dy) / 110.0f;
            if (d < 1.0f && Canvas::bayer (x, y, heat * 0.6f * flick * (1.0f - d) * (1.0f - d)))
                col = mix (col, 0x2c0e0a, 0.85f);
            if (y < 110 && hash (x, y) % 480u == 0u)
                col = mix (pal::ash, pal::smoke, 0.5f + 0.5f * std::sin (time * 1.7f + (float) (x * 7 + y)));
            c.set (x, y, col);
        }

    // moon
    constexpr int mx = 134, my = 16;
    for (int y = -6; y <= 6; ++y)
        for (int x = -6; x <= 6; ++x)
        {
            if (x * x + y * y > 38) continue;
            RGB col = (x + y > 3) ? (RGB) 0x4a4650 : (RGB) 0x6e6a74;
            if (hash (x + 20, y + 20) % 6u == 0u) col = 0x56525c;
            c.set (mx + x, my + y, col);
        }
}

void PyreEditor::drawSmoke (Canvas& c)
{
    for (const auto& s : smoke)
    {
        if (s.life <= 0.0f) continue;
        const float a = s.life / s.maxLife;
        const int ir = (int) std::ceil (s.r);
        for (int y = -ir; y <= ir; ++y)
            for (int x = -ir; x <= ir; ++x)
            {
                const float d = std::sqrt ((float) (x * x + y * y)) / s.r;
                if (d > 1.0f) continue;
                const int px = (int) s.x + x, py = (int) s.y + y;
                if (px < 0 || px >= stageW) continue;
                if (Canvas::bayer (px, py, a * 0.5f * (1.0f - d * d))) c.set (px, py, mix (pal::iron, pal::ash, a));
            }
    }
    for (const auto& f : ash)
        if ((int) f.x < stageW) c.set ((int) f.x, (int) f.y, 0x3a3640);
}

void PyreEditor::drawGround (Canvas& c)
{
    for (int y = groundY; y < H; ++y)
        for (int x = 0; x < stageW; ++x)
        {
            const auto h = hash (x, y);
            RGB col = y == groundY ? (RGB) 0x17110f : (h % 7u == 0u ? (RGB) 0x110d0d : (RGB) 0x0b0909);
            const float dx = (float) (x - centreX), dy = (float) (y - groundY) * 2.2f;
            const float d = std::sqrt (dx * dx + dy * dy) / 80.0f;
            const float g = d < 1.0f ? (0.08f + heat * 0.9f) * (1.0f - d) * (1.0f - d) : 0.0f;
            if (Canvas::bayer (x, y, g)) col = mix (col, 0x5c1a0c, 0.6f);
            // stray embers in the ash around the pyre
            if (d < 0.7f && h % 53u == 0u)
                col = mix (col, fireRamp[9], (0.3f + 0.7f * heat) * (0.5f + 0.5f * std::sin (time * (2.0f + (float) (h % 5u)) + (float) h)));
            c.set (x, y, col);
        }
}

void PyreEditor::drawStake (Canvas& c)
{
    for (int y = stakeTop; y < groundY; ++y)
    {
        const float lit = heat * 0.6f * smooth01 (70.0f, 150.0f, (float) y);
        const RGB base = mix (0x2c1d14, charcoal, charAmt * smooth01 (90.0f, 150.0f, (float) y));
        c.set (stakeX,     y, mix (mix (base, 0x4a3020, 0.6f), fireRamp[10], lit));
        c.set (stakeX + 1, y, mix (base, fireRamp[9], lit * 0.6f));
        c.set (stakeX + 2, y, base);
        c.set (stakeX + 3, y, mix (base, pal::voidC, 0.5f));
    }
    // chain
    for (int y : { 112, 118 })
        for (int x = stakeX - 3; x <= stakeX + 6; ++x)
            if (((x + y / 6) & 1) == 0) c.set (x, y, mix (0x3a3640, fireRamp[11], heat * 0.5f));
}

void PyreEditor::drawFire (Canvas& c)
{
    // colour lookup: the tone pushes the ramp towards white-hot (bright) or deep red (dark)
    std::array<RGB, 64> lut {};
    const float ex = 1.0f - 0.35f * tone;
    for (int k = 0; k < 64; ++k)
    {
        const float v = std::pow ((float) k / 63.0f, ex) * 15.0f;
        const int i = std::min ((int) v, 14);
        lut[(size_t) k] = mix (fireRamp[i], fireRamp[i + 1], v - (float) i);
    }

    const int top = fireBottom - fireH + 1;
    for (int y = 0; y < fireH; ++y)
        for (int x = 0; x < fireW; ++x)
        {
            const float v = cells[(size_t) (y * fireW + x)];
            if (v < 0.03f) continue;
            // pixel-art look: colours in hard bands, soft edges as ordered dither instead of alpha
            const int k = std::min (63, (int) (std::floor (v * 11.0f) / 11.0f * 63.0f + 3.0f));
            const int px = fireX + x, py = top + y;
            if (Canvas::bayer (px, py, std::min (1.0f, v * 2.6f))) c.set (px, py, lut[(size_t) k]);
        }
}

void PyreEditor::drawLogs (Canvas& c)
{
    auto ember = [this] (uint32_t h)
    {
        return heat * (0.5f + 0.5f * std::sin (time * (3.0f + (float) (h % 5u)) + (float) (h % 97u)));
    };

    auto sideLog = [&] (const SideLog& l)
    {
        for (int j = -2; j <= 2; ++j)
            for (int x = l.x0; x <= l.x1; ++x)
            {
                const bool end = x <= l.x0 + 1 || x >= l.x1 - 1;
                if ((x == l.x0 || x == l.x1) && std::abs (j) == 2) continue; // rounded ends
                RGB col;
                if (end && std::abs (j) <= 1)  col = (x == l.x0 || x == l.x1) ? grainDark : grain;
                else if (j == -2)              col = mix (barkLit, fireRamp[10], 0.2f + 0.6f * heat);
                else if (j == 2)               col = 0x120c0a;
                else if (j == 1)               col = mix (bark, pal::voidC, 0.3f);
                else                           col = bark;
                col = mix (col, charcoal, charAmt * 0.7f);
                const auto h = hash (x, l.y + j);
                if (std::abs (j) <= 1 && ! end && h % 9u == 0u)
                {
                    const float g = ember (h);
                    col = mix (col, g > 0.5f ? fireRamp[13] : fireRamp[8], g);
                }
                c.set (x, l.y + j, col);
            }
    };

    auto endRow = [&] (const EndRow& r)
    {
        for (int cx = r.x0; cx <= r.x1; cx += r.step)
            for (int y = -3; y <= 3; ++y)
                for (int x = -3; x <= 3; ++x)
                {
                    const float d = std::sqrt ((float) (x * x + y * y));
                    if (d > 3.3f) continue;
                    RGB col = d > 2.5f ? bark : (d > 1.2f && d < 1.9f ? grainDark : grain);
                    if (y < -1 && d > 2.5f) col = mix (col, fireRamp[10], 0.3f + 0.5f * heat);
                    col = mix (col, charcoal, charAmt * 0.6f);
                    if (d < 1.0f) col = mix (col, fireRamp[11], ember (hash (cx, r.y)) * 0.9f); // burning core
                    c.set (cx + x, r.y + y, col);
                }
    };

    sideLog (sideLogs[0]);
    endRow (endRows[0]);
    sideLog (sideLogs[1]);
    endRow (endRows[1]);
    sideLog (sideLogs[2]);

    // the skull sits above the flames' base and is lit from below
    const float flick = 0.75f + 0.25f * std::sin (time * 11.0f) * std::sin (time * 4.3f);
    for (int j = 0; j < skullH; ++j)
        for (int i = 0; i < skullW; ++i)
        {
            const int x = skullX + i, y = skullY + j;
            if (skull[j][i] == '#')
            {
                const float lit = (0.35f + 0.65f * (float) j / (float) (skullH - 1)) * heat;
                RGB col = mix (boneDim, boneLit, 0.25f + 0.3f * (float) (skullW - i) / (float) skullW);
                col = mix (col, fireRamp[12], lit * 0.55f);
                c.set (x, y, col);
            }
            else if (j >= 4 && j <= 5 && (i == 2 || i == 3 || i == 7 || i == 8))
            {
                c.set (x, y, mix (0x050405, pyrepal::flame, heat * flick));    // eyes catch fire
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if (dx != 0 || dy != 0) c.add (x + dx, y + dy, pyrepal::flame, 0.05f * heat * flick);
            }
        }
}

void PyreEditor::drawSparks (Canvas& c)
{
    for (const auto& s : sparks)
    {
        if (s.life <= 0.0f) continue;
        const int x = (int) s.x, y = (int) s.y;
        if (x < 0 || x >= stageW) continue;
        const float f = s.life / s.maxLife;
        c.set (x, y, mix (fireRamp[6], pyrepal::hot, f));
        if (f > 0.7f)
            for (auto [dx, dy] : { std::pair { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } })
                c.add (x + dx, y + dy, pyrepal::flame, 0.15f);
    }
}

void PyreEditor::drawTitle (Canvas& c)
{
    const float flick = 0.85f + 0.15f * std::sin (time * 9.0f) * std::sin (time * 3.1f);
    for (const auto& g : title)
        for (int j = 0; j < glyphH; ++j)
            for (int i = 0; g.rows[j][i] != 0; ++i)
            {
                if (g.rows[j][i] != '#') continue;
                const int bx = g.x + i * titleScale, by = titleY + j * titleScale;
                for (int sy = 0; sy < titleScale; ++sy)
                    for (int sx = 0; sx < titleScale; ++sx)
                    {
                        const int x = bx + sx, y = by + sy;
                        const float down = (float) (j * titleScale + sy) / (float) (glyphH * titleScale);
                        RGB col;
                        if (g.accent) col = mix (pyrepal::hot, pyrepal::flame, down * (1.0f - 0.4f * heat));
                        else
                        {
                            // cold bone, licked by the fire from below
                            col = mix (pal::bone, pal::smoke, down * 0.55f);
                            if (Canvas::bayer (x, y, (0.15f + heat) * down * 1.2f * flick)) col = mix (col, fireRamp[11], 0.65f);
                        }
                        c.set (x, y, col);
                    }
                // drop shadow
                const RGB sh = g.accent ? pyrepal::deep : (RGB) 0x15141a;
                if (j + 1 >= glyphH || g.rows[j + 1][i] != '#')
                    for (int s = 0; s < titleScale; ++s) c.set (bx + s + 1, by + titleScale, sh);
                if (g.rows[j][i + 1] != '#')
                    for (int s = 0; s < titleScale; ++s) c.set (bx + titleScale, by + s + 1, sh);
            }

    font::drawText (c, 78, 18, "SATURATOR", pal::smoke);
}

//==============================================================================
void PyreEditor::drawPanel (Canvas& c)
{
    const int x = panelX, y = panelY, w = panelW, h = panelH;
    c.fillRect (x + 1, y + 1, w - 2, h - 2, pal::ink);
    const RGB border = mix (pal::iron, pyrepal::dim, 0.25f + 0.5f * heat);
    c.hline (x + 1, x + w - 2, y, border);
    c.hline (x + 1, x + w - 2, y + h - 1, border);
    c.vline (x, y + 1, y + h - 2, border);
    c.vline (x + w - 1, y + 1, y + h - 2, border);
    c.hline (x + 2, x + w - 3, y + 1, 0x17161d);
    for (int i = x + 2; i < x + w - 2; ++i)
    {
        c.dither (i, 87, pal::ash, 0.5f);
        c.dither (i, 125, pal::ash, 0.5f);
    }
}

void PyreEditor::drawCurve (Canvas& c)
{
    const int x0 = cvX, y0 = cvY, w = cvW, h = cvH;
    c.fillRect (x0, y0, w, h, 0x08070a);
    c.rect (x0 - 1, y0 - 1, w + 2, h + 2, mix (pal::iron, pyrepal::dim, 0.3f + 0.5f * heat));
    const int cx = x0 + w / 2, cy = y0 + h / 2;
    const float sy = (float) (h / 2 - 3);

    for (int i = x0; i < x0 + w; ++i) c.dither (i, cy, pal::ash, 0.5f);
    for (int j = y0; j < y0 + h; ++j) c.dither (cx, j, pal::ash, 0.5f);
    font::drawText (c, x0 + 2, y0 + 2, "CURVE", pal::ash);

    const float dg = juce::Decibels::decibelsToGain (driveDb);
    const float b = bias * sdsp::Saturator::maxBias;
    int prevY = cy;
    for (int i = 0; i < w; ++i)
    {
        const float xin = ((float) i + 0.5f) / (float) w * 2.0f - 1.0f;
        if ((i & 1) == 0) c.set (x0 + i, cy - (int) std::lround (xin * sy), 0x231e26); // unity line

        const float s = sdsp::Saturator::shapeBiased (mode, dg * xin, b);
        const float yv = juce::jlimit (-1.1f, 1.1f, xin + (s - xin) * mixAmt);
        const int py = cy - (int) std::lround (yv * sy);
        const bool live = std::abs (xin) <= inLevel;
        const RGB col = live ? mix (pyrepal::flame, pyrepal::hot, 0.3f + 0.5f * heat) : mix (pyrepal::dim, pyrepal::flame, 0.4f);
        if (i == 0) c.set (x0, py, col);
        else        c.vline (x0 + i, std::min (prevY, py), std::max (prevY, py), col);
        prevY = py;
    }
}

void PyreEditor::drawHarmonics (Canvas& c)
{
    const int x0 = hmX, y0 = hmY, w = hmW - 2, h = hmH;
    c.fillRect (x0, y0, w, h, 0x08070a);
    c.rect (x0 - 1, y0 - 1, w + 2, h + 2, mix (pal::iron, pyrepal::dim, 0.3f + 0.5f * heat));
    font::drawText (c, x0 + 2, y0 + 2, harmPreview ? "HARMONICS -6DB" : "HARMONICS", pal::ash);

    constexpr int barW = 7, gap = 2;
    const int top = y0 + 9, bottom = y0 + h - 8, area = bottom - top;
    for (int db : { -20, -40 })
    {
        const int gy = bottom - (int) std::lround ((1.0f + (float) db / 60.0f) * (float) area);
        for (int i = x0 + 2; i < x0 + w - 2; ++i) c.dither (i, gy, pal::ash, 0.25f);
    }

    for (int k = 0; k < numHarm; ++k)
    {
        const int n = k + 2;
        const int bx = x0 + 4 + k * (barW + gap);
        const float norm = juce::jlimit (0.0f, 1.0f, 1.0f + harmShown[(size_t) k] / 60.0f);
        const int bh = (int) std::lround (norm * (float) area);
        RGB col = (n % 2 == 0) ? mix (pyrepal::flame, pyrepal::hot, 0.5f)    // even: warm
                               : mix (pyrepal::flame, pyrepal::dim, 0.2f);   // odd: edgy
        if (harmPreview) col = mix (col, pal::ash, 0.6f);
        for (int r = 0; r < bh; ++r)
            c.hline (bx, bx + barW - 1, bottom - 1 - r, (r & 1) == 0 ? col : scale (col, 0.55f));
        font::drawCentred (c, bx + barW / 2, bottom + 2, std::to_string (n), (n % 2 == 0) ? pal::smoke : (RGB) 0x46424e);
    }
}

void PyreEditor::drawMeters (Canvas& c)
{
    const Meter* ms[3] = { &mIn, &mHeat, &mOut };
    const char* labels[3] = { "IN", "HEAT", "OUT" };
    const float knee = -heatFloorDb / heatSpanDb;

    for (int m = 0; m < 3; ++m)
    {
        const int y = meterRowY[m];
        const Meter& mt = *ms[m];
        const bool isHeat = m == 1;
        font::drawText (c, 158, y, labels[m], pal::smoke);

        const int peakSeg = (int) std::lround (mt.peak * meterSegs) - 1;
        for (int s = 0; s < meterSegs; ++s)
        {
            const float pos = (float) (s + 1) / (float) meterSegs;
            const bool lit = pos <= mt.shown + 0.001f;
            RGB col;
            if (isHeat) col = pos > knee ? mix (pyrepal::flame, pyrepal::hot, (pos - knee) / (1.0f - knee))
                                         : mix (pyrepal::dim, pyrepal::flame, 0.8f * pos / knee);
            else        col = pos > 0.9f ? pyrepal::hot : mix (pal::smoke, pal::bone, pos);
            if (! lit) col = 0x141218;
            if (s == peakSeg && mt.peak > 0.02f) col = pal::bone;
            c.fillRect (meterBarX + s * 3, y, 2, 5, col);
        }
        if (isHeat)
        {
            const int kx = meterBarX + (int) std::lround (knee * meterSegs) * 3 - 2;
            c.set (kx, y - 1, pal::bone);
            c.set (kx, y + 5, pal::bone);
        }

        juce::String txt;
        if (mt.value <= -99.0f) txt = "-INF";
        else txt = (isHeat && mt.value > 0.0f ? "+" : "") + juce::String (mt.value, 1);
        font::drawRight (c, panelX + panelW - 3, y, txt.toStdString(), isHeat && mt.value > 0.0f ? pyrepal::flame : pal::smoke);
    }
}
