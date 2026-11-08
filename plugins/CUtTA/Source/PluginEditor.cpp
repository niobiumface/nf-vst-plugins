#include "PluginEditor.h"

using namespace px;

namespace
{
// 7x9 title glyphs, drawn at 3x. Index 2 is the inverted cross that replaces the first "t".
const char* const glyphC[]     = { ".#####.", "##...##", "##.....", "##.....", "##.....", "##.....", "##.....", "##...##", ".#####." };
const char* const glyphU[]     = { "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##...##", "##..###", ".###.##" };
const char* const glyphCross[] = { "..###..", "..###..", "..###..", "..###..", "..###..", "#######", "#######", "..###..", "..###.." };
const char* const glyphT[]     = { "#######", "#.###.#", "..###..", "..###..", "..###..", "..###..", "..###..", "..###..", ".#####." };
const char* const glyphA[]     = { "..###..", ".##.##.", "##...##", "##...##", "#######", "##...##", "##...##", "##...##", "##...##" };
const char* const* const title[] = { glyphC, glyphU, glyphCross, glyphT, glyphA };

constexpr int titleScale = 3, glyphW = 7, glyphH = 9, titleGap = 4;
constexpr int titleW = 5 * glyphW * titleScale + 4 * titleGap;
constexpr int titleX = (CuttaEditor::W - titleW) / 2, titleY = 6;
constexpr int crossX = titleX + 2 * (glyphW * titleScale + titleGap);
constexpr int dripX = crossX + 3 * titleScale + 1, dripY0 = titleY + glyphH * titleScale, dripFloor = 46;

constexpr float scopeFloorDb = -72.0f;

// distance field around the cross for the dithered glow (computed once)
constexpr int glowPad = 7, glowW = glyphW * titleScale + 2 * glowPad, glowH = glyphH * titleScale + 2 * glowPad;
const std::array<float, glowW * glowH>& crossDistance()
{
    static const auto field = []
    {
        std::array<float, glowW * glowH> d {};
        for (int y = 0; y < glowH; ++y)
            for (int x = 0; x < glowW; ++x)
            {
                float best = 99.0f;
                for (int j = 0; j < glyphH * titleScale; ++j)
                    for (int i = 0; i < glyphW * titleScale; ++i)
                        if (glyphCross[j / titleScale][i / titleScale] == '#')
                        {
                            const float dx = (float) (x - glowPad - i), dy = (float) (y - glowPad - j);
                            best = std::min (best, std::sqrt (dx * dx + dy * dy));
                        }
                d[(size_t) (y * glowW + x)] = best;
            }
        return d;
    }();
    return field;
}

// tiny inverted-cross ornament (3x5)
void ornament (Canvas& c, int x, int y, RGB col)
{
    c.vline (x + 1, y, y + 4, col);
    c.hline (x, x + 2, y + 3, col);
}

float toDb (float g) { return sdsp::gainToDb (g); }
} // namespace

