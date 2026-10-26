#include "VetusEditor.h"

using namespace juce;
using namespace nob::ui;

namespace
{
    constexpr float twoPi = MathConstants<float>::twoPi;
}

//==============================================================================
void MediumScreen::update()
{
    const float r = proc.engine.uiRotation.load (std::memory_order_relaxed);
    if (std::abs (r - rotation) > 1.0e-4f) { rotation = r; repaint(); }
}

void MediumScreen::paintScreen (Graphics& g, Rectangle<float> area, const NobPalette& p)
{
    const auto prm = proc.currentParams();
    label = vetus::mediumNames()[(int) prm.medium];
    rightLabel = "AGE " + String (roundToInt (prm.age * 100.0f));
    auto art = area.reduced (14.0f).withTrimmedTop (18.0f).withTrimmedBottom (20.0f);
    switch (prm.medium)
    {
        case vetus::Medium::tape:    drawTape (g, art, p, rotation, prm.age); break;
        case vetus::Medium::vinyl:   drawVinyl (g, art, p, rotation, prm.age); break;
        case vetus::Medium::sampler: drawSampler (g, art, p, rotation, prm.age); break;
        case vetus::Medium::radio:   drawRadio (g, art, p, rotation, prm.age); break;
    }

    // footer readout
    g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 10.0f, Font::plain)));
    g.setColour (p.screenInk);
    String info;
    switch (prm.medium)
    {
        case vetus::Medium::tape:    info = "7.5 IPS   WOW " + String (proc.engine.uiWow.load(), 2) + " ms"; break;
        case vetus::Medium::vinyl:   info = "33 1/3 RPM   WOW " + String (proc.engine.uiWow.load(), 2) + " ms"; break;
        case vetus::Medium::sampler: info = String (proc.engine.uiRate.load() / 1000.0f, 1) + " kHz   " + String (roundToInt (proc.engine.uiBits.load())) + " BIT"; break;
        case vetus::Medium::radio:   info = "AM   MONO   " + String (roundToInt (4200.0f - 1700.0f * prm.age)) + " Hz"; break;
    }
    g.drawText (info, area.reduced (12.0f, 6.0f).removeFromBottom (12.0f), Justification::centredLeft);
}

void MediumScreen::drawTape (Graphics& g, Rectangle<float> a, const NobPalette& p, float rot, float age)
{
    const float r = jmin (a.getWidth() * 0.19f, a.getHeight() * 0.20f);
    const Point<float> c1 (a.getX() + a.getWidth() * 0.28f, a.getY() + a.getHeight() * 0.30f);
    const Point<float> c2 (a.getX() + a.getWidth() * 0.72f, c1.y);
    const auto line = p.glow (0.8f);

    // tape path: off the outside of each pack, round two guides, across the head
    const Point<float> head (a.getCentreX(), a.getY() + a.getHeight() * 0.78f);
    const float packL = r * 0.92f, packR = r * 0.55f;            // more tape on the supply reel
    const Point<float> g1 (a.getX() + a.getWidth() * 0.16f, head.y), g2 (a.getRight() - a.getWidth() * 0.16f, head.y);
    Path tape;
    tape.startNewSubPath (c1.translated (-packL, 0));
    tape.lineTo (g1.translated (-5, 0));
    tape.lineTo (g2.translated (5, 0));
    tape.lineTo (c2.translated (packR, 0));
    g.setColour (line.withAlpha (0.25f));
    g.strokePath (tape, PathStrokeType (4.0f));
    g.setColour (line.withAlpha (0.8f));
    g.strokePath (tape, PathStrokeType (1.2f));
    for (auto gp : { g1, g2 })
    {
        g.setColour (p.screen);
        g.fillEllipse (Rectangle<float> (10, 10).withCentre (gp));
        g.setColour (line);
        g.drawEllipse (Rectangle<float> (10, 10).withCentre (gp), 1.2f);
    }
    // head
    g.setColour (p.screenDim);
    g.fillRoundedRectangle (Rectangle<float> (26, 12).withCentre (head.translated (0, 7)), 2.0f);

    auto reel = [&] (Point<float> c, float pack, float speed)
    {
        g.setColour (line.withAlpha (0.10f));
        g.fillEllipse (Rectangle<float> (pack * 2, pack * 2).withCentre (c));
        g.setColour (line.withAlpha (0.35f));
        g.drawEllipse (Rectangle<float> (pack * 2, pack * 2).withCentre (c), 1.0f);
        g.setColour (line.withAlpha (0.9f));
        g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), 1.6f);
        // three windows in the flange, turning
        for (int k = 0; k < 3; ++k)
        {
            const float ang = rot * twoPi * speed + (float) k * twoPi / 3.0f;
            Path win;
            win.addCentredArc (c.x, c.y, r * 0.62f, r * 0.62f, 0.0f, ang - 0.55f, ang + 0.55f, true);
            g.setColour (line.withAlpha (0.65f));
            g.strokePath (win, PathStrokeType (r * 0.22f, PathStrokeType::curved, PathStrokeType::butt));
        }
        g.setColour (p.screen);
        g.fillEllipse (Rectangle<float> (r * 0.36f, r * 0.36f).withCentre (c));
        g.setColour (line);
        g.drawEllipse (Rectangle<float> (r * 0.36f, r * 0.36f).withCentre (c), 1.2f);
    };
    reel (c1, packL, 0.55f);
    reel (c2, packR, 0.85f);

    // dust: more with age
    Random rnd (7);
    g.setColour (p.screenDim.withAlpha (0.35f));
    for (int i = 0; i < (int) (age * 40.0f); ++i)
        g.fillEllipse (a.getX() + rnd.nextFloat() * a.getWidth(), a.getY() + rnd.nextFloat() * a.getHeight(), 1.4f, 1.4f);
}

