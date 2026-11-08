#include "PluginEditor.h"

using namespace px;

namespace
{
// 7x9 title glyphs, drawn at 3x
const char* const glyphS[] = { ".#####.", "##...##", "##.....", ".####..", "...###.", ".....##", ".....##", "##...##", ".#####." };
const char* const glyphU[] = { "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##..###", ".###.##" };
const char* const glyphB[] = { "######.", "##...##", "##...##", "######.", "##...##", "##...##", "##...##", "##...##", "######." };
const char* const* const title[] = { glyphS, glyphU, glyphB };

constexpr int titleScale = 3, glyphW = 7, glyphH = 9, titleGap = 5;
constexpr int titleW = 3 * glyphW * titleScale + 2 * titleGap;
constexpr int panelX = 141, panelY = 46, panelW = 143, panelH = 118;
constexpr int titleX = panelX + (panelW - titleW) / 2, titleY = 6;

constexpr const char* moods[] = { "SERENE", "SOLEMN", "FALTERING", "KNEELING", "DAMNED" };

// face cracks (face-local pixel offsets), revealed one by one at high darkness
constexpr int crackPts[][2] = { { -1, -6 }, { -1, -5 }, { 0, -4 }, { 4, 0 }, { 5, 1 }, { 4, 2 }, { -2, -5 }, { -4, 1 }, { -5, 2 }, { 1, -6 }, { 5, -2 } };
constexpr int numCracks = (int) (sizeof (crackPts) / sizeof (crackPts[0]));

// window cracks (absolute stage coords)
constexpr int glassCracks[][4] = { { 52, 28, 60, 46 }, { 60, 46, 55, 62 }, { 90, 22, 81, 40 }, { 81, 40, 86, 52 }, { 74, 82, 80, 96 }, { 80, 96, 76, 110 } };

float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

RGB mix3 (RGB a, RGB b, RGB c, float t) noexcept
{
    return t < 0.5f ? mix (a, b, t * 2.0f) : mix (b, c, (t - 0.5f) * 2.0f);
}

float gainToNorm (float g) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (g, -100.0f) + 48.0f) / 48.0f);
}
} // namespace

//==============================================================================
SubEditor::SubEditor (SubProcessor& p)
    : PixelEditor (p, W, H, (int) p.apvts.state.getProperty ("uiScale", 3)),
      proc (p),
      nun (*p.apvts.getParameter (params::cutoff)),
      response (*p.apvts.getParameter (params::cutoff), *p.apvts.getParameter (params::reso)),
      layerSel (*p.apvts.getParameter (params::layers)),
      kCut (*p.apvts.getParameter (params::cutoff), "CUT", "CUTOFF", "HZ",  subpal::vio),
      kRes (*p.apvts.getParameter (params::reso),   "RES", "RESO",   "%",   subpal::vio),
      kDrv (*p.apvts.getParameter (params::drive),  "DRV", "DRIVE",  "DB",  subpal::vio),
      kMix (*p.apvts.getParameter (params::mix),    "MIX", "MIX",    "%",   subpal::vio),
      kOut (*p.apvts.getParameter (params::output), "OUT", "OUTPUT", "DB",  subpal::vio)
{
    nun.bounds      = { 0, 0, stageW, floorY + 2 };
    response.bounds = { panelX + 5, panelY + 14, panelW - 10, 44 };
    layerSel.bounds = { panelX + 5, panelY + 62, panelW - 10, 9 };

    px::Knob* ks[] = { &kCut, &kRes, &kDrv, &kMix, &kOut };
    for (int i = 0; i < 5; ++i)
    {
        ks[i]->bounds = { panelX + 1 + i * 28, panelY + 78, 28, 32 };
        ks[i]->hot = subpal::hot;
        ks[i]->unlit = subpal::dim;
    }

    nun.onPoke = [this] { poke = 1.0f; };

    for (auto* w : std::initializer_list<Widget*> { &nun, &response, &layerSel, &kCut, &kRes, &kDrv, &kMix, &kOut })
        addWidget (*w);

    for (auto& pt : particles)
        pt = { random.nextFloat() * (float) stageW, random.nextFloat() * (float) floorY, 2.0f + random.nextFloat() * 5.0f, random.nextFloat() * 6.28f };

    onScaleChanged = [this] (int s) { proc.apvts.state.setProperty ("uiScale", s, nullptr); };
}

