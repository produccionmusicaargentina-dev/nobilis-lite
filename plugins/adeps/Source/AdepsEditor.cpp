#include "AdepsEditor.h"

using namespace juce;
using namespace nob::ui;

namespace
{
    juce::String voiceName (float c)
    {
        if (c < 0.2f) return "LADDER";
        if (c < 0.4f) return "LADDER / OTA";
        if (c < 0.6f) return "OTA";
        if (c < 0.8f) return "OTA / SALLEN-KEY";
        return "SALLEN-KEY";
    }

    // input lamp: dark below -36 dB RMS, green in the sweet spot, amber, then red
    Colour lampColour (float db)
    {
        if (db > -4.0f)  return Colour (0xffff3b2f);
        if (db > -10.0f) return Colour (0xffffb020);
        return Colour (0xff3ee07a);
    }
}

//==============================================================================
void TransferScreen::update()
{
    const float in = Decibels::decibelsToGain (proc.engine.uiInDb.load()) * 1.414f;
    const float g = proc.engine.uiGr.load();
    inLevel += (jlimit (0.0f, 1.0f, in) - inLevel) * 0.35f;
    gr += (g - gr) * (g > gr ? 0.5f : 0.15f);
    const auto prm = proc.currentParams();
    const auto k = String (prm.fatness, 3) + String (prm.color, 3);
    if (k != key || std::abs (in - inLevel) > 0.002f || std::abs (g - gr) > 0.02f) { key = k; repaint(); }
}

void TransferScreen::paintScreen (Graphics& g, Rectangle<float> area, const NobPalette& p)
{
    const auto prm = proc.currentParams();
    label = "TRANSFER";
    rightLabel = voiceName (prm.color);
    auto plot = area.reduced (14.0f, 8.0f).withTrimmedTop (18.0f).withTrimmedBottom (22.0f);
    plot = plot.withSizeKeepingCentre (jmin (plot.getWidth(), plot.getHeight() * 1.5f), plot.getHeight());

    // axes and the clean diagonal
    g.setColour (p.screenDim.withAlpha (0.3f));
    g.drawHorizontalLine (roundToInt (plot.getCentreY()), plot.getX(), plot.getRight());
    g.drawVerticalLine (roundToInt (plot.getCentreX()), plot.getY(), plot.getBottom());
    g.setColour (p.screenDim.withAlpha (0.25f));
    g.drawLine (Line<float> (plot.getBottomLeft(), plot.getTopRight()), 1.0f);

    // the curve
    Path curve;
    const int N = 160;
    for (int i = 0; i <= N; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) N;
        const float y = jlimit (-1.1f, 1.1f, proc.engine.transfer (x));
        const auto pt = Point<float> (plot.getX() + (x + 1.0f) * 0.5f * plot.getWidth(), plot.getCentreY() - y * 0.5f * plot.getHeight());
        if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
    }
    g.setColour (p.glow (0.85f).withAlpha (0.35f));
    g.strokePath (curve, PathStrokeType (5.0f));
    g.setColour (p.curve);
    g.strokePath (curve, PathStrokeType (1.8f));

    // live input level: a dot riding the curve
    if (inLevel > 0.003f)
    {
        const float y = proc.engine.transfer (inLevel);
        const Point<float> dot (plot.getX() + (inLevel + 1.0f) * 0.5f * plot.getWidth(), plot.getCentreY() - y * 0.5f * plot.getHeight());
        g.setColour (p.glow (1.0f).withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (14, 14).withCentre (dot));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (6, 6).withCentre (dot));
    }

    // pressure (glue compression) bar
    auto bar = area.reduced (14.0f, 8.0f).removeFromBottom (10.0f);
    g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
    g.setColour (p.screenDim);
    g.drawText ("PRESSURE", bar.removeFromLeft (64.0f), Justification::centredLeft);
    g.setColour (p.screenDim.withAlpha (0.25f));
    g.fillRoundedRectangle (bar.withHeight (4.0f).withY (bar.getCentreY() - 2.0f), 2.0f);
    const float frac = jlimit (0.0f, 1.0f, gr / 12.0f);
    g.setColour (p.glow (0.6f + 0.4f * frac));
    g.fillRoundedRectangle (bar.withHeight (4.0f).withY (bar.getCentreY() - 2.0f).withWidth (bar.getWidth() * frac), 2.0f);
}

//==============================================================================
AdepsEditor::AdepsEditor (AdepsProcessor& p)
    : LiteEditor (p), proc (p),
      fatness (p.apvts, adeps::ID::fatness, "FATNESS"),
      color (p.apvts, adeps::ID::color, "COLOR"),
      gain (p.apvts, adeps::ID::gain, "GAIN", "-18", "+18", true),
      mix (p.apvts, nob::CommonID::mix, "MIX", "DRY", "WET"),
      output (p.apvts, nob::CommonID::output, "OUTPUT", "-24", "+12", true),
      autoGain (p.apvts, adeps::ID::autoGain, "AUTO GAIN")
{
    color.scaleNames = { "LADDER", "OTA", "SALLEN-KEY" };
    fatness.activity = [this] { return jlimit (0.0f, 1.0f, proc.engine.uiGr.load() / 10.0f); };
    for (Component* c : std::initializer_list<Component*> { &screen, &fatness, &color, &gain, &mix, &output, &autoGain, &lamp })
        addToBody (*c);
    finishSetup();
}