//==============================================================================
CuttaEditor::CuttaEditor (CuttaProcessor& p)
    : PixelEditor (p, W, H, (int) p.apvts.state.getProperty ("uiScale", 3)),
      proc (p),
      gateToggle (*p.apvts.getParameter (params::gateOn), "GATE"),
      limToggle  (*p.apvts.getParameter (params::limOn),  "LIMIT"),
      kThr  (*p.apvts.getParameter (params::gateThresh),  "THR", "THRESH",  "DB"),
      kRng  (*p.apvts.getParameter (params::gateRange),   "RNG", "RANGE",   "DB"),
      kAtk  (*p.apvts.getParameter (params::gateAttack),  "ATK", "ATTACK",  "MS"),
      kHld  (*p.apvts.getParameter (params::gateHold),    "HLD", "HOLD",    "MS"),
      kRel  (*p.apvts.getParameter (params::gateRelease), "REL", "RELEASE", "MS"),
      kDrv  (*p.apvts.getParameter (params::limDrive),    "DRV", "DRIVE",   "DB"),
      kCeil (*p.apvts.getParameter (params::limCeiling),  "CEIL", "CEIL",   "DB"),
      kLRel (*p.apvts.getParameter (params::limRelease),  "REL", "REL",     "MS"),
      inMeter  ("IN",  VMeter::Mode::level,     -60.0f),
      grMeter  ("GR",  VMeter::Mode::reduction, -24.0f),
      outMeter ("OUT", VMeter::Mode::level,     -60.0f)
{
    gateToggle.bounds = { 12, 54, 30, 5 };
    limToggle.bounds  = { 163, 54, 34, 5 };

    px::Knob* gk[] = { &kThr, &kRng, &kAtk, &kHld, &kRel };
    for (int i = 0; i < 5; ++i) gk[i]->bounds = { 11 + i * 27, 110, 27, 32 };
    px::Knob* lk[] = { &kDrv, &kCeil, &kLRel };
    for (int i = 0; i < 3; ++i) lk[i]->bounds = { 162 + i * 28, 110, 28, 32 };

    inMeter.bounds  = { 172, 63, 8, 44 };
    grMeter.bounds  = { 200, 63, 8, 44 };
    outMeter.bounds = { 228, 63, 8, 44 };

    presetSel.bounds = { 66, 153, 124, 7 };
    presetSel.getText = [this]
    {
        const int i = proc.getCurrentProgram();
        return proc.getProgramName (i) + (proc.isPresetModified() ? "*" : "");
    };
    presetSel.onStep = [this] (int dir)
    {
        proc.setCurrentProgram ((proc.getCurrentProgram() + dir + presets::count) % presets::count);
    };
    presetSel.onClick = [this] { showPresetMenu(); };

    menuLnf.setColourScheme ({ 0xff0e0e13, 0xff060608, 0xff0e0e13, 0xff2e2c36, 0xffcfc6b4,
                               0xff1b1a22, 0xffcfc6b4, 0xffb3121f, 0xffcfc6b4 });

    for (auto* w : std::initializer_list<Widget*> { &gateToggle, &limToggle, &kThr, &kRng, &kAtk, &kHld, &kRel,
                                                    &kDrv, &kCeil, &kLRel, &inMeter, &grMeter, &outMeter, &presetSel })
        addWidget (*w);

    histLevel.fill (-100.0f);
    histGate.fill (0.0f);

    for (auto& pt : particles)
        pt = { random.nextFloat() * W, random.nextFloat() * H, 0.0f, -(2.0f + random.nextFloat() * 6.0f),
               random.nextFloat(), random.nextFloat() * 6.28f, random.nextFloat() < 0.15f };

    onScaleChanged = [this] (int s) { proc.apvts.state.setProperty ("uiScale", s, nullptr); };
}

void CuttaEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&menuLnf);
    menu.addSectionHeader ("PRESETS");
    for (int i = 0; i < presets::count; ++i)
        menu.addItem (i + 1, presets::factory[i].name, true, i == proc.getCurrentProgram());

    const auto sb = presetSel.bounds * getPixelScale();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (sb)),
                        [safe = juce::Component::SafePointer<CuttaEditor> (this)] (int result)
                        {
                            if (safe != nullptr && result > 0)
                            {
                                safe->proc.setCurrentProgram (result - 1);
                                safe->presetSel.flashNow();
                            }
                        });
}