//==============================================================================
void SubEditor::tick (float dt)
{
    auto& ap = proc.apvts;
    const float cut = ap.getRawParameterValue (params::cutoff)->load();
    const float res = ap.getRawParameterValue (params::reso)->load() * 0.01f;
    const float lay = ap.getRawParameterValue (params::layers)->load();
    const float drv = ap.getRawParameterValue (params::drive)->load() / 24.0f;
    const float mx  = ap.getRawParameterValue (params::mix)->load() * 0.01f;

    // --- how dark is the filter? low cutoff dominates, then layers, resonance, drive; mix scales it
    const float cutDark = 1.0f - std::log (juce::jlimit (20.0f, 20000.0f, cut) / 20.0f) / std::log (1000.0f);
    const float raw = (0.6f * cutDark + 0.22f * (lay - 1.0f) / 3.0f + 0.1f * res + 0.08f * drv) * (0.2f + 0.8f * mx);
    const float target = juce::jlimit (0.0f, 1.0f, raw / 0.85f);
    dark += (target - dark) * ease (dt, 2.5f);

    // --- audio
    const float lvOut = gainToNorm (proc.meters.outPeak.exchange (0.0f));
    const float lvIn  = gainToNorm (proc.meters.inPeak.exchange (0.0f));
    if (lvOut - level > 0.3f && dark > 0.45f) shake = 1.0f;
    level   += (lvOut - level)   * ease (dt, lvOut > level ? 30.0f : 4.0f);
    inLevel += (lvIn - inLevel)  * ease (dt, lvIn > inLevel ? 30.0f : 4.0f);
    shake = std::max (0.0f, shake - dt * 5.0f);
    poke  = std::max (0.0f, poke - dt * 1.4f);
    shakeX = shake > 0.2f ? (float) (random.nextInt (3) - 1) : 0.0f;

    // --- she raises her head when you hover or grab her
    const bool engaged = nun.hovered || nun.isDragging();
    lookUp += ((engaged ? 1.0f : 0.0f) - lookUp) * ease (dt, engaged ? 3.0f : 1.5f);

    // --- pose: standing -> head bows -> sinks to the knees -> hunched, head down
    auto& ps = pose;
    ps.lower = smooth01 (0.2f, 0.85f, dark);
    ps.hunch = smooth01 (0.6f, 1.0f, dark);
    float tilt = 0.3f * smooth01 (0.0f, 0.35f, dark) + 0.7f * smooth01 (0.7f, 1.0f, dark);
    tilt *= 1.0f - 0.7f * lookUp;
    tilt *= 1.0f - std::min (1.0f, poke * 1.5f);
    ps.tilt = tilt;
    const float breath = std::sin (time * 1.3f);
    ps.shY    = std::round (lerp (54.0f, 92.0f, ps.lower) + 2.0f * ps.hunch + (breath > 0.55f ? -1.0f : 0.0f));
    ps.shW    = 15.0f + 2.0f * ps.hunch;
    ps.headY  = std::round (ps.shY - 18.0f + 15.0f * tilt);
    ps.headX  = (float) figX + shakeX;
    ps.waistY = std::round (lerp (92.0f, 124.0f, ps.lower));
    ps.waistW = lerp (13.0f, 18.0f, ps.lower);
    ps.hemW   = lerp (24.0f, 46.0f, ps.lower);
    ps.handsY = ps.shY + 15.0f;

    // --- gaze follows the mouse, wanders otherwise
    const auto mouse = getMouseXYRelative();
    if (getLocalBounds().contains (mouse))
        gazeTarget = juce::jlimit (-1.0f, 1.0f, ((float) mouse.x / (float) getPixelScale() - ps.headX) / 25.0f);
    else if ((wanderTimer -= dt) <= 0.0f)
    {
        wanderTimer = 1.5f + random.nextFloat() * 3.0f;
        gazeTarget = random.nextFloat() * 2.0f - 1.0f;
    }
    gazeX += (gazeTarget - gazeX) * ease (dt, 8.0f);

    // --- blinking (the damned don't blink)
    if (blink > 0.0f) blink = std::max (0.0f, blink - dt * 7.0f);
    else if ((blinkTimer -= dt) <= 0.0f && dark < 0.8f) { blink = 1.0f; blinkTimer = 2.0f + random.nextFloat() * 4.0f; }

    // --- candles
    if ((flickTimer -= dt) <= 0.0f)
    {
        flickTimer = 0.05f + random.nextFloat() * 0.12f;
        flickTargetL = 0.7f + random.nextFloat() * 0.3f + level * 0.4f * (random.nextFloat() - 0.5f);
        flickTargetR = 0.7f + random.nextFloat() * 0.3f + level * 0.4f * (random.nextFloat() - 0.5f);
    }
    flickL += (flickTargetL - flickL) * ease (dt, 20.0f);
    flickR += (flickTargetR - flickR) * ease (dt, 20.0f);

    if (dark > 0.8f && (wispTimer -= dt) <= 0.0f)
    {
        wispTimer = 0.35f + random.nextFloat() * 0.5f;
        for (auto& w : wisps)
            if (w.life <= 0.0f) { w = { random.nextBool() ? 16.0f : 122.0f, 135.0f, 1.0f }; break; }
    }
    for (auto& w : wisps)
        if (w.life > 0.0f) { w.life -= dt * 0.6f; w.y -= dt * 9.0f; w.x += std::sin (time * 3.0f + w.y * 0.3f) * dt * 4.0f; }

    // --- widgets fed with state
    layerShown += (lay - layerShown) * ease (dt, 12.0f);
    response.layerPos = layerShown;
    response.wetMix = mx;
    response.level = level;
    response.darkness = dark;
    layerSel.level = level;

    // --- title light sweep
    if (sweep < 320.0f) sweep += dt * 140.0f;
    else if ((sweepTimer -= dt) <= 0.0f) { sweep = (float) titleX - 30.0f; sweepTimer = 5.0f + random.nextFloat() * 5.0f; }

    // --- mood plaque
    const int m = dark < 0.15f ? 0 : dark < 0.35f ? 1 : dark < 0.6f ? 2 : dark < 0.85f ? 3 : 4;
    if (m != mood) { if (mood >= 0) moodFlash = 1.0f; mood = m; }
    moodFlash = std::max (0.0f, moodFlash - dt * 1.5f);

    // --- dust / embers
    for (auto& pt : particles)
    {
        pt.y -= pt.vy * dt * (1.0f + 2.0f * dark + level);
        pt.x += std::sin (time * 0.8f + pt.phase) * dt * 3.0f;
        if (pt.y < 0.0f) { pt.y = (float) floorY; pt.x = random.nextFloat() * (float) stageW; }
    }
}

