#include "LatusProcessor.h"
#include "LatusEditor.h"

using namespace juce;

namespace latus
{
const StringArray& modeNames() { static const StringArray a { "I", "II", "I + II", "ENSEMBLE" }; return a; }
}

AudioProcessorValueTreeState::ParameterLayout LatusProcessor::createLayout()
{
    using namespace latus;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::width, 1 }, "Width", NormalisableRange<float> (0.0f, 1.0f), 0.6f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::mode, 1 }, "Mode", modeNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::speed, 1 }, "Speed", nob::logRange (0.25f, 4.0f), 1.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " x"; })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::age, 1 }, "Age", NormalisableRange<float> (0.0f, 1.0f), 0.25f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::bassMono, 1 }, "Bass Mono", true));
    nob::LiteProcessor::addCommonParameters (l);
    return l;
}

LatusProcessor::LatusProcessor() : nob::LiteProcessor (createLayout())
{
    using namespace latus;
    rWidth = raw (ID::width); rMode = raw (ID::mode); rSpeed = raw (ID::speed); rAge = raw (ID::age); rBass = raw (ID::bassMono);
}

latus::Params LatusProcessor::currentParams() const noexcept
{
    latus::Params p;
    p.width = rWidth.f();
    p.mode = (latus::Mode) jlimit (0, 3, rMode.i());
    p.speed = rSpeed.f();
    p.age = rAge.f();
    p.bassMono = rBass.b();
    return p;
}

void LatusProcessor::prepareEffect (double sampleRate, int)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate);
}

void LatusProcessor::processEffect (float* L, float* R, int n)
{
    engine.setParams (currentParams());
    engine.process (L, R, n);
}

AudioProcessorEditor* LatusProcessor::createEditor() { return new LatusEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new LatusProcessor(); }
