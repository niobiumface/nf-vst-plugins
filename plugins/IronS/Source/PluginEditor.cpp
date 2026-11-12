#include "PluginEditor.h"

using namespace px;

namespace
{
// 7x9 title glyphs, drawn at 2x
const char* const glyphI[] = { "######", "..##..", "..##..", "..##..", "..##..", "..##..", "..##..", "..##..", "######" };
const char* const glyphR[] = { "######.", "##...##", "##...##", "##...##", "######.", "##.##..", "##..##.", "##...##", "##...##" };
const char* const glyphO[] = { ".#####.", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", ".#####." };
const char* const glyphN[] = { "##...##", "###..##", "###..##", "####.##", "##.####", "##..###", "##..###", "##...##", "##...##" };
const char* const glyphS[] = { ".#####.", "##...##", "##.....", ".####..", "...###.", ".....##", ".....##", "##...##", ".#####." };

struct TitleGlyph { const char* const* rows; int x; bool accent; };
constexpr TitleGlyph title[] = { { glyphI, 8, false }, { glyphR, 23, false }, { glyphO, 40, false }, { glyphN, 57, false }, { glyphS, 80, true } };
constexpr int titleY = 5, titleScale = 2, glyphH = 9;

constexpr int panelX = 155, panelY = 73, panelW = 179, panelH = 96;
constexpr int ctlX = 158;                                  // left edge of the controls
constexpr int meterX = 248, meterBarX = 262, meterSegs = 25;
constexpr int readoutY = 173;

// cloth
constexpr RGB cloth = 0x4b4856, clothLit = 0x6a6676, clothDark = 0x2b2933, clothEdge = 0xa59d8f;
constexpr int boardLeft = 9;                                // the board's nose; the cloth hangs down in front of it

float gainToDb (float g) noexcept { return juce::Decibels::gainToDecibels (g, -100.0f); }
} // namespace

//==============================================================================
void IronSEditor::Meter::feed (float norm, float value, float dt, float attack, float release) noexcept
{
    norm = juce::jlimit (0.0f, 1.0f, norm);
    shown += (norm - shown) * ease (dt, norm > shown ? attack : release);
    if (norm >= peak) { peak = norm; peakValue = value; hold = 1.2f; }
    else if ((hold -= dt) < 0.0f)
    {
        peak = std::max (shown, peak - dt * 0.6f);
        if (peak <= shown + 0.001f) peakValue = value;
    }
}

//==============================================================================
IronSEditor::IronSEditor (IronSProcessor& p)
    : PixelEditor (p, W, H, (int) p.apvts.state.getProperty ("uiScale", 3)),
      proc (p),
      iron (*p.apvts.getParameter (params::thresh)),
      spectrum (p),
      cMode   (*p.apvts.getParameter (params::mode),   "MODE",   { "REL", "ABS" },          ironpal::heat),
      cDetect (*p.apvts.getParameter (params::detect), "DETECT", { "BAND", "HIGH" },        ironpal::heat),
      cCut    (*p.apvts.getParameter (params::cut),    "CUT",    { "WIDE", "SPLIT" },       ironpal::heat),
      cListen (*p.apvts.getParameter (params::listen), "LISTEN", { "OFF", "SIDE", "DELTA" }, ironpal::heat),
      bypass  (*p.apvts.getParameter (params::bypass), "BYP", ironpal::heat),
      kThr  (*p.apvts.getParameter (params::thresh), "THRESH", "THRESHOLD", "DB",  ironpal::heat),
      kRng  (*p.apvts.getParameter (params::range),  "RANGE",  "RANGE",     "DB",  ironpal::heat),
      kFrq  (*p.apvts.getParameter (params::freq),   "FREQ",   "FREQUENCY", "HZ",  ironpal::heat),
      kWid  (*p.apvts.getParameter (params::width),  "WIDTH",  "WIDTH",     "OCT", ironpal::heat),
      kSpd  (*p.apvts.getParameter (params::speed),  "SPEED",  "RELEASE",   "MS",  ironpal::heat),
      kSmk  (*p.apvts.getParameter (params::smack),  "SMACK",  "SMACK / CLICK REMOVAL", "%", ironpal::heat),
      kLook (*p.apvts.getParameter (params::look),   "LOOK",   "LOOKAHEAD", "MS",  ironpal::heat),
      kLink (*p.apvts.getParameter (params::link),   "LINK",   "STEREO LINK", "%", ironpal::heat),
      kOut  (*p.apvts.getParameter (params::output), "OUT",    "OUTPUT",    "DB",  ironpal::heat)
{
    iron.bounds     = { 0, 28, stageW, floorY - 28 };
    spectrum.bounds = { ctlX, 14, 174, 56 };
    presetSel.bounds = { ctlX + 18, 3, 126, 7 };

    cMode.bounds   = { ctlX, 77, cMode.getPreferredWidth(), 7 };
    cDetect.bounds = { ctlX + 68, 77, cDetect.getPreferredWidth(), 7 };
    cCut.bounds    = { ctlX, 87, cCut.getPreferredWidth(), 7 };
    cListen.bounds = { ctlX + 68, 87, cListen.getPreferredWidth(), 7 };
    bypass.bounds  = { 310, 79, 22, 5 };
    for (auto* ch : { &cMode, &cDetect, &cCut, &cListen }) { ch->hot = ironpal::hot; ch->dim = ironpal::dim; }

    px::Knob* row1[] = { &kThr, &kRng, &kFrq, &kWid, &kSpd, &kSmk };
    px::Knob* row2[] = { &kLook, &kLink, &kOut };
    for (int i = 0; i < 6; ++i) row1[i]->bounds = { ctlX + i * 29, 100, 29, 32 };
    for (int i = 0; i < 3; ++i) row2[i]->bounds = { ctlX + i * 29, 134, 29, 32 };
    for (auto* k : { &kThr, &kRng, &kFrq, &kWid, &kSpd, &kSmk, &kLook, &kLink, &kOut })
    {
        k->hot = ironpal::hot;
        k->unlit = ironpal::dim;
    }

    presetSel.accent = ironpal::heat;
    presetSel.getText = [this]
    {
        return proc.getProgramName (proc.getCurrentProgram()) + (proc.isPresetModified() ? "*" : "");
    };
    presetSel.onStep = [this] (int dir)
    {
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + presets::count) % presets::count);
    };
    presetSel.onClick = [this] { showPresetMenu(); };

    iron.onPoke = [this] { poke = 1.0f; spawnSteam (14, true); };

    for (auto* w : std::initializer_list<Widget*> { &iron, &spectrum, &presetSel, &cMode, &cDetect, &cCut, &cListen, &bypass,
                                                    &kThr, &kRng, &kFrq, &kWid, &kSpd, &kSmk, &kLook, &kLink, &kOut })
        addWidget (*w);

    thr = p.apvts.getRawParameterValue (params::thresh)->load();
    rangeShown = p.apvts.getRawParameterValue (params::range)->load();

    onScaleChanged = [this] (int s) { proc.apvts.state.setProperty ("uiScale", s, nullptr); };
}

void IronSEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("PRESETS");
    for (int i = 0; i < presets::count; ++i)
        menu.addItem (i + 1, presets::factory[i].name, true, i == proc.getCurrentProgram());

    const auto sb = presetSel.bounds * getPixelScale();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (sb)),
                        [safe = juce::Component::SafePointer<IronSEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                            {
                                safe->proc.setCurrentProgram (result - 1);
                                safe->presetSel.flashNow();
                            }
                        });
}

void IronSEditor::advanceForTest (float seconds)
{
    constexpr float dt = 1.0f / 60.0f;
    for (float t = 0.0f; t < seconds; t += dt)
    {
        time += dt;
        for (auto* w : std::initializer_list<Widget*> { &iron, &spectrum, &presetSel, &cMode, &cDetect, &cCut, &cListen, &bypass,
                                                        &kThr, &kRng, &kFrq, &kWid, &kSpd, &kSmk, &kLook, &kLink, &kOut })
            w->update (dt);
        tick (dt);
    }
}

//==============================================================================
void IronSEditor::spawnSteam (int count, bool burst)
{
    const float sole = (float) (ironY() + ironH);
    for (int n = 0; n < count; ++n)
        for (auto& s : steam)
            if (s.life <= 0.0f)
            {
                // steam escapes at the back, at the tip and through the vents in between
                const float u = burst ? random.nextFloat() * (float) ironW
                                      : (random.nextFloat() < 0.5f ? random.nextFloat() * 9.0f - 3.0f : 40.0f + random.nextFloat() * 20.0f);
                s.x = (float) ironX() + u;
                s.y = sole - 1.0f;
                s.vx = (random.nextFloat() - 0.6f) * (burst ? 26.0f : 12.0f);
                s.vy = -(10.0f + random.nextFloat() * (burst ? 34.0f : 20.0f));
                s.life = 0.6f + random.nextFloat() * 0.5f;
                break;
            }
}