//==============================================================================
void SubEditor::paintScene (Canvas& c)
{
    c.clear (pal::voidC);

    for (int y = 4; y < H; y += 8)
        for (int x = stageW + 6; x < W; x += 8)
            c.set (x, y, 0x0f0f14);
    for (int y = 4; y < H - 4; ++y) c.dither (stageW + 1, y, pal::iron, 0.5f);

    drawWindow (c);
    drawFloor (c);
    drawCandle (c, 16, flickL);
    drawCandle (c, 122, flickR);
    drawHalo (c);
    drawBody (c);
    drawHands (c);
    drawRosary (c);
    drawHead (c);
    drawHorns (c);
    drawEyeGlow (c);
    drawParticles (c);
    drawPlaque (c);

    drawTitle (c);
    font::drawCentred (c, panelX + panelW / 2, 37, "STACKED LOWPASS", pal::smoke);
    drawPanel (c);
    font::drawText (c, panelX, 168, "V0.1", pal::ash);
    font::drawRight (c, W - 4, 168, "BY N-F.PRO", pal::ash);
}

void SubEditor::paintOverlay (Canvas& c)
{
    if (auto* w = getActiveWidget())
    {
        const auto txt = w->getReadout().toUpperCase().toStdString();
        if (! txt.empty()) font::drawRight (c, panelX + panelW - 5, panelY + 3, txt, pal::bone);
    }
    else
    {
        const auto txt = "DARKNESS " + std::to_string (juce::roundToInt (dark * 100.0f)) + "%";
        font::drawRight (c, panelX + panelW - 5, panelY + 3, txt, mix (pal::smoke, subpal::hot, dark * 0.6f));
    }
}

//==============================================================================
void SubEditor::drawWindow (Canvas& c)
{
    constexpr int cx = figX, top = 10, archCy = 46, bottom = 114;
    constexpr float r = 36.0f;
    const float holy = 1.0f - smooth01 (0.0f, 0.5f, dark);
    const float hell = smooth01 (0.3f, 1.0f, dark);

    auto inside = [&] (int x, int y, float rad)
    {
        const float dx = (float) (x - cx);
        if (y >= archCy) return std::abs (dx) <= rad;
        const float dy = (float) (y - archCy);
        return dx * dx + dy * dy <= rad * rad;
    };

    for (int y = top; y <= bottom; ++y)
        for (int x = cx - (int) r; x <= cx + (int) r; ++x)
        {
            if (! inside (x, y, r)) continue;
            if (! inside (x, y, r - 2.0f)) { c.set (x, y, pal::iron); continue; }

            const float rd = std::sqrt ((float) ((x - cx) * (x - cx) + (y - 40) * (y - 40)));
            const bool lead = x == cx || y == 70 || y == 92 || std::abs (rd - 10.0f) < 0.6f
                              || (rd < 10.0f && (std::abs (x - cx) == std::abs (y - 40)));
            if (lead) { c.set (x, y, 0x16141c); continue; }

            const int pxi = (x - cx + (int) r) / 9, pyi = y / 11;
            const float hash = (float) ((pxi * 37 + pyi * 91) % 7) / 7.0f;
            RGB col = 0x09090e;
            const float lv = holy * 0.17f * (0.6f + 0.8f * hash) * (1.0f - (float) (y - top) / (float) (bottom - top) * 0.5f);
            if (Canvas::bayer (x, y, lv)) col = mix (pal::ash, pal::smoke, hash);
            const float hv = hell * (0.16f + 0.25f * level) * (0.5f + hash);
            if (Canvas::bayer (x + 2, y + 1, hv)) col = mix (subpal::dim, subpal::vio, hash * hell);
            c.set (x, y, col);
        }

    // the glass cracks when she falls
    const float crack = smooth01 (0.7f, 0.95f, dark);
    const int n = (int) std::lround (crack * (float) std::size (glassCracks));
    for (int i = 0; i < n; ++i)
    {
        const auto* g = glassCracks[i];
        c.line (g[0], g[1], g[2], g[3], pal::voidC);
        c.set (g[2], g[3], mix (subpal::dim, subpal::hot, level));
    }
}

void SubEditor::drawFloor (Canvas& c)
{
    for (int y = floorY; y < H; ++y)
        for (int x = 0; x <= stageW; ++x)
        {
            RGB col = 0x0a0a0e;
            const int depth = y - floorY;
            if (depth % 5 == 0) col = Canvas::bayer (x, y, 0.4f) ? 0x131219 : col;
            const int persp = (x - figX) * 6 / (depth + 6);
            if (persp % 8 == 0 && Canvas::bayer (x, y, 0.3f)) col = 0x121118;
            c.set (x, y, col);
        }
    // shadow under her
    const float rx = pose.hemW + 6.0f;
    for (int y = floorY - 1; y <= floorY + 3; ++y)
        for (int x = figX - (int) rx; x <= figX + (int) rx; ++x)
        {
            const float dx = (float) (x - figX) / rx, dy = (float) (y - floorY - 1) / 2.5f;
            if (dx * dx + dy * dy <= 1.0f) c.set (x, y, 0x040406);
        }
    c.hline (0, stageW, floorY, 0x141319);
}

