#include "LatusEditor.h"

using namespace juce;
using namespace nob::ui;

//==============================================================================
void FieldScreen::update()
{
    const int wp = proc.engine.writePos.load (std::memory_order_relaxed);
    float peak = 1.0e-4f, sLR = 0.0f, sLL = 0.0f, sRR = 0.0f;
    for (int i = 0; i < latus::Engine::scopeSize; ++i)
    {
        const int k = (wp + i) % latus::Engine::scopeSize;
        l[(size_t) i] = proc.engine.scopeL[(size_t) k].load (std::memory_order_relaxed);
        r[(size_t) i] = proc.engine.scopeR[(size_t) k].load (std::memory_order_relaxed);
        peak = jmax (peak, std::abs (l[(size_t) i]), std::abs (r[(size_t) i]));
        sLR += l[(size_t) i] * r[(size_t) i]; sLL += l[(size_t) i] * l[(size_t) i]; sRR += r[(size_t) i] * r[(size_t) i];
    }
    const float target = 0.62f / peak;
    scale += (jlimit (0.5f, 60.0f, target) - scale) * (target < scale ? 0.5f : 0.05f);
    const float corr = (sLL > 1.0e-9f && sRR > 1.0e-9f) ? sLR / std::sqrt (sLL * sRR) : 1.0f;
    correlation += (corr - correlation) * 0.2f;
    repaint();
}

void FieldScreen::paintScreen (Graphics& g, Rectangle<float> area, const NobPalette& p)
{
    label = "STEREO FIELD";
    rightLabel = "MODE " + latus::modeNames()[(int) proc.currentParams().mode];
    auto plot = area.reduced (16.0f).withTrimmedTop (14.0f).withTrimmedBottom (18.0f);
    const float s = jmin (plot.getWidth(), plot.getHeight());
    plot = plot.withSizeKeepingCentre (s, s);
    const auto c = plot.getCentre();
    const float R = s * 0.5f;

    // guides: the M (vertical) and S (horizontal) axes and the L / R diagonals
    g.setColour (p.screenDim.withAlpha (0.22f));
    g.drawEllipse (plot, 1.0f);
    g.drawLine (c.x, plot.getY(), c.x, plot.getBottom(), 1.0f);
    g.drawLine (plot.getX(), c.y, plot.getRight(), c.y, 1.0f);
    g.setColour (p.screenDim.withAlpha (0.12f));
    g.drawLine (Line<float> (c.getPointOnCircumference (R, -0.785f), c.getPointOnCircumference (R, 2.356f)), 1.0f);
    g.drawLine (Line<float> (c.getPointOnCircumference (R, 0.785f), c.getPointOnCircumference (R, -2.356f)), 1.0f);
    g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
    g.setColour (p.screenDim);
    g.drawText ("L", Rectangle<float> (16, 12).withCentre (c.getPointOnCircumference (R * 1.08f, -0.785f)), Justification::centred);
    g.drawText ("R", Rectangle<float> (16, 12).withCentre (c.getPointOnCircumference (R * 1.08f, 0.785f)), Justification::centred);

    // the points: mid up, side across
    const int n = latus::Engine::scopeSize;
    for (int i = 0; i < n; ++i)
    {
        const float m = (l[(size_t) i] + r[(size_t) i]) * 0.7071f * scale;
        const float sd = (r[(size_t) i] - l[(size_t) i]) * 0.7071f * scale;
        const auto pt = Point<float> (c.x + sd * R, c.y - m * R);
        const float age = (float) i / (float) n;                    // newest are brightest
        g.setColour (p.glow (0.55f + 0.45f * age).withAlpha (0.10f + 0.6f * age));
        g.fillEllipse (Rectangle<float> (2.4f, 2.4f).withCentre (pt));
    }

    // correlation readout
    g.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), 10.0f, Font::plain)));
    g.setColour (p.screenInk);
    g.drawText ("CORR " + String (correlation >= 0 ? "+" : "") + String (correlation, 2),
                area.reduced (12.0f, 6.0f).removeFromBottom (12.0f), Justification::centredLeft);
}

//==============================================================================
LatusEditor::LatusEditor (LatusProcessor& p)
    : LiteEditor (p), proc (p),
      width (p.apvts, latus::ID::width, "WIDTH"),
      mode (p.apvts, latus::ID::mode, latus::modeNames(), 2),
      speed (p.apvts, latus::ID::speed, "SPEED", "1/4", "4x"),
      age (p.apvts, latus::ID::age, "AGE", "NEW", "OLD"),
      mix (p.apvts, nob::CommonID::mix, "MIX", "DRY", "WET"),
      output (p.apvts, nob::CommonID::output, "OUTPUT", "-24", "+12", true),
      bassMono (p.apvts, latus::ID::bassMono, "BASS MONO")
{
    width.activity = [this]
    {
        const float ph = proc.engine.uiPhase.load (std::memory_order_relaxed);
        return 0.25f * proc.currentParams().width * (0.5f + 0.5f * std::sin (ph * MathConstants<float>::twoPi));
    };
    mode.setGap (6.0f);
    for (Component* c : std::initializer_list<Component*> { &screen, &width, &mode, &speed, &age, &mix, &output, &bassMono })
        addToBody (*c);
    finishSetup();
}

void LatusEditor::refreshTexts()
{
    width.slider.setTooltip (TRANS ("How much chorus: wet level and depth together"));
    speed.slider.setTooltip (TRANS ("Scales the speed of the chosen mode"));
    age.slider.setTooltip (TRANS ("The bucket-brigade chip: darker, more saturated and a little hiss"));
    bassMono.bat.setTooltip (TRANS ("Keeps everything under 150 Hz dry and centred"));
}

void LatusEditor::layoutBody()
{
    screen.setBounds (R (40, 172, 262, 334));
    width.setBounds (R (316, 164, 268, 312));
    mode.setBounds (R (612, 190, 244, 76));
    speed.setBounds (R (618, 286, 104, 104));
    age.setBounds (R (746, 286, 104, 104));
    mix.setBounds (R (606, 414, 80, 96));
    output.setBounds (R (698, 414, 80, 96));
    bassMono.setBounds (R (786, 420, 70, 84));
}

void LatusEditor::paintBody (Graphics& g, const NobPalette& p, bool dark)
{
    drawSectionTitle (g, p, { 612, 170, 200, 14 }, "MODE");
    drawPlate (g, p, dark, { 600, 404, 264, 112 });
    g.setColour (p.inkDim.withAlpha (0.8f));
    g.setFont (NobLookAndFeel::font (7.5f, true, 0.3f));
    g.drawText ("BUCKET BRIGADE  /  STEREO", Rectangle<float> (316, 492, 268, 12), Justification::centred);
}

void LatusEditor::bodyTimer()
{
    screen.update();
}