void IronSEditor::tick (float dt)
{
    auto& ap = proc.apvts;
    const bool bypassed = bypass.isOn();
    const bool band = cDetect.getIndex() == 0;

    levelFloor += ((cMode.getIndex() == 0 ? -36.0f : -60.0f) - levelFloor) * ease (dt, 8.0f);
    iron.dbPerPixel = -levelFloor / maxWrinkle;
    thr        += (ap.getRawParameterValue (params::thresh)->load() - thr) * ease (dt, 14.0f);
    rangeShown += (ap.getRawParameterValue (params::range)->load() - rangeShown) * ease (dt, 14.0f);

    // --- audio (meters are "max since last read"; ease them so slow audio callbacks don't flicker)
    const float lvl = proc.meters.level.exchange (-60.0f);
    const float gr  = proc.meters.reduction.exchange (0.0f);
    const float sm  = proc.meters.smack.exchange (0.0f);
    const float inDb  = gainToDb (proc.meters.inPeak.exchange (0.0f));
    const float outDb = gainToDb (proc.meters.outPeak.exchange (0.0f));

    live     += (lvl - live) * ease (dt, lvl > live ? 45.0f : 7.0f);
    grNow    += (gr - grNow) * ease (dt, gr > grNow ? 50.0f : 8.0f);
    smackNow += (sm - smackNow) * ease (dt, sm > smackNow ? 60.0f : 10.0f);

    mIn.feed ((inDb + 60.0f) / 60.0f, inDb, dt, 60.0f, 7.0f);
    mOut.feed ((outDb + 60.0f) / 60.0f, outDb, dt, 60.0f, 7.0f);
    mGr.feed (grNow / 24.0f, grNow, dt, 60.0f, 12.0f);
    mSmack.feed (smackNow / 24.0f, smackNow, dt, 60.0f, 12.0f);

    const float work = juce::jlimit (0.0f, 1.0f, (grNow + 0.5f * smackNow) / 10.0f);
    act  += (work - act) * ease (dt, work > act ? 30.0f : 5.0f);
    heat += (work - heat) * ease (dt, work > heat ? 6.0f : 1.2f);

    // --- the cloth runs to the left; every pixel it moves, one column of history is laid down behind the iron
    pendLevel = std::max (pendLevel, live);
    pendRed   = std::max (pendRed, bypassed ? 0.0f : grNow);
    scroll += dt * scrollSpeed;
    while ((int) scroll > scrollInt)
    {
        ++scrollInt;
        for (int i = histLen - 1; i > 0; --i) hist[(size_t) i] = hist[(size_t) i - 1];
        hist[0] = { pendLevel, pendRed };
        pendLevel = -60.0f; pendRed = 0.0f;
    }
    if (scroll > 4096.0f) { scroll -= 4096.0f; scrollInt -= 4096; }

    // --- the iron: glides a little while it works, jolts when poked
    hover += ((iron.hovered || iron.isDragging() ? 1.0f : 0.0f) - hover) * ease (dt, 10.0f);
    poke = std::max (0.0f, poke - dt * 3.0f);
    ironDx = (int) std::lround (std::sin (time * 9.0f) * 1.6f * act);

    // --- steam while pressing, sparks when a smack gets zapped
    steamAcc += dt * (act * 55.0f + heat * 4.0f);
    while (steamAcc >= 1.0f) { steamAcc -= 1.0f; spawnSteam (1, false); }
    for (auto& s : steam)
        if (s.life > 0.0f)
        {
            s.life -= dt * 0.8f;
            s.x += (s.vx + std::sin (time * 4.0f + s.y * 0.2f) * 5.0f) * dt;
            s.y += s.vy * dt;
            s.vy *= 1.0f - 0.6f * dt;
        }

    sparkCooldown -= dt;
    if (sm > 1.5f && ! bypassed && sparkCooldown <= 0.0f)
    {
        sparkCooldown = 0.09f;
        int made = 0;
        for (auto& s : sparks)
            if (s.life <= 0.0f && made < 5)
            {
                s = { (float) (ironX() + ironW - 2), (float) (ironY() + ironH - 2),
                      10.0f + random.nextFloat() * 45.0f, -(15.0f + random.nextFloat() * 45.0f), 0.25f + random.nextFloat() * 0.3f };
                ++made;
            }
    }
    for (auto& s : sparks)
        if (s.life > 0.0f) { s.life -= dt; s.x += s.vx * dt; s.y += s.vy * dt; s.vy += 160.0f * dt; }

    // --- widgets fed with state
    spectrum.band = band;
    spectrum.activity = act;
    kWid.enabled = band;
    bypass.pulse = 0.0f;
}