void SubEditor::drawCandle (Canvas& c, int x, float flick)
{
    constexpr int top = 138;
    c.fillRect (x - 1, top, 3, floorY - top, 0x8a8275);
    c.vline (x - 1, top, floorY - 1, 0xa59d8f);
    c.vline (x + 1, top, floorY - 1, 0x5c564f);
    c.set (x + 1, top + 3, 0xa59d8f);
    c.set (x + 1, top + 4, 0x8a8275);
    c.set (x, top - 1, 0x2a2630);

    const float out = smooth01 (0.72f, 0.95f, dark);
    const float fh = (1.0f - out) * flick;
    if (fh < 0.05f) return;

    const RGB outer = mix (pal::smoke, subpal::vio, smooth01 (0.2f, 0.7f, dark));
    const RGB core  = mix (pal::bone, subpal::hot, smooth01 (0.4f, 0.9f, dark));

    const float rad = 6.0f + 6.0f * fh;
    const int ir = (int) rad;
    for (int j = -ir; j <= ir; ++j)
        for (int i = -ir; i <= ir; ++i)
        {
            const float d = std::sqrt ((float) (i * i + j * j));
            if (d >= rad) continue;
            const float lv = 0.45f * fh * (1.0f - d / rad) * (1.0f - d / rad);
            if (Canvas::bayer (x + i, top - 3 + j, lv)) c.add (x + i, top - 3 + j, outer, 0.22f);
        }

    const int h = 1 + (int) std::lround (fh * 4.0f);
    c.set (x - 1, top - 2, outer);
    c.set (x + 1, top - 2, outer);
    if (h > 2) { c.set (x - 1, top - 3, outer); c.set (x + 1, top - 3, outer); }
    for (int k = 0; k < h; ++k)
        c.set (x + (k == h - 1 && flick > 0.93f ? 1 : 0), top - 2 - k, k < h - 1 ? core : outer);
}

void SubEditor::drawHalo (Canvas& c)
{
    const float hx = pose.headX, hy = pose.headY - 2.0f;
    const float holy = 1.0f - smooth01 (0.0f, 0.35f, dark);
    if (holy > 0.01f)
    {
        constexpr int steps = 120;
        for (int i = 0; i < steps; ++i)
        {
            if (dark > 0.12f && ((i * 7 + (int) (time * 3.0f)) % 11) < (int) (dark * 20.0f)) continue; // crumbling
            const float a = (float) i / (float) steps * juce::MathConstants<float>::twoPi;
            c.blend ((int) std::lround (hx + std::cos (a) * 14.0f), (int) std::lround (hy + std::sin (a) * 13.0f),
                     pal::bone, 0.5f * holy * (0.8f + 0.2f * std::sin (time * 2.0f)));
        }
    }

    const float crown = smooth01 (0.65f, 1.0f, dark);
    if (crown > 0.01f)
    {
        const RGB col = mix (subpal::dim, subpal::vio, crown * (0.6f + 0.4f * level));
        for (int s = 0; s < 10; ++s)
        {
            const float a = (float) s / 10.0f * juce::MathConstants<float>::twoPi + time * 0.4f;
            const float r0 = 13.0f, r1 = 13.0f + 4.5f * crown * (0.8f + 0.4f * level);
            const int x1 = (int) std::lround (hx + std::cos (a) * r1), y1 = (int) std::lround (hy + std::sin (a) * r1);
            c.line ((int) std::lround (hx + std::cos (a) * r0), (int) std::lround (hy + std::sin (a) * r0), x1, y1, col);
            c.set (x1, y1, mix (col, subpal::hot, level));
        }
    }
}

//==============================================================================
float SubEditor::bodyHalfWidth (float y) const noexcept
{
    const auto& p = pose;
    if (y < p.shY - 4.0f || y > (float) floorY) return -1.0f;
    if (y < p.shY)
    {
        const float t = (p.shY - y) / 4.5f;
        return p.shW * std::sqrt (std::max (0.0f, 1.0f - t * t));
    }
    if (y < p.waistY)
    {
        const float t = (y - p.shY) / std::max (1.0f, p.waistY - p.shY);
        return lerp (p.shW, p.waistW, t) + 1.5f * std::sin (t * 3.14159f);
    }
    const float t = (y - p.waistY) / std::max (1.0f, (float) floorY - p.waistY);
    return lerp (p.waistW, p.hemW, std::pow (t, lerp (1.5f, 0.5f, p.lower)));
}