void MediumScreen::drawVinyl (Graphics& g, Rectangle<float> a, const NobPalette& p, float rot, float age)
{
    const float R = jmin (a.getWidth(), a.getHeight()) * 0.46f;
    const auto c = Point<float> (a.getCentreX() - a.getWidth() * 0.04f, a.getCentreY());
    const auto line = p.glow (0.8f);
    const float ang = rot * twoPi * 0.555f;

    g.setColour (Colour (0xff050505));
    g.fillEllipse (Rectangle<float> (R * 2, R * 2).withCentre (c));
    // grooves
    for (float rr = R * 0.38f; rr < R * 0.97f; rr += 3.2f)
    {
        g.setColour (line.withAlpha (0.06f + 0.05f * std::sin (rr * 0.7f)));
        g.drawEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre (c), 0.8f);
    }
    // the sheen: light catching the grooves, turning with the record
    for (int k = 0; k < 2; ++k)
    {
        Path s;
        const float base = ang + (float) k * MathConstants<float>::pi;
        s.addPieSegment (Rectangle<float> (R * 2, R * 2).withCentre (c), base - 0.22f, base + 0.22f, 0.38f);
        g.setColour (line.withAlpha (0.16f));
        g.fillPath (s);
    }
    g.setColour (line.withAlpha (0.9f));
    g.drawEllipse (Rectangle<float> (R * 2, R * 2).withCentre (c), 1.4f);
    // label (with a mark, so the turning shows)
    const float lr = R * 0.34f;
    g.setColour (p.accent.withAlpha (0.85f));
    g.fillEllipse (Rectangle<float> (lr * 2, lr * 2).withCentre (c));
    g.setColour (p.screen);
    g.fillEllipse (Rectangle<float> (5, 5).withCentre (c));
    g.setColour (Colours::white.withAlpha (0.8f));
    g.drawLine (Line<float> (c.getPointOnCircumference (lr * 0.45f, ang), c.getPointOnCircumference (lr * 0.85f, ang)), 2.0f);
    // scratches with age
    Random rnd (11);
    g.setColour (Colours::white.withAlpha (0.18f));
    for (int i = 0; i < (int) (age * 9.0f); ++i)
    {
        const float a0 = rnd.nextFloat() * twoPi + ang, rr = R * (0.45f + 0.5f * rnd.nextFloat());
        Path sc;
        sc.addCentredArc (c.x, c.y, rr, rr, 0.0f, a0, a0 + 0.25f + rnd.nextFloat() * 0.4f, true);
        g.strokePath (sc, PathStrokeType (0.8f));
    }
    // tonearm
    const Point<float> pivot (a.getRight() - 10.0f, a.getY() + 10.0f);
    const auto tip = c.getPointOnCircumference (R * 0.78f, 0.95f);
    g.setColour (p.screenInk.withAlpha (0.85f));
    g.drawLine (Line<float> (pivot, tip), 2.2f);
    g.fillEllipse (Rectangle<float> (12, 12).withCentre (pivot));
    g.fillRoundedRectangle (Rectangle<float> (10, 6).withCentre (tip), 1.5f);
}