//==============================================================================
void CuttaEditor::tick (float dt)
{
    auto& m = proc.meters;

    // --- meters
    const float gateGain = m.gateGain.load (std::memory_order_relaxed);
    const bool  gateOpen = m.gateOpen.load (std::memory_order_relaxed);
    pendingPeak = std::max (pendingPeak, m.inPeak.exchange (0.0f));
    const float limGain = m.limGain.exchange (1.0f);

    inMeter.setDb  (toDb (m.preLim.exchange (0.0f)));
    outMeter.setDb (toDb (m.outPeak.exchange (0.0f)));
    grMeter.setDb  (toDb (limGain));
    outMeter.markerDb = proc.apvts.getRawParameterValue (params::limCeiling)->load();

    gateShown     += (gateGain - gateShown) * ease (dt, 20.0f);
    gateOpenShown += ((gateOpen ? 1.0f : 0.0f) - gateOpenShown) * ease (dt, gateOpen ? 30.0f : 6.0f);
    limActivity   += (juce::jlimit (0.0f, 1.0f, -toDb (limGain) / 6.0f) - limActivity) * ease (dt, 12.0f);
    grDbShown     += (toDb (limGain) - grDbShown) * ease (dt, toDb (limGain) < grDbShown ? 30.0f : 4.0f);
    limToggle.pulse = limActivity;
    gateToggle.pulse = gateOpenShown;

    const bool gOn = gateToggle.isOn(), lOn = limToggle.isOn();
    for (auto* k : { &kThr, &kRng, &kAtk, &kHld, &kRel }) k->enabled = gOn;
    for (auto* k : { &kDrv, &kCeil, &kLRel }) k->enabled = lOn;
    inMeter.enabled = grMeter.enabled = outMeter.enabled = lOn;

    // --- scope history at a fixed 40 Hz
    histAccum += dt;
    while (histAccum >= 1.0f / 40.0f)
    {
        histAccum -= 1.0f / 40.0f;
        histLevel[(size_t) histPos] = toDb (pendingPeak);
        histGate[(size_t) histPos]  = gateGain;
        histPos = (histPos + 1) % (int) histLevel.size();
        pendingPeak = 0.0f;
    }

    // --- candle flicker of the cross
    if ((flickerTimer -= dt) <= 0.0f)
    {
        flickerTimer  = 0.04f + random.nextFloat() * 0.16f;
        flickerTarget = random.nextFloat() < 0.05f ? 0.35f : 0.78f + random.nextFloat() * 0.22f;
    }
    flicker += (flickerTarget - flicker) * ease (dt, 25.0f);

    // --- light sweep across the letters
    if (sweep < 300.0f) sweep += dt * 140.0f;
    else if ((sweepTimer -= dt) <= 0.0f) { sweep = (float) titleX - 30.0f; sweepTimer = 5.0f + random.nextFloat() * 5.0f; }

    // --- ash particles
    for (auto& pt : particles)
    {
        pt.y += pt.vy * dt;
        pt.x += pt.vx * dt + std::sin (time * 0.7f + pt.phase) * dt * 3.0f;
        pt.life += dt * 0.3f;
        if (pt.y < -2.0f)
        {
            pt.y = (float) H + 2.0f;
            pt.x = random.nextFloat() * W;
            pt.ember = random.nextFloat() < 0.15f;
        }
    }

    // --- blood drip from the cross
    switch (drip.state)
    {
        case Drip::idle:
            if ((drip.timer -= dt * (1.0f + 2.5f * gateOpenShown)) <= 0.0f) { drip.state = Drip::forming; drip.t = 0.0f; }
            break;
        case Drip::forming:
            if ((drip.t += dt) > 0.9f) { drip.state = Drip::falling; drip.y = (float) dripY0 + 1.0f; drip.vy = 0.0f; }
            break;
        case Drip::falling:
            drip.vy += 260.0f * dt;
            drip.y  += drip.vy * dt;
            if (drip.y >= (float) dripFloor)
            {
                drip.state = Drip::splash; drip.t = 0.0f;
                for (auto& s : sparks)
                    s = { (float) dripX, (float) dripFloor - 1.0f, (random.nextFloat() - 0.5f) * 70.0f,
                          -(25.0f + random.nextFloat() * 40.0f), 0.5f + random.nextFloat() * 0.4f };
            }
            break;
        case Drip::splash:
            if ((drip.t += dt) > 1.4f) { drip.state = Drip::idle; drip.timer = 4.0f + random.nextFloat() * 7.0f; }
            break;
    }
    for (auto& s : sparks)
    {
        if (s.life <= 0.0f) continue;
        s.vy += 220.0f * dt; s.x += s.vx * dt; s.y += s.vy * dt; s.life -= dt;
        if (s.y > (float) dripFloor) { s.y = (float) dripFloor; s.vy *= -0.3f; s.vx *= 0.5f; }
    }
}

//==============================================================================
void CuttaEditor::paintScene (Canvas& c)
{
    c.clear (pal::voidC);

    // faint dot grid
    for (int y = 4; y < H; y += 8)
        for (int x = 4; x < W; x += 8)
            c.set (x, y, 0x0f0f14);

    drawParticles (c);
    drawTitle (c);
    drawDrip (c);

    font::drawCentred (c, W / 2, 39, "NOISE GATE + LIMITER", pal::smoke);

    drawPanel (c, 6, 50, 146, 102, gateToggle.getLevel());
    drawPanel (c, 158, 50, 92, 102, limToggle.getLevel());
    drawScope (c);

    font::drawText (c, 6, 154, "V0.1", pal::ash);
    font::drawRight (c, W - 6, 154, "BY N-F.PRO", pal::ash);
}