bool SubEditor::insideBody (float x, float y) const noexcept
{
    const float cx = (float) figX;
    const float hw = bodyHalfWidth (y);
    if (hw >= 0.0f && std::abs (x - cx) <= hw) return true;

    // veil hanging from the head down over the shoulders
    const float vTop = pose.headY, vBot = pose.shY + 10.0f;
    if (y >= vTop && y <= vBot && vBot > vTop)
    {
        const float t = (y - vTop) / (vBot - vTop);
        if (std::abs (x - lerp (pose.headX, cx, t)) <= lerp (9.5f, pose.shW + 1.5f, t)) return true;
    }

    // knees pushing through the robe when kneeling
    if (pose.lower > 0.35f && y <= (float) floorY)
    {
        const float kr = smooth01 (0.35f, 1.0f, pose.lower);
        for (float s : { -1.0f, 1.0f })
        {
            const float dx = (x - (cx + s * 11.0f)) / 10.0f, dy = (y - ((float) floorY - 2.0f)) / (2.0f + 4.0f * kr);
            if (dx * dx + dy * dy <= 1.0f) return true;
        }
    }
    return false;
}

float SubEditor::armsDown() const noexcept { return smooth01 (0.78f, 1.0f, dark) * (1.0f - lookUp * 0.5f); }

float SubEditor::handX (float side) const noexcept
{
    return (float) figX + side * lerp (5.0f, 15.0f, armsDown());
}

float SubEditor::handY() const noexcept
{
    return lerp (pose.handsY + 4.0f, (float) floorY - 1.0f, armsDown());
}

RGB SubEditor::skinColour() const noexcept
{
    return mix3 (0xc9bdae, 0x7a6f80, 0x3b3345, dark);
}

void SubEditor::drawBody (Canvas& c)
{
    const float hell = smooth01 (0.3f, 1.0f, dark);
    const int yTop = (int) std::min (pose.headY, pose.shY - 4.0f);
    const int xr = (int) pose.hemW + 14;
    const RGB rimL = mix (0x3c3846, subpal::vio, hell * (0.5f + 0.5f * level));
    const RGB rimR = mix (0x24212c, subpal::dim, hell);

    for (int y = yTop; y <= floorY; ++y)
        for (int x = figX - xr; x <= figX + xr; ++x)
        {
            const float fx = (float) x, fy = (float) y;
            if (! insideBody (fx, fy)) continue;

            RGB col = 0x100f15;
            const float hw = std::max (4.0f, bodyHalfWidth (fy));
            const float u = (fx - (float) figX) / hw;
            const float fold = std::sin (u * 7.5f + fy * 0.05f);
            if (Canvas::bayer (x, y, std::max (0.0f, fold) * 0.2f)) col = 0x1b1922;

            const bool edge = ! insideBody (fx - 1.0f, fy) || ! insideBody (fx + 1.0f, fy) || ! insideBody (fx, fy - 1.0f);
            if (edge) col = x < figX ? rimL : rimR;
            c.set (x, y, col);
        }

    // arms from the shoulders to the hands (praying at the chest, or down on the floor)
    const RGB crease = 0x2a2733;
    for (float s : { -1.0f, 1.0f })
    {
        const float hx = handX (s), hy = handY();
        const int sx0 = (int) ((float) figX + s * (pose.shW - 3.0f)), sy0 = (int) pose.shY + 3;
        c.line (sx0, sy0, (int) hx, (int) hy - 2, crease);
        c.line (sx0 + (int) s, sy0, (int) hx + (int) s, (int) hy - 2, 0x15131b);
        // bell sleeve around the hands
        const int sx = (int) hx, sy = (int) hy - 2;
        for (int j = -3; j <= 3; ++j)
            for (int i = -5; i <= 5; ++i)
            {
                const float d = (float) (i * i) / 25.0f + (float) (j * j) / 9.0f;
                if (d <= 1.0f) c.set (sx + i, sy + j, d > 0.6f && j < 0 ? 0x1d1b25 : 0x0e0d13);
            }
    }

    // white wimple bib under the face
    const float bTop = pose.headY + 6.0f, bBot = pose.shY + 6.0f;
    const RGB coif = mix3 (0xb8b0a2, 0x6f6874, 0x2e2a35, dark);
    for (int y = (int) bTop; y <= (int) bBot; ++y)
    {
        const float t = (bBot > bTop) ? ((float) y - bTop) / (bBot - bTop) : 0.0f;
        const int half = (int) lerp (5.0f, 8.0f, t);
        for (int x = figX - half; x <= figX + half; ++x)
        {
            const float sh = (float) (x - figX) / (float) half * 0.5f + t * 0.5f;
            c.set (x, y, Canvas::bayer (x, y, 0.55f - sh) ? scale (coif, 0.85f) : scale (coif, 0.6f));
        }
        if (y == (int) bBot) c.hline (figX - half + 1, figX + half - 1, y, scale (coif, 0.55f));
    }
}