//==============================================================================
float IronSEditor::ridgeAt (int x) const noexcept
{
    // folds travel with the cloth; their crests reach exactly 1 so a crest shows the true level
    const float s = (float) (x + scrollInt);
    return juce::jlimit (0.5f, 1.0f, 0.76f + 0.24f * std::sin (s * 0.3f) + 0.07f * std::sin (s * 0.83f + 1.0f));
}

float IronSEditor::clothHeight (int x) const noexcept
{
    const int ix = ironX();
    if (x >= ix + ironW) return heightPx (live) * ridgeAt (x);                                   // the pile in front of the tip: live
    if (x >= ix) return std::min (heightPx (live) * ridgeAt (x), heightPx (thr));                // squeezed under the sole
    const auto& h = hist[(size_t) juce::jlimit (0, histLen - 1, ix - 1 - x)];
    // ironed: what is left - and the more was pressed, the smoother the folds
    const float ridge = ridgeAt (x);
    return heightPx (h.level - h.red) * (ridge + (1.0f - ridge) * juce::jlimit (0.0f, 1.0f, h.red / 4.0f));
}

float IronSEditor::ghostHeight (int x) const noexcept
{
    const int ix = ironX();
    if (x >= ix) return 0.0f;
    const auto& h = hist[(size_t) juce::jlimit (0, histLen - 1, ix - 1 - x)];
    return h.red > 0.3f ? heightPx (h.level) * ridgeAt (x) : 0.0f;
}

int IronSEditor::ironMaterial (int u, int v) noexcept
{
    if (u < 0 || u >= ironW || v < 0 || v >= ironH) return 0;
    if (v >= 29) // sole plate, nose slightly lifted
    {
        const int uMin = v == 31 ? 1 : 0, uMax = v == 29 ? 57 : (v == 30 ? 56 : 53);
        return u >= uMin && u <= uMax ? 3 : 0;
    }
    if (v >= 12) // body: flat at the back, falling to the tip
    {
        const float top = u <= 25 ? 12.0f : 12.0f + 17.0f * std::pow ((float) (u - 25) / 32.0f, 1.6f);
        const int uMin = v < 15 ? 2 : (v < 20 ? 1 : 0);
        if (u >= uMin && (float) v >= top) return 2;
    }
    if (v <= 4) // handle bar
    {
        const int a = v == 0 ? 9 : (v == 1 ? 7 : 6), b = v == 0 ? 34 : (v == 1 ? 36 : 37);
        if (u >= a && u <= b) return 1;
    }
    if (v >= 3 && v <= 12 && u >= 6 && u <= 11) return 1; // back post
    if (v >= 4 && v <= 18)                                // front post
    {
        const float a = 31.0f + (float) (v - 4) * 0.3f;
        if ((float) u >= a && (float) u <= a + 6.0f) return 1;
    }
    return 0;
}

//==============================================================================
void IronSEditor::paintScene (Canvas& c)
{
    c.clear (pal::voidC);

    drawRoom (c);
    drawCord (c);
    drawBoard (c);
    drawCloth (c);
    drawRuler (c);
    drawIron (c);
    drawSteam (c);
    drawPlaque (c);
    drawTitle (c);

    drawPanel (c);
    drawMeters (c);
    font::drawText (c, ctlX, 183, "V0.1", pal::ash);
    font::drawRight (c, W - 4, 183, "BY N-F.PRO", pal::ash);
}