void CuttaEditor::paintOverlay (Canvas& c)
{
    // readout of the active control in its panel header
    if (auto* w = getActiveWidget())
    {
        const auto txt = w->getReadout().toUpperCase().toStdString();
        if (! txt.empty())
        {
            const bool left = w->bounds.getX() < 155;
            font::drawRight (c, left ? 147 : 246, 53, txt, pal::bone);
        }
    }
    else if (limToggle.isOn() && grDbShown < -0.1f)
    {
        font::drawRight (c, 246, 53, juce::String (grDbShown, 1).toStdString() + "DB", mix (pal::smoke, pal::ember, limActivity));
    }

    // dim disabled sections
    if (gateToggle.getLevel() < 0.99f)
        c.ditherRect (scopeX, scopeY, scopeW, scopeH, pal::voidC, 0.6f * (1.0f - gateToggle.getLevel()));
}

//==============================================================================
void CuttaEditor::drawPanel (Canvas& c, int x, int y, int w, int h, float lvl)
{
    c.fillRect (x + 1, y + 1, w - 2, h - 2, pal::ink);
    const RGB border = mix (pal::iron, mix (pal::iron, pal::rust, 0.8f), lvl * 0.6f);
    c.hline (x + 1, x + w - 2, y, border);
    c.hline (x + 1, x + w - 2, y + h - 1, border);
    c.vline (x, y + 1, y + h - 2, border);
    c.vline (x + w - 1, y + 1, y + h - 2, border);
    c.hline (x + 2, x + w - 3, y + 1, 0x17161d);
    for (int i = x + 2; i < x + w - 2; ++i) c.dither (i, y + 10, pal::ash, 0.5f);
    ornament (c, x + w - 6, y + h - 8, mix (pal::iron, pal::rust, lvl));
}

void CuttaEditor::drawTitle (Canvas& c)
{
    // ornament lines left & right
    for (int x = 14; x <= 58; ++x)
    {
        const float t = (float) (x - 14) / 44.0f;
        c.dither (x, 19, pal::ash, t);
        c.dither (W - 1 - x, 19, pal::ash, t);
    }
    ornament (c, 8, 17, pal::rust);
    ornament (c, W - 11, 17, pal::rust);

    const float intensity = (0.3f + 0.7f * gateOpenShown) * flicker;

    // dithered glow around the cross
    const auto& dist = crossDistance();
    for (int y = 0; y < glowH; ++y)
        for (int x = 0; x < glowW; ++x)
        {
            const float d = dist[(size_t) (y * glowW + x)];
            if (d <= 0.0f || d > (float) glowPad) continue;
            const float lv = intensity * std::pow (1.0f - d / ((float) glowPad + 0.5f), 1.6f);
            const int px_ = crossX - glowPad + x, py = titleY - glowPad + y;
            if (Canvas::bayer (px_, py, lv)) c.set (px_, py, mix (pal::rust, pal::blood, lv));
            if (d < 1.5f) c.add (px_, py, pal::blood, 0.35f * intensity);
        }

    for (int g = 0; g < 5; ++g)
    {
        const int gx = titleX + g * (glyphW * titleScale + titleGap);
        const bool cross = g == 2;
        for (int j = 0; j < glyphH; ++j)
            for (int i = 0; i < glyphW; ++i)
            {
                if (title[g][j][i] != '#') continue;
                for (int sy = 0; sy < titleScale; ++sy)
                    for (int sx = 0; sx < titleScale; ++sx)
                    {
                        const int x = gx + i * titleScale + sx, y = titleY + j * titleScale + sy;
                        RGB col;
                        if (cross)
                        {
                            col = mix (pal::rust, pal::blood, 0.35f + 0.65f * intensity);
                            if (sx == 1 && sy == 1) col = mix (col, pal::ember, intensity * 0.8f);
                        }
                        else
                        {
                            col = mix (pal::bone, pal::smoke, (float) j / (glyphH - 1) * 0.5f);
                            const float band = (float) x - sweep + (float) y * 0.6f;
                            if (band >= 0.0f && band < 3.0f) col = mix (col, 0xffffff, 0.55f);
                        }
                        c.set (x, y, col);
                    }
                // drop shadow
                if (! cross)
                    for (int s = 0; s < titleScale; ++s)
                    {
                        const int bx = gx + i * titleScale, by = titleY + j * titleScale;
                        if (j + 1 >= glyphH || title[g][j + 1][i] != '#') c.set (bx + s + 1, by + titleScale, 0x2a0609);
                        if (i + 1 >= glyphW || title[g][j][i + 1] != '#') c.set (bx + titleScale, by + s + 1, 0x2a0609);
                    }
            }
    }
}