void SubEditor::drawHands (Canvas& c)
{
    const RGB skin = skinColour();
    const RGB shade = scale (skin, 0.7f);
    const float claws = smooth01 (0.6f, 0.85f, dark);

    if (armsDown() > 0.3f)
    {
        // palms flat on the floor, claws scratching
        for (float s : { -1.0f, 1.0f })
        {
            const int hx = (int) handX (s), hy = (int) handY();
            c.fillRect (hx - 1, hy - 1, 3, 2, skin);
            c.set (hx + (int) s, hy, shade);
            c.set (hx - (int) s, hy, shade);
            if (claws > 0.5f)
                for (int k = -1; k <= 1; ++k) c.set (hx + k, hy + 1, mix (0x1a1520, subpal::vio, level * 0.6f));
        }
        return;
    }

    const int hx = figX - 2, hy = (int) pose.handsY - 4;
    static const char* const rows[] = { ".##.", ".##.", "####", "####", "####" };
    for (int j = 0; j < 5; ++j)
        for (int i = 0; i < 4; ++i)
            if (rows[j][i] == '#')
                c.set (hx + i, hy + j, i >= 2 ? shade : skin);
    c.vline (hx + 1, hy, hy + 2, shade); // the gap between the palms

    // claws grow at high darkness
    if (claws > 0.5f)
    {
        c.set (hx + 1, hy - 1, 0x1a1520);
        c.set (hx + 2, hy - 1, 0x1a1520);
        if (claws > 0.9f) c.set (hx + 1, hy - 2, mix (0x1a1520, subpal::vio, level));
    }
}

void SubEditor::drawRosary (Canvas& c)
{
    const float sway = std::sin (time * 1.2f) * 1.5f + shakeX;
    const RGB bead = mix (0x6d665e, subpal::dim, dark);
    const float ax = (float) figX, ay = lerp (pose.handsY + 2.0f, pose.shY + 10.0f, armsDown());
    float bx = ax, by = ay;
    for (int i = 1; i <= 6; ++i)
    {
        const float t = (float) i / 6.0f;
        bx = ax + sway * t;
        by = ay + (float) i * 3.0f;
        c.set ((int) std::lround (bx), (int) std::lround (by), bead);
        if (i % 2 == 0) c.set ((int) std::lround (bx), (int) std::lround (by) - 1, scale (bead, 0.6f));
    }

    // the cross turns upside down as she falls
    const float theta = juce::MathConstants<float>::pi * smooth01 (0.4f, 0.6f, dark);
    const float ccx = bx, ccy = by + 5.0f;
    const float cs = std::cos (theta), sn = std::sin (theta);
    const float evil = smooth01 (0.45f, 0.7f, dark);
    const RGB col = mix (0x8f877a, mix (subpal::vio, subpal::hot, level * 0.7f), evil);
    for (int j = -5; j <= 5; ++j)
        for (int i = -5; i <= 5; ++i)
        {
            const float lu = (float) i * cs + (float) j * sn;
            const float lv = -(float) i * sn + (float) j * cs;
            const bool on = (std::abs (lu) < 0.55f && lv > -4.5f && lv < 4.5f) || (std::abs (lv + 2.0f) < 0.55f && std::abs (lu) < 2.5f);
            if (on) c.set ((int) std::lround (ccx) + i, (int) std::lround (ccy) + j, col);
            else if (evil > 0.5f && level > 0.2f && std::abs (lu) < 2.0f && std::abs (lv) < 5.5f && Canvas::bayer (i + 64, j + 64, 0.2f * level))
                c.add ((int) std::lround (ccx) + i, (int) std::lround (ccy) + j, subpal::vio, 0.3f);
        }
}