void AdepsEditor::refreshTexts()
{
    fatness.slider.setTooltip (TRANS ("Compression, drive and clipping, all from one knob"));
    color.slider.setTooltip (TRANS ("The character of the drive: dark ladder, creamy OTA or bright Sallen-Key"));
    gain.slider.setTooltip (TRANS ("Set it so the lamp glows green, then play with FATNESS and COLOR"));
    autoGain.bat.setTooltip (TRANS ("Keeps the output as loud as the input, so you judge the tone and not the volume"));
}

void AdepsEditor::layoutBody()
{
    screen.setBounds (R (40, 172, 262, 186));
    gain.setBounds (R (40, 390, 96, 118));
    lamp.setBounds (R (146, 422, 44, 44));
    fatness.setBounds (R (316, 164, 268, 312));
    color.setBounds (R (602, 164, 262, 236));
    mix.setBounds (R (606, 414, 80, 96));
    output.setBounds (R (698, 414, 80, 96));
    autoGain.setBounds (R (786, 420, 70, 84));
}

void AdepsEditor::paintBody (Graphics& g, const NobPalette& p, bool dark)
{
    drawSectionTitle (g, p, { 40, 372, 200, 14 }, "INPUT");
    drawPlate (g, p, dark, { 600, 404, 264, 112 });

    // hint next to the input lamp
    g.setColour (p.inkDim);
    g.setFont (NobLookAndFeel::font (8.0f, true, 0.18f));
    g.drawFittedText (TRANS ("TURN GAIN UNTIL THE LAMP IS GREEN"), Rectangle<int> (186, 428, 116, 34), Justification::centredLeft, 3);

    // what the macro does, engraved under the big knob
    g.setColour (p.inkDim.withAlpha (0.8f));
    g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
    g.drawText ("COMP  +  DRIVE  +  CLIP", Rectangle<float> (316, 492, 268, 12), Justification::centred);
}

void AdepsEditor::paintLogo (Graphics& g, Rectangle<float> area, const NobPalette& p)
{
    // The logo gets fatter with FATNESS: letters swell from the middle and the stroke thickens.
    const float fat = jlimit (0.0f, 1.0f, proc.currentParams().fatness);
    GlyphArrangement ga;
    ga.addLineOfText (NobLookAndFeel::font (40.0f, true, 0.30f), nob::product().name, 0.0f, 0.0f);
    Path letters;
    ga.createPath (letters);
    const auto lb = letters.getBounds();
    const float cxL = lb.getCentreX(), cyL = lb.getCentreY();

    Path warped;
    for (PathFlatteningIterator it (letters, {}, 0.3f); it.next();)
    {
        auto warp = [&] (float x, float y)
        {
            const float nx = (x - cxL) / (lb.getWidth() * 0.5f);              // -1..1 across the word
            const float ny = (y - cyL) / (lb.getHeight() * 0.5f);             // -1..1 top to bottom
            const float belly = 1.0f - 0.55f * ny * ny;                       // swell most in the middle rows
            const float sx = 1.0f + 0.05f * fat;
            const float sy = 1.0f + 0.22f * fat * (1.0f - 0.35f * nx * nx);
            return Point<float> (cxL + (x - cxL) * sx + nx * 3.0f * fat * belly, cyL + (y - cyL) * sy);
        };
        const auto a = warp (it.x1, it.y1), b = warp (it.x2, it.y2);
        if (it.subPathIndex == 0) warped.startNewSubPath (a);
        warped.lineTo (b);
        if (it.closesSubPath) warped.closeSubPath();
    }
    warped.applyTransform (AffineTransform::translation (area.getCentreX() - cxL + 6.0f, area.getCentreY() - cyL));

    if (fat > 0.5f)
    {
        g.setColour (p.accent.withAlpha ((fat - 0.5f) * 0.35f));
        g.strokePath (warped, PathStrokeType (2.0f + 6.0f * fat, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (p.ink);
    g.fillPath (warped);
    if (fat > 0.02f)
        g.strokePath (warped, PathStrokeType (2.4f * fat, PathStrokeType::curved, PathStrokeType::rounded));
}

void AdepsEditor::bodyTimer()
{
    screen.update();
    const float fat = proc.currentParams().fatness;
    if (std::abs (fat - shownFat) > 0.003f)
    {
        shownFat = fat;
        repaint();                                   // the logo swells
    }
    const float db = proc.engine.uiInDb.load();
    const float lvl = db < -36.0f ? 0.0f : jlimit (0.15f, 1.0f, (db + 36.0f) / 20.0f);
    lamp.set (lampColour (db), lvl);
}