void MediumScreen::drawSampler (Graphics& g, Rectangle<float> a, const NobPalette& p, float rot, float age)
{
    const auto line = p.glow (0.85f);
    auto wave = a.withTrimmedTop (a.getHeight() * 0.10f).withTrimmedBottom (a.getHeight() * 0.18f);
    // a phrase of a waveform, sampled and held: fewer, chunkier steps as AGE rises
    const int steps = jmax (6, roundToInt (64.0f * std::pow (0.2f, age)));
    const int levels = jmax (3, roundToInt (std::pow (2.0f, 6.0f - 3.5f * age)));
    Path stepped, smooth;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = (float) i / 200.0f;
        const float v = 0.55f * std::sin (t * twoPi * 2.0f + rot * 2.0f) + 0.3f * std::sin (t * twoPi * 5.3f - rot * 3.0f);
        const auto pt = Point<float> (wave.getX() + t * wave.getWidth(), wave.getCentreY() - v * wave.getHeight() * 0.45f);
        if (i == 0) smooth.startNewSubPath (pt); else smooth.lineTo (pt);
    }
    for (int s = 0; s < steps; ++s)
    {
        const float t = (float) s / (float) steps;
        float v = 0.55f * std::sin (t * twoPi * 2.0f + rot * 2.0f) + 0.3f * std::sin (t * twoPi * 5.3f - rot * 3.0f);
        v = std::round (v * (float) levels) / (float) levels;
        const float x0 = wave.getX() + t * wave.getWidth(), x1 = wave.getX() + (float) (s + 1) / (float) steps * wave.getWidth();
        const float y = wave.getCentreY() - v * wave.getHeight() * 0.45f;
        if (s == 0) stepped.startNewSubPath (x0, y); else stepped.lineTo (x0, y);
        stepped.lineTo (x1, y);
    }
    g.setColour (p.screenDim.withAlpha (0.35f));
    g.strokePath (smooth, PathStrokeType (1.0f));
    g.setColour (line.withAlpha (0.3f));
    g.strokePath (stepped, PathStrokeType (4.0f));
    g.setColour (p.curve);
    g.strokePath (stepped, PathStrokeType (1.6f));

    // pads
    auto pads = a.removeFromBottom (a.getHeight() * 0.13f);
    const float pw = pads.getWidth() / 8.0f;
    const int lit = (int) (rot * 4.0f) % 8;
    for (int i = 0; i < 8; ++i)
    {
        auto pad = Rectangle<float> (pads.getX() + (float) i * pw, pads.getY(), pw, pads.getHeight()).reduced (3.0f, 2.0f);
        g.setColour (i == lit ? line.withAlpha (0.8f) : p.screenDim.withAlpha (0.25f));
        g.fillRoundedRectangle (pad, 2.0f);
    }
}