//==============================================================================
void SubEditor::drawHead (Canvas& c)
{
    const float hx = pose.headX, hy = pose.headY, tilt = pose.tilt;
    const float f  = std::max (0.15f, 1.0f - 0.85f * tilt); // vertical foreshortening of the face
    const float fx = hx, fy = hy + 1.0f + 4.0f * tilt;
    const float hell = smooth01 (0.3f, 1.0f, dark);
    const float anger = smooth01 (0.25f, 0.6f, dark);

    const RGB skin = skinColour();
    const RGB coif = mix3 (0xb8b0a2, 0x6f6874, 0x2e2a35, dark);
    const RGB lip  = mix (0x5a4650, 0x060507, dark);
    const RGB brow = mix (0x4a4048, 0x050406, dark);
    const RGB sclera = mix (0xd8d0c2, 0x040305, smooth01 (0.3f, 0.55f, dark));
    const RGB iris = mix (0x2a2430, mix (subpal::vio, subpal::hot, std::min (1.0f, level * 0.6f + poke * 0.6f)), smooth01 (0.35f, 0.65f, dark));
    const int look = juce::jlimit (-1, 1, (int) std::lround (gazeX));

    const int mouthHw = dark > 0.5f ? 3 : 2;
    const int corner  = dark < 0.2f ? 1 : dark < 0.4f ? 0 : dark < 0.8f ? -1 : 1; // smile, flat, frown, evil grin
    const int open    = (dark > 0.75f ? 1 : 0) + (int) std::lround (level * smooth01 (0.6f, 0.9f, dark) * 2.0f)
                        + (poke > 0.3f && dark > 0.5f ? 2 : 0);
    const int tearLen = (int) std::lround (smooth01 (0.35f, 0.8f, dark) * 5.0f);
    const int cracks  = (int) std::lround (smooth01 (0.7f, 1.0f, dark) * (float) numCracks);
    const int fxi = (int) std::lround (fx);

    for (int y = (int) hy - 12; y <= (int) hy + 13; ++y)
        for (int x = (int) hx - 11; x <= (int) hx + 11; ++x)
        {
            const float dxv = ((float) x - hx) / 10.0f, dyv = ((float) y - hy) / 11.0f;
            const float dv = dxv * dxv + dyv * dyv;
            const float dxc = ((float) x - fx) / 7.5f, dyc = ((float) y - fy) / (9.5f * f);
            const float dxf = ((float) x - fx) / 6.0f, dyf = ((float) y - fy) / (8.0f * f);
            const bool inVeil = dv <= 1.0f, inCoif = dxc * dxc + dyc * dyc <= 1.0f, inFace = dxf * dxf + dyf * dyf <= 1.0f;
            if (! inVeil && ! inCoif) continue;

            RGB col;
            if (inFace)
            {
                // --- skin with simple top-left lighting, hellish uplight when dark
                const float l = 0.55f - 0.3f * dxf - 0.2f * dyf;
                col = Canvas::bayer (x, y, l) ? skin : scale (skin, 0.78f);
                if (dxf * dxf + dyf * dyf > 0.7f) col = scale (col, 0.85f);
                if (hell > 0.0f && dyf > 0.2f && Canvas::bayer (x, y, hell * 0.5f * dyf)) col = mix (col, subpal::vio, 0.35f);

                const int iu = x - fxi;
                const int iv = (int) std::lround (((float) y - fy) / f);

                for (int s : { -1, 1 })
                {
                    const int ec = 3 * s;
                    // sunken sockets
                    if (iv >= -2 && iv <= 0 && std::abs (iu - ec) <= 1) col = mix (col, 0x1a1520, 0.15f + 0.45f * dark);
                    // brows: soft and raised when serene, angled down when wrathful
                    const int a = std::abs (iu);
                    if (a >= 2 && a <= 4 && (iu > 0) == (s > 0))
                    {
                        int bv = -3;
                        if (anger > 0.5f)      bv = a == 2 ? -2 : (a == 4 ? -4 : -3);
                        else if (dark < 0.15f) bv = a == 2 ? -4 : -3;
                        if (iv == bv) col = brow;
                    }
                    // eyes
                    if (iv == -1 && std::abs (iu - ec) <= 1)
                    {
                        if (blink > 0.0f)                         col = scale (skin, 0.55f);
                        else if (anger > 0.5f && iu == ec - s)    col = scale (skin, 0.5f); // squint
                        else                                      col = (iu == ec + look) ? iris : sclera;
                    }
                    // black tears
                    if (tearLen > 0 && iu == ec && iv >= 0 && iv <= tearLen)
                        col = (iv == tearLen) ? mix (0x050406, subpal::vio, level * 0.5f) : 0x050406;
                }

                // mouth
                if (std::abs (iu) <= mouthHw)
                {
                    const int lipV = 4 - (std::abs (iu) == mouthHw ? corner : 0);
                    if (iv == lipV) col = lip;
                    if (open > 0 && std::abs (iu) < mouthHw)
                    {
                        if (iv > lipV && iv <= lipV + open)
                        {
                            col = (iu == 0 && iv == lipV + open) ? mix (subpal::deep, subpal::vio, level) : (RGB) 0x040305;
                            if (dark > 0.72f && std::abs (iu) == mouthHw - 1 && iv == lipV + 1) col = pal::bone; // fangs
                        }
                        if (iv == lipV + open + 1) col = lip;
                    }
                }

                // cracks
                for (int i = 0; i < cracks; ++i)
                    if (iu == crackPts[i][0] && iv == crackPts[i][1])
                        col = mix (0x0a080d, subpal::vio, level * 0.8f);
            }
            else if (inCoif)
            {
                const float sh = dxc * 0.4f + dyc * 0.4f;
                col = Canvas::bayer (x, y, 0.75f - sh) ? coif : scale (coif, 0.72f);
            }
            else
            {
                col = 0x100f15;
                if (dyv < -0.45f && dxv < 0.1f && Canvas::bayer (x, y, 0.22f + 0.2f * tilt)) col = 0x1d1b25; // sheen on the veil
                if (dv > 0.8f) col = dxv < 0.0f ? mix (0x3c3846, subpal::vio, hell * (0.5f + 0.5f * level)) : mix (0x24212c, subpal::dim, hell);
            }
            c.set (x, y, col);
        }
}

void SubEditor::drawHorns (Canvas& c)
{
    const float g = smooth01 (0.55f, 0.85f, dark);
    if (g < 0.02f) return;
    const float len = 13.0f * g;
    const int n = (int) (len * 2.0f) + 1;
    for (float s : { -1.0f, 1.0f })
    {
        const float bx = pose.headX + s * 5.0f, by = pose.headY - 8.0f;
        for (int i = 0; i <= n; ++i)
        {
            const float t = (float) i / (float) n;
            const float x = bx + s * (t * 3.0f + std::sin (t * 2.6f) * 3.5f);
            const float y = by - t * len * (1.0f - 0.45f * pose.tilt) + t * t * len * 0.25f * pose.tilt;
            const float r = (1.0f - t) * 1.1f + 0.3f;
            RGB col = mix (0x3a3442, 0x6e6377, (1.0f - t) * 0.6f);
            if (t > 0.8f) col = mix (col, mix (subpal::vio, subpal::hot, level), (t - 0.8f) * 5.0f * (0.5f + 0.5f * level));
            c.disc ((int) std::lround (x), (int) std::lround (y), r, col);
        }
    }
}

