#include "PulsusProcessor.h"
#include "PulsusEditor.h"

using namespace juce;

namespace pulsus
{
const StringArray& rateNames()  { static const StringArray a { "1/32", "1/16", "1/8T", "1/8", "1/4T", "1/4", "1/2", "1 BAR", "2 BARS", "4 BARS" }; return a; }
const StringArray& shapeNames() { static const StringArray a { "PUMP", "SOFT", "RAMP", "CHOP" }; return a; }
const StringArray& bandNames()  { static const StringArray a { "FULL", "LOWS", "HIGHS" }; return a; }
}

AudioProcessorValueTreeState::ParameterLayout PulsusProcessor::createLayout()
{
    using namespace pulsus;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::depth, 1 }, "Depth", NormalisableRange<float> (0.0f, 1.0f), 0.7f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::rate, 1 }, "Rate", rateNames(), 5));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::shape, 1 }, "Shape", shapeNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::shift, 1 }, "Shift", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::breathe, 1 }, "Breathe", NormalisableRange<float> (0.0f, 1.0f), 0.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::band, 1 }, "Band", bandNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::xover, 1 }, "Crossover", nob::logRange (40.0f, 1000.0f), 150.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " Hz"; }).withLabel ("Hz")));
    nob::LiteProcessor::addCommonParameters (l);
    return l;
}

PulsusProcessor::PulsusProcessor() : nob::LiteProcessor (createLayout())
{
    using namespace pulsus;
    rDepth = raw (ID::depth); rRate = raw (ID::rate); rShape = raw (ID::shape); rShift = raw (ID::shift);
    rBreathe = raw (ID::breathe); rBand = raw (ID::band); rXover = raw (ID::xover);
}

pulsus::Params PulsusProcessor::currentParams() const noexcept
{
    pulsus::Params p;
    p.depth = rDepth.f();
    p.rate = jlimit (0, 9, rRate.i());
    p.shape = (pulsus::Shape) jlimit (0, 3, rShape.i());
    p.shift = rShift.f();
    p.breathe = rBreathe.f();
    p.band = (pulsus::Band) jlimit (0, 2, rBand.i());
    p.xover = rXover.f();
    return p;
}

void PulsusProcessor::prepareEffect (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

void PulsusProcessor::processEffect (float* L, float* R, int n)
{
    double hostBeat = -1.0, bpm = 120.0, beatsPerBar = 4.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto ts = pos->getTimeSignature()) beatsPerBar = ts->numerator * 4.0 / jmax (1, ts->denominator);
            playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) hostBeat = *q;
        }
    engine.setParams (currentParams());
    engine.setTransport (hostBeat, bpm, beatsPerBar, playing);
    engine.process (L, R, n);
}

AudioProcessorEditor* PulsusProcessor::createEditor() { return new PulsusEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PulsusProcessor(); }
