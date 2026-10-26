#include "PulsusEditor.h"

using namespace juce;
using namespace nob::ui;

//==============================================================================
void GrooveScreen::update()
{
    const float ph = proc.engine.uiWindowPos.load (std::memory_order_relaxed);
    if (std::abs (ph - playhead) > 0.0005f) { playhead = ph; repaint(); }
}

void GrooveScreen::paintScreen (Graphics& g, Rectangle<float> area, const NobPalette& p)
{
    const auto prm = proc.currentParams();
    const double bpb = proc.engine.uiBeatsPerBar.load();
    const double bpm = proc.engine.uiBpm.load();
    const double stepBeats = pulsus::rateBeats (prm.rate, bpb);
    const double window = jmax (stepBeats, bpb);
    const float stepSec = (float) (stepBeats * 60.0 / jmax (1.0, bpm));

    label = "GROOVE";
    rightLabel = pulsus::rateNames()[prm.rate] + "  /  " + pulsus::shapeNames()[(int) prm.shape];

    auto plot = area.reduced (12.0f, 10.0f).withTrimmedTop (16.0f).withTrimmedBottom (12.0f);

    // beat grid
    const int beats = (int) std::round (window);
    for (int b = 0; b <= beats; ++b)
    {
        const float x = plot.getX() + plot.getWidth() * (float) b / (float) jmax (1, beats);
        const bool barLine = std::fmod ((double) b, bpb) < 1.0e-6;
        g.setColour (p.screenDim.withAlpha (barLine ? 0.45f : 0.18f));
        g.drawVerticalLine (roundToInt (x), plot.getY(), plot.getBottom());
    }
    g.setColour (p.screenDim.withAlpha (0.25f));
    g.drawHorizontalLine (roundToInt (plot.getY()), plot.getX(), plot.getRight());

    // gain curve
    Path curve, fill;
    const int N = jmax (64, (int) plot.getWidth() * 2);
    for (int i = 0; i <= N; ++i)
    {
        const double beat = window * (double) i / (double) N;
        const double pos = beat / stepBeats - (double) prm.shift;
        const float u = (float) (pos - std::floor (pos));
        const float gain = 1.0f - prm.depth * pulsus::duckCurve (prm.shape, u, stepSec);
        const float x = plot.getX() + plot.getWidth() * (float) i / (float) N;
        const float y = plot.getBottom() - plot.getHeight() * gain;
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); }
        else curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (p.glow (0.7f).withAlpha (0.35f), 0, plot.getY(), p.glow (0.3f).withAlpha (0.02f), 0, plot.getBottom(), false));
    g.fillPath (fill);
    g.setColour (p.glow (0.85f).withAlpha (0.35f));
    g.strokePath (curve, PathStrokeType (4.0f));
    g.setColour (p.curve);
    g.strokePath (curve, PathStrokeType (1.6f));

    // playhead + dot on the curve
    {
        const float x = plot.getX() + plot.getWidth() * playhead;
        g.setColour (p.glow (1.0f).withAlpha (0.55f));
        g.drawVerticalLine (roundToInt (x), plot.getY(), plot.getBottom());
        const double beat = window * playhead;
        const double pos = beat / stepBeats - (double) prm.shift;
        const float gain = 1.0f - prm.depth * pulsus::duckCurve (prm.shape, (float) (pos - std::floor (pos)), stepSec);
        const Point<float> dot (x, plot.getBottom() - plot.getHeight() * gain);
        g.setColour (p.glow (1.0f).withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (14, 14).withCentre (dot));
        g.setColour (Colours::white);
        g.fillEllipse (Rectangle<float> (6, 6).withCentre (dot));
    }

    // gain-reduction readout
    g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 10.0f, Font::plain)));
    g.setColour (p.screenInk);
    const float duck = proc.engine.uiDuck.load();
    g.drawText ("GR " + String (nob::dsp::gainToDb (1.0f - jmin (0.999f, duck)), 1) + " dB",
                area.reduced (12.0f, 6.0f).removeFromBottom (12.0f), Justification::centredLeft);
}