void SubEditor::drawEyeGlow (Canvas& c)
{
    const float g = smooth01 (0.45f, 0.9f, dark) * (0.45f + 0.55f * level) + poke * smooth01 (0.3f, 0.6f, dark);
    if (g < 0.05f || blink > 0.0f) return;

    const float f  = std::max (0.15f, 1.0f - 0.85f * pose.tilt);
    const float fy = pose.headY + 1.0f + 4.0f * pose.tilt;
    const int look = juce::jlimit (-1, 1, (int) std::lround (gazeX));
    const int ey = pose.tilt > 0.65f ? (int) std::lround (fy) : (int) std::lround (fy - f);
    const RGB core = mix (subpal::vio, subpal::hot, std::min (1.0f, g));

    for (int s : { -1, 1 })
    {
        const int ex = (int) std::lround (pose.headX) + 3 * s + look;
        for (int j = -4; j <= 4; ++j)
            for (int i = -4; i <= 4; ++i)
            {
                const float d = std::sqrt ((float) (i * i + j * j));
                if (d < 0.5f || d > 4.0f) continue;
                if (Canvas::bayer (ex + i, ey + j, std::min (1.0f, g) * (1.0f - d / 4.0f)))
                    c.add (ex + i, ey + j, subpal::vio, 0.35f);
            }
        c.set (ex, ey, core);
    }
}

void SubEditor::drawParticles (Canvas& c)
{
    for (const auto& pt : particles)
    {
        const float tw = 0.5f + 0.5f * std::sin (time * 3.0f + pt.phase * 5.0f);
        const RGB dust = mix (0x16151b, 0x34313d, tw);
        const RGB ember = mix (subpal::dim, subpal::vio, tw * (0.6f + 0.4f * level));
        c.set ((int) pt.x, (int) pt.y, mix (dust, ember, smooth01 (0.4f, 0.9f, dark)));
    }
    for (const auto& w : wisps)
        if (w.life > 0.0f) c.blend ((int) w.x, (int) w.y, pal::smoke, w.life * 0.6f);
}

void SubEditor::drawPlaque (Canvas& c)
{
    if (mood < 0) return;
    const std::string txt = moods[mood];
    const int tw = font::textWidth (txt);
    const int x0 = figX - tw / 2 - 4, y0 = 163;
    c.fillRect (x0, y0, tw + 8, 9, 0x0c0b10);
    c.rect (x0, y0, tw + 8, 9, mix (pal::iron, subpal::dim, dark));
    font::drawCentred (c, figX, y0 + 2, txt, mix (mix (pal::smoke, pal::bone, 0.4f + 0.3f * (1.0f - dark)), subpal::hot, moodFlash));
}

//==============================================================================
void SubEditor::drawTitle (Canvas& c)
{
    const float glow = 0.3f + 0.7f * dark;
    for (int g = 0; g < 3; ++g)
    {
        const int gx = titleX + g * (glyphW * titleScale + titleGap);
        for (int j = 0; j < glyphH; ++j)
            for (int i = 0; i < glyphW; ++i)
            {
                if (title[g][j][i] != '#') continue;
                for (int sy = 0; sy < titleScale; ++sy)
                    for (int sx = 0; sx < titleScale; ++sx)
                    {
                        const int x = gx + i * titleScale + sx, y = titleY + j * titleScale + sy;
                        RGB col = mix (pal::bone, pal::smoke, (float) j / (glyphH - 1) * 0.5f + dark * 0.3f);
                        // violet seeping up from below as it gets darker
                        if (Canvas::bayer (x, y, glow * (float) j / (float) glyphH * 0.8f)) col = mix (col, subpal::vio, 0.5f * glow);
                        const float band = (float) x - sweep + (float) y * 0.6f;
                        if (band >= 0.0f && band < 3.0f) col = mix (col, 0xffffff, 0.5f);
                        c.set (x, y, col);
                    }
                for (int s = 0; s < titleScale; ++s)
                {
                    const int bx = gx + i * titleScale, by = titleY + j * titleScale;
                    if (j + 1 >= glyphH || title[g][j + 1][i] != '#') c.set (bx + s + 1, by + titleScale, subpal::deep);
                    if (i + 1 >= glyphW || title[g][j][i + 1] != '#') c.set (bx + titleScale, by + s + 1, subpal::deep);
                }
            }
    }
}

void SubEditor::drawPanel (Canvas& c)
{
    const int x = panelX, y = panelY, w = panelW, h = panelH;
    c.fillRect (x + 1, y + 1, w - 2, h - 2, pal::ink);
    const RGB border = mix (pal::iron, subpal::dim, 0.3f + 0.5f * dark);
    c.hline (x + 1, x + w - 2, y, border);
    c.hline (x + 1, x + w - 2, y + h - 1, border);
    c.vline (x, y + 1, y + h - 2, border);
    c.vline (x + w - 1, y + 1, y + h - 2, border);
    c.hline (x + 2, x + w - 3, y + 1, 0x17161d);
    for (int i = x + 2; i < x + w - 2; ++i) c.dither (i, y + 10, pal::ash, 0.5f);
    font::drawText (c, x + 5, y + 3, "FILTER", pal::smoke);
}