void CuttaEditor::drawDrip (Canvas& c)
{
    const RGB drop = mix (pal::rust, pal::blood, 0.9f);
    switch (drip.state)
    {
        case Drip::forming:
            c.set (dripX, dripY0, drop);
            if (drip.t > 0.45f) c.set (dripX, dripY0 + 1, drop);
            break;
        case Drip::falling:
        {
            const int y = (int) drip.y;
            c.set (dripX, y - 2, pal::rust);
            c.set (dripX, y - 1, drop);
            c.set (dripX, y, pal::blood);
            c.set (dripX, y + 1, pal::ember);
            break;
        }
        case Drip::splash:
        {
            const float k = 1.0f - drip.t / 1.4f;
            const int half = (int) std::lround (1.0f + 3.0f * (1.0f - k * k));
            c.hline (dripX - half, dripX + half, dripFloor, mix (pal::voidC, pal::blood, k));
            break;
        }
        case Drip::idle: break;
    }
    for (auto& s : sparks)
        if (s.life > 0.0f) c.set ((int) s.x, (int) s.y, mix (pal::rust, pal::ember, s.life));
}

void CuttaEditor::drawParticles (Canvas& c)
{
    for (auto& pt : particles)
    {
        const float tw = 0.5f + 0.5f * std::sin (pt.life * 9.0f + pt.phase);
        const RGB col = pt.ember ? mix (pal::rust, pal::ember, (0.25f + 0.75f * gateShown) * tw)
                                 : mix (0x141319, pal::ash, tw);
        c.set ((int) pt.x, (int) pt.y, col);
    }
}

void CuttaEditor::drawScope (Canvas& c)
{
    const int x0 = scopeX + 1, y0 = scopeY + 1, iw = scopeW - 2, ih = scopeH - 2;
    c.rect (scopeX, scopeY, scopeW, scopeH, pal::iron);
    c.fillRect (x0, y0, iw, ih, 0x08080b);

    auto dbToY = [&] (float db)
    {
        const float n = juce::jlimit (0.0f, 1.0f, (db - scopeFloorDb) / -scopeFloorDb);
        return y0 + ih - 1 - (int) std::lround (n * (float) (ih - 1));
    };

    for (float db = -12.0f; db > scopeFloorDb; db -= 12.0f)
        for (int x = x0; x < x0 + iw; x += 2) c.set (x, dbToY (db), 0x121117);

    const int n = (int) histLevel.size();
    int prevGy = -1;
    for (int i = 0; i < n; ++i)
    {
        const int idx = (histPos + i) % n;
        const int x = x0 + i;
        const float lv = histLevel[(size_t) idx];
        const float gg = histGate[(size_t) idx];
        const bool cut = gg < 0.5f;
        const float age = (float) i / (float) n;

        if (lv > scopeFloorDb)
        {
            const int top = dbToY (lv);
            const RGB body = cut ? mix (0x1a0a0d, pal::rust, age) : mix (0x16151b, pal::ash, age);
            c.vline (x, top, y0 + ih - 1, body);
            c.set (x, top, cut ? mix (pal::rust, pal::blood, age) : mix (pal::ash, pal::smoke, age));
        }

        const int gy = dbToY (toDb (gg));
        if (prevGy >= 0 && std::abs (gy - prevGy) > 1)
            c.vline (x, std::min (gy, prevGy), std::max (gy, prevGy), mix (pal::rust, pal::blood, age));
        c.set (x, gy, mix (pal::rust, pal::blood, 0.4f + 0.6f * age));
        prevGy = gy;
    }
    // newest point
    c.fillRect (x0 + iw - 2, prevGy - 1, 2, 2, pal::ember);

    // marching threshold line
    const float thrDb = proc.apvts.getRawParameterValue (params::gateThresh)->load();
    const int ty = dbToY (thrDb);
    const int off = (int) (time * 14.0f);
    const bool thrActive = kThr.hovered || getActiveWidget() == &kThr;
    for (int x = x0; x < x0 + iw; ++x)
        if (((x + off) % 6) < 3) c.set (x, ty, thrActive ? pal::bone : mix (pal::smoke, pal::bone, 0.3f));

    // state tag
    const bool open = gateOpenShown > 0.5f;
    font::drawRight (c, x0 + iw - 2, y0 + 2, open ? "OPEN" : "SHUT", open ? mix (pal::blood, pal::ember, flicker) : pal::smoke);
}