void IronSEditor::paintOverlay (Canvas& c)
{
    std::string txt;
    RGB col = pal::bone;
    if (auto* w = getActiveWidget())
        txt = w->getReadout().toUpperCase().toStdString();
    if (txt.empty())
    {
        txt = bypass.isOn() ? "BYPASSED" : "DRAG THE IRON DOWN TO PRESS HARDER";
        col = pal::smoke;
    }
    font::drawRight (c, W - 4, readoutY, txt, col);
}

//==============================================================================
void IronSEditor::drawRoom (Canvas& c)
{
    // wallpaper dots everywhere, a dithered seam between stage and controls
    for (int y = 4; y < H; y += 8)
        for (int x = 6; x < W; x += 8)
            c.set (x, y, 0x0f0f14);
    for (int y = 4; y < H - 4; ++y) c.dither (stageW + 1, y, pal::iron, 0.5f);

    // floor
    for (int y = floorY; y < H; ++y)
        for (int x = 0; x <= stageW; ++x)
        {
            RGB col = 0x0a0a0e;
            const int depth = y - floorY;
            if (depth % 5 == 0 && Canvas::bayer (x, y, 0.4f)) col = 0x131219;
            c.set (x, y, col);
        }
    c.hline (0, stageW, floorY, 0x141319);

    // wall socket
    c.fillRect (5, 44, 9, 11, pal::iron);
    c.rect (5, 44, 9, 11, pal::ash);
    c.set (8, 48, pal::voidC); c.set (10, 48, pal::voidC);
    c.fillRect (8, 50, 3, 3, 0x24222b);
    c.set (9, 51, mix (ironpal::dim, ironpal::heat, 0.4f + 0.6f * heat)); // power light
}

void IronSEditor::drawCord (Canvas& c)
{
    // a sagging cable from the socket to the back of the iron
    const float x0 = 11.0f, y0 = 52.0f;
    const float x1 = (float) ironX() + 3.0f, y1 = (float) ironY() + 15.0f;
    const float cx = (x0 + x1) * 0.5f + std::sin (time * 1.1f) * 2.0f;
    const float cy = std::max (y0, y1) + 22.0f + std::sin (time * 1.7f) * 1.5f + 4.0f * act;
    int px0 = (int) x0, py0 = (int) y0;
    for (int i = 1; i <= 24; ++i)
    {
        const float t = (float) i / 24.0f, a = (1.0f - t) * (1.0f - t), b = 2.0f * t * (1.0f - t), d = t * t;
        const int x = (int) std::lround (a * x0 + b * cx + d * x1), y = (int) std::lround (a * y0 + b * cy + d * y1);
        c.line (px0, py0, x, y, i % 3 == 0 ? pal::ash : 0x24222b);
        px0 = x; py0 = y;
    }
}

void IronSEditor::drawBoard (Canvas& c)
{
    // crossed legs
    constexpr int topY = boardY + 6, lx = 38, rx = 118;
    for (int k = 0; k < 2; ++k)
    {
        c.line (lx + k, topY, rx + k, floorY - 1, k == 0 ? pal::ash : pal::iron);
        c.line (rx + k, topY, lx + k, floorY - 1, k == 0 ? pal::ash : pal::iron);
    }
    c.fillRect ((lx + rx) / 2 - 1, (topY + floorY) / 2 - 1, 4, 3, pal::smoke);
    c.hline (lx - 3, lx + 4, floorY - 1, pal::smoke);
    c.hline (rx - 3, rx + 4, floorY - 1, pal::smoke);

    // shadow on the floor
    for (int y = floorY + 1; y <= floorY + 3; ++y)
        for (int x = 20; x <= 136; ++x)
        {
            const float dx = (float) (x - 78) / 58.0f, dy = (float) (y - floorY - 2) / 1.8f;
            if (dx * dx + dy * dy <= 1.0f) c.set (x, y, 0x040406);
        }

    // the board: a padded slab with a rounded nose on the left, running off to the right
    for (int y = boardY; y < boardY + 6; ++y)
    {
        const int inset = (y == boardY || y == boardY + 5) ? 2 : (y == boardY + 1 || y == boardY + 4 ? 1 : 0);
        for (int x = boardLeft + inset; x <= stageW; ++x)
        {
            RGB col = y == boardY ? 0x3a3744 : (y >= boardY + 4 ? 0x131218 : pal::iron);
            if (y == boardY + 2 && Canvas::bayer (x, y, 0.5f)) col = pal::ash; // quilting
            c.set (x, y, col);
        }
    }
}