//==============================================================================
PulsusEditor::PulsusEditor (PulsusProcessor& p)
    : LiteEditor (p), proc (p),
      depth (p.apvts, pulsus::ID::depth, "DEPTH"),
      rate (p.apvts, pulsus::ID::rate, pulsus::rateNames(), 5),
      shape (p.apvts, pulsus::ID::shape, pulsus::shapeNames()),
      band (p.apvts, pulsus::ID::band, pulsus::bandNames()),
      shift (p.apvts, pulsus::ID::shift, "SHIFT", "0", "1 STEP"),
      breathe (p.apvts, pulsus::ID::breathe, "BREATHE", "0", "MAX"),
      xover (p.apvts, pulsus::ID::xover, "XOVER", "40", "1k"),
      mix (p.apvts, nob::CommonID::mix, "MIX", "DRY", "WET"),
      output (p.apvts, nob::CommonID::output, "OUTPUT", "-24", "+12", true)
{
    depth.activity = [this] { return proc.engine.uiDuck.load (std::memory_order_relaxed); };
    for (Component* c : std::initializer_list<Component*> { &screen, &depth, &rate, &shape, &band, &shift, &breathe, &xover, &mix, &output })
        addToBody (*c);
    rate.setGap (5.0f);
    shape.setGap (5.0f);
    band.setGap (5.0f);
    finishSetup();
}

void PulsusEditor::refreshTexts()
{
    depth.slider.setTooltip (TRANS ("How deep the volume dips on every step"));
    shift.slider.setTooltip (TRANS ("Moves the dip off the beat, for off-beat and counter-rhythm pumps"));
    breathe.slider.setTooltip (TRANS ("A low-pass that closes with the dip and opens as the sound comes back"));
    xover.slider.setTooltip (TRANS ("Split frequency for the LOWS / HIGHS bands"));
}

void PulsusEditor::layoutBody()
{
    // left: display + rate + shape
    screen.setBounds (R (40, 170, 360, 180));
    rate.setBounds (R (40, 380, 360, 62));
    shape.setBounds (R (40, 474, 360, 32));
    // centre: the big knob + band
    depth.setBounds (R (414, 166, 230, 268));
    band.setBounds (R (430, 474, 198, 32));
    // right: 2 x 3 knobs (tempo readout in the middle row)
    const float x0 = 660, cw = 102;
    shift.setBounds   (R (x0,      170, cw, 96));
    breathe.setBounds (R (x0 + cw, 170, cw, 96));
    xover.setBounds   (R (x0,      286, cw, 96));
    readout = { x0 + cw + 8, 312, cw - 16, 44 };
    mix.setBounds     (R (x0,      410, cw, 96));
    output.setBounds  (R (x0 + cw, 410, cw, 96));
}

void PulsusEditor::paintBody (Graphics& g, const NobPalette& p, bool dark)
{
    drawSectionTitle (g, p, { 40, 360, 200, 14 }, "RATE");
    drawSectionTitle (g, p, { 40, 454, 200, 14 }, "SHAPE");
    drawSectionTitle (g, p, { 430, 454, 198, 14 }, "BAND", Justification::centred);
    drawPlate (g, p, dark, { 652, 164, 212, 352 });

    // tempo readout: a little LCD
    g.setColour (p.screen);
    g.fillRoundedRectangle (readout, 5.0f);
    g.setColour (p.screenEdge);
    g.drawRoundedRectangle (readout, 5.0f, 1.0f);
    const bool synced = proc.engine.synced.load();
    g.setColour (p.screenInk);
    g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 13.0f, Font::bold)));
    g.drawText (String (proc.engine.uiBpm.load(), 1), readout.withTrimmedBottom (18).translated (0, 4), Justification::centred);
    g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
    g.setColour (synced ? p.glow (0.85f) : p.screenDim);
    g.drawText (synced ? "BPM  /  SYNC" : "BPM  /  FREE", readout.withTrimmedTop (24), Justification::centred);
    drawLamp (g, { readout.getRight() - 6, readout.getY() + 6 }, 2.2f, p.glow (0.9f), synced ? 1.0f : 0.0f);
}

void PulsusEditor::bodyTimer()
{
    screen.update();
    const auto t = String (proc.engine.uiBpm.load(), 1) + (proc.engine.synced.load() ? "s" : "f");
    if (t != shownTempo) { shownTempo = t; repaint(); }
}