void MediumScreen::drawRadio (Graphics& g, Rectangle<float> a, const NobPalette& p, float rot, float age)
{
    const auto line = p.glow (0.85f);
    auto dial = a.withSizeKeepingCentre (a.getWidth(), a.getHeight() * 0.42f);
    g.setColour (line.withAlpha (0.07f));
    g.fillRoundedRectangle (dial, 6.0f);
    g.setColour (line.withAlpha (0.4f));
    g.drawRoundedRectangle (dial, 6.0f, 1.0f);
    const int marks[] = { 54, 60, 70, 80, 100, 120, 140, 160 };
    g.setFont (NobLookAndFeel::font (8.0f, true, 0.05f));
    for (int i = 0; i < 8; ++i)
    {
        const float x = dial.getX() + 14.0f + (dial.getWidth() - 28.0f) * (float) i / 7.0f;
        g.setColour (line.withAlpha (0.8f));
        g.drawLine (x, dial.getY() + 6.0f, x, dial.getY() + 16.0f, 1.0f);
        g.drawText (String (marks[i]), Rectangle<float> (30, 12).withCentre ({ x, dial.getY() + 26.0f }), Justification::centred);
        for (int k = 1; k < 4 && i < 7; ++k)
        {
            const float xm = x + (dial.getWidth() - 28.0f) / 7.0f * (float) k / 4.0f;
            g.setColour (line.withAlpha (0.35f));
            g.drawLine (xm, dial.getY() + 6.0f, xm, dial.getY() + 11.0f, 1.0f);
        }
    }
    g.setColour (p.screenDim);
    g.drawText ("AM  kHz x10", dial.removeFromBottom (16.0f), Justification::centred);
    // the needle drifts a little (tuning)
    const float pos = 0.46f + 0.012f * std::sin (rot * 0.7f) * (0.3f + age);
    const float nx = a.getX() + 14.0f + (a.getWidth() - 28.0f) * pos;
    g.setColour (p.accent.withAlpha (0.35f));
    g.drawLine (nx, a.getCentreY() - a.getHeight() * 0.26f, nx, a.getCentreY() + a.getHeight() * 0.26f, 5.0f);
    g.setColour (p.accent.brighter (0.4f));
    g.drawLine (nx, a.getCentreY() - a.getHeight() * 0.26f, nx, a.getCentreY() + a.getHeight() * 0.26f, 1.6f);
    // speaker grille
    for (int i = 0; i < 9; ++i)
    {
        const float y = a.getBottom() - 46.0f + (float) i * 4.0f;
        g.setColour (p.screenDim.withAlpha (0.25f));
        g.drawHorizontalLine ((int) y, a.getX() + 20.0f, a.getRight() - 20.0f);
    }
    // signal lamp
    drawLamp (g, { a.getRight() - 14.0f, a.getBottom() - 10.0f }, 3.5f, line, 0.6f + 0.4f * std::abs (std::sin (rot * 3.0f)));
}

//==============================================================================
VetusEditor::VetusEditor (VetusProcessor& p)
    : LiteEditor (p), proc (p),
      age (p.apvts, vetus::ID::age, "AGE"),
      medium (p.apvts, vetus::ID::medium, vetus::mediumNames(), 2),
      noise (p.apvts, vetus::ID::noise, "NOISE", "0", "MAX"),
      wobble (p.apvts, vetus::ID::wobble, "WOBBLE", "0", "MAX"),
      mix (p.apvts, nob::CommonID::mix, "MIX", "DRY", "WET"),
      output (p.apvts, nob::CommonID::output, "OUTPUT", "-24", "+12", true),
      gate (p.apvts, vetus::ID::gate, "GATE NOISE")
{
    age.scaleNames = {};
    medium.setGap (6.0f);
    for (Component* c : std::initializer_list<Component*> { &screen, &age, &medium, &noise, &wobble, &mix, &output, &gate })
        addToBody (*c);
    finishSetup();
}

void VetusEditor::refreshTexts()
{
    age.slider.setTooltip (TRANS ("From almost new to found in a basement"));
    noise.slider.setTooltip (TRANS ("Hiss, crackle and static of the medium (middle = as it comes)"));
    wobble.slider.setTooltip (TRANS ("Wow and flutter: the pitch movement of the medium (middle = as it comes)"));
    gate.bat.setTooltip (TRANS ("The noise only plays while there is audio, so the end of the track is true silence"));
}

void VetusEditor::layoutBody()
{
    screen.setBounds (R (40, 172, 262, 334));
    age.setBounds (R (316, 164, 268, 312));
    medium.setBounds (R (612, 190, 244, 76));
    noise.setBounds (R (618, 286, 104, 104));
    wobble.setBounds (R (746, 286, 104, 104));
    mix.setBounds (R (606, 414, 80, 96));
    output.setBounds (R (698, 414, 80, 96));
    gate.setBounds (R (786, 420, 70, 84));
}

void VetusEditor::paintBody (Graphics& g, const NobPalette& p, bool dark)
{
    drawSectionTitle (g, p, { 612, 170, 200, 14 }, "MEDIUM");
    drawPlate (g, p, dark, { 600, 404, 264, 112 });
    g.setColour (p.inkDim.withAlpha (0.8f));
    g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
    g.drawText ("WOW  +  NOISE  +  BANDWIDTH  +  GRIT", Rectangle<float> (316, 492, 268, 12), Justification::centred);
}

void VetusEditor::bodyTimer()
{
    screen.update();
}