void IronSEditor::drawCloth (Canvas& c)
{
    const int base = boardY - clothThick; // top row of flat cloth

    // the ironed end hangs down in front of the board's nose
    for (int x = boardLeft - 5; x < boardLeft; ++x)
    {
        const int top = base + (boardLeft - 1 - x) / 2, bottom = 166 + (int) std::lround (std::sin (time * 1.6f + (float) x * 0.7f));
        for (int y = top; y <= bottom; ++y)
        {
            RGB col = x == boardLeft - 5 ? clothLit : (x == boardLeft - 1 ? clothDark : cloth);
            if ((y - scrollInt) % 7 == 0) col = scale (col, 0.72f);
            if (y == top || y == bottom) col = clothEdge;
            c.set (x, y, col);
        }
    }

    for (int x = boardLeft; x < stageW; ++x)
    {
        const float h = clothHeight (x);
        const int top = base - (int) std::lround (h);
        const float slope = clothHeight (x + 1) - clothHeight (x - 1);
        const bool crest = h > 3.0f && h >= clothHeight (x - 1) && h > clothHeight (x + 1);

        // what the iron took away: a dithered ghost of the wrinkle as it came in
        const int ghostTop = base - (int) std::lround (ghostHeight (x));
        for (int y = ghostTop; y < top; ++y)
        {
            if (y == ghostTop) c.set (x, y, mix (ironpal::dim, ironpal::heat, 0.55f));
            else if (Canvas::bayer (x, y, 0.5f)) c.set (x, y, mix (ironpal::deep, ironpal::dim, 0.7f));
        }

        for (int y = top; y < boardY; ++y)
        {
            const int d = y - top;
            RGB col = mix (cloth, clothDark, juce::jlimit (0.0f, 1.0f, (float) d / 16.0f));
            if (d % 4 == 3) col = scale (col, 0.78f);                                        // layers of the fold
            if (d == 1) col = slope > 0.35f ? clothLit : (slope < -0.35f ? clothDark : col); // light from the left
            if (crest && d > 1 && d < (int) (h * 0.5f) && Canvas::bayer (x, y, 0.5f)) col = clothDark; // crease
            if (d == 0) col = slope < -0.35f ? mix (clothEdge, cloth, 0.55f) : clothEdge;
            if ((x + scrollInt) % 9 == 0 && d > 0 && d < 3) col = scale (col, 0.7f);         // stitches show the motion
            if (y == boardY - 1) col = scale (col, 0.6f);
            c.set (x, y, col);
        }
    }
}

void IronSEditor::drawRuler (Canvas& c)
{
    // dB ruler on the right edge (every 6 dB) and the threshold line the sole sits on
    const int base = boardY - clothThick;
    for (int db = 0; (float) db >= levelFloor; db -= 6)
    {
        const int y = base - (int) std::lround (heightPx ((float) db)) - 1;
        c.hline (stageW - (db % 12 == 0 ? 3 : 2), stageW - 1, y, pal::smoke);
    }
    const int ty = base - (int) std::lround (heightPx (thr)) - 1;
    const RGB tc = mix (ironpal::dim, ironpal::heat, 0.5f + 0.5f * std::max (hover, act));
    for (int x = ironX() + ironW + 1; x < stageW - 4; ++x)
        if ((x + (int) (time * 8.0f)) % 4 < 2) c.blend (x, ty, tc, 0.7f);
    c.set (stageW - 6, ty, tc); c.vline (stageW - 5, ty - 1, ty + 1, tc); // pointer
}

void IronSEditor::drawIron (Canvas& c)
{
    const int ix = ironX(), iy = ironY() - (poke > 0.5f ? 1 : 0);
    const float glow = juce::jlimit (0.0f, 1.0f, heat * 0.9f + poke * 0.4f);

    for (int v = -1; v <= ironH; ++v)
        for (int u = -1; u <= ironW; ++u)
        {
            const int m = ironMaterial (u, v);
            const int x = ix + u, y = iy + v;
            if (m == 0)
            {
                // outline, except underneath the sole
                if (v < ironH && (ironMaterial (u - 1, v) || ironMaterial (u + 1, v) || ironMaterial (u, v - 1) || ironMaterial (u, v + 1)))
                    c.set (x, y, 0x050507);
                continue;
            }

            const bool openAbove = ironMaterial (u, v - 1) == 0, openBelow = ironMaterial (u, v + 1) == 0;
            RGB col;
            if (m == 1)      col = openAbove ? mix (0x6a6676, pal::bone, hover * 0.5f) : (openBelow ? 0x15141a : 0x2a2832);
            else if (m == 2)
            {
                col = 0x3a3744;
                if (ironMaterial (u, v - 1) != 2)      col = ironMaterial (u, v - 1) == 1 ? 0x2a2832 : mix (0x8f897d, pal::bone, hover * 0.5f);
                else if (ironMaterial (u, v - 2) == 0) col = 0x5a5664;
                else if (u > 28 && Canvas::bayer (u, v, 0.3f)) col = 0x322f3b;
                if (v >= 25) col = 0x211f28;                                           // heat shield
                if (v == 24) col = mix (ironpal::dim, ironpal::heat, 0.25f + 0.75f * glow); // glowing trim
            }
            else             col = v == 29 ? 0xa59d8f : (v == 30 ? 0x6a6676 : mix (0x4b4856, mix (ironpal::heat, ironpal::hot, glow), glow));
            c.set (x, y, col);
        }

    // temperature dial = range
    const int dx = ix + 14, dy = iy + 19;
    c.disc (dx, dy, 4.0f, 0x15141a);
    for (int k = 0; k < 24; ++k)
    {
        const float a = (float) k / 24.0f * juce::MathConstants<float>::twoPi;
        c.set (dx + (int) std::lround (std::sin (a) * 4.0f), dy - (int) std::lround (std::cos (a) * 4.0f), 0x5a5664);
    }
    const float da = juce::degreesToRadians (-135.0f + 270.0f * rangeShown / 24.0f);
    c.line (dx, dy, dx + (int) std::lround (std::sin (da) * 3.0f), dy - (int) std::lround (std::cos (da) * 3.0f), ironpal::hot);
    c.set (dx, dy, pal::bone);

    // working light
    const RGB led = bypass.isOn() ? 0x24222b : mix (ironpal::dim, ironpal::hot, juce::jlimit (0.0f, 1.0f, 0.15f + act * 1.2f));
    c.fillRect (ix + 26, iy + 18, 2, 2, led);

    // heat spilling out under the sole
    if (glow > 0.05f)
        for (int j = 0; j < 4; ++j)
            for (int u = -2; u < ironW + 2; ++u)
                if (Canvas::bayer (ix + u, iy + ironH + j, glow * (0.55f - 0.13f * (float) j)))
                    c.add (ix + u, iy + ironH + j, ironpal::heat, 0.3f);
}

void IronSEditor::drawSteam (Canvas& c)
{
    for (const auto& s : steam)
    {
        if (s.life <= 0.0f) continue;
        const int x = (int) s.x, y = (int) s.y;
        if (y < 28 || x < 1 || x >= stageW - 2) continue;
        // a puff starts as a bright dot and grows into a thinning, dithered cloud
        const float a = juce::jlimit (0.0f, 1.0f, s.life * 1.3f) * 0.8f;
        const int r = s.life > 0.8f ? 0 : (s.life > 0.45f ? 1 : 2);
        for (int j = -r; j <= r; ++j)
            for (int i = -r; i <= r; ++i)
            {
                if (i * i + j * j > r * r + 1) continue;
                if (r == 2 && ! Canvas::bayer (x + i, y + j, 0.6f)) continue;
                c.blend (x + i, y + j, i + j < 0 ? pal::bone : mix (pal::bone, pal::smoke, 0.6f), a * (r == 0 ? 1.0f : 0.7f));
            }
    }
    for (const auto& s : sparks)
        if (s.life > 0.0f && (int) s.x < stageW)
            c.set ((int) s.x, (int) s.y, mix (ironpal::heat, ironpal::hot, juce::jlimit (0.0f, 1.0f, s.life * 3.0f)));
}

void IronSEditor::drawPlaque (Canvas& c)
{
    std::string txt = "SMOOTH";
    const float total = mGr.peakValue + mSmack.peakValue;
    if (bypass.isOn()) txt = "IRON OFF";
    else if (mGr.peak > 0.02f || mSmack.peak > 0.02f) txt = "PRESSED " + juce::String (-total, 1).toStdString() + " DB";
    const int tw = font::textWidth (txt), cx = 78;
    const int x0 = cx - tw / 2 - 4, y0 = 181;
    c.fillRect (x0, y0, tw + 8, 9, 0x0c0b10);
    c.rect (x0, y0, tw + 8, 9, mix (pal::iron, ironpal::dim, heat));
    font::drawCentred (c, cx, y0 + 2, txt, mix (mix (pal::smoke, pal::bone, 0.4f), ironpal::hot, act));
}

//==============================================================================
void IronSEditor::drawTitle (Canvas& c)
{
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
                        if (g.accent) col = mix (ironpal::hot, ironpal::heat, down * (1.0f - 0.6f * heat));
                        else
                        {
                            // cold steel that starts to glow from below when the iron works
                            col = mix (pal::bone, pal::smoke, down * 0.55f);
                            if (Canvas::bayer (x, y, heat * down * 1.2f)) col = mix (col, ironpal::heat, 0.6f);
                        }
                        c.set (x, y, col);
                    }
                // drop shadow
                const RGB sh = g.accent ? ironpal::deep : (RGB) 0x15141a;
                if (j + 1 >= glyphH || g.rows[j + 1][i] != '#')
                    for (int s = 0; s < titleScale; ++s) c.set (bx + s + 1, by + titleScale, sh);
                if (g.rows[j][i + 1] != '#')
                    for (int s = 0; s < titleScale; ++s) c.set (bx + titleScale, by + s + 1, sh);
            }

    // a wisp of steam curling off the S
    for (int k = 0; k < 7; ++k)
    {
        const float t = std::fmod (time * 0.35f + (float) k / 7.0f, 1.0f);
        const int x = 97 + (int) std::lround (std::sin (t * 7.0f + (float) k) * 2.0f + t * 4.0f), y = 12 - (int) std::lround (t * 9.0f);
        c.blend (x, y, pal::bone, (1.0f - t) * (0.2f + 0.5f * heat));
    }

    font::drawText (c, 106, 18, "DE-ESSER", pal::smoke);
}

void IronSEditor::drawPanel (Canvas& c)
{
    const int x = panelX, y = panelY, w = panelW, h = panelH;
    c.fillRect (x + 1, y + 1, w - 2, h - 2, pal::ink);
    const RGB border = mix (pal::iron, ironpal::dim, 0.25f + 0.5f * heat);
    c.hline (x + 1, x + w - 2, y, border);
    c.hline (x + 1, x + w - 2, y + h - 1, border);
    c.vline (x, y + 1, y + h - 2, border);
    c.vline (x + w - 1, y + 1, y + h - 2, border);
    c.hline (x + 2, x + w - 3, y + 1, 0x17161d);
    for (int i = x + 2; i < x + w - 2; ++i) c.dither (i, 97, pal::ash, 0.5f);
}

void IronSEditor::drawMeters (Canvas& c)
{
    struct Row { const char* label; const Meter* m; bool reduction; };
    const Row rows[] = { { "IN", &mIn, false }, { "GR", &mGr, true }, { "SMK", &mSmack, true }, { "OUT", &mOut, false } };

    int y = 137;
    for (const auto& r : rows)
    {
        font::drawText (c, meterX, y, r.label, pal::smoke);
        const int lit = (int) std::lround (r.m->shown * meterSegs);
        const int pk = (int) std::lround (r.m->peak * meterSegs) - 1;
        for (int s = 0; s < meterSegs; ++s)
        {
            const float pos = (float) (s + 1) / (float) meterSegs;
            RGB col = 0x15131a;
            if (s < lit)
                col = r.reduction ? mix (ironpal::heat, ironpal::hot, pos)
                                  : (pos > 0.96f ? ironpal::hot : pos > 0.8f ? ironpal::heat : mix (pal::smoke, pal::bone, pos));
            if (s == pk && r.m->peak > 0.02f) col = pal::bone;
            c.fillRect (meterBarX + s * 2, y, 1, 5, col);
        }
        const bool quiet = r.m->peak <= 0.02f;
        const auto txt = quiet ? std::string (r.reduction ? "0.0" : "-")
                               : (r.reduction ? juce::String (-r.m->peakValue, 1) : juce::String (juce::roundToInt (r.m->peakValue))).toStdString();
        font::drawRight (c, W - 4, y, txt, quiet ? pal::ash : (r.reduction ? ironpal::hot : pal::bone));
        y += 8;
    }
}
