#include "VetusProcessor.h"
#include "VetusEditor.h"

using namespace juce;

namespace vetus
{
const StringArray& mediumNames() { static const StringArray a { "TAPE", "VINYL", "SAMPLER", "RADIO" }; return a; }
}

AudioProcessorValueTreeState::ParameterLayout VetusProcessor::createLayout()
{
    using namespace vetus;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::age, 1 }, "Age", NormalisableRange<float> (0.0f, 1.0f), 0.45f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::medium, 1 }, "Medium", mediumNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::noise, 1 }, "Noise", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::wobble, 1 }, "Wobble", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::gate, 1 }, "Gate Noise", true));
    nob::LiteProcessor::addCommonParameters (l);
    return l;
}

VetusProcessor::VetusProcessor() : nob::LiteProcessor (createLayout())
{
    using namespace vetus;
    rAge = raw (ID::age); rMedium = raw (ID::medium); rNoise = raw (ID::noise); rWobble = raw (ID::wobble); rGate = raw (ID::gate);
}

vetus::Params VetusProcessor::currentParams() const noexcept
{
    vetus::Params p;
    p.age = rAge.f();
    p.medium = (vetus::Medium) jlimit (0, 3, rMedium.i());
    p.noise = rNoise.f();
    p.wobble = rWobble.f();
    p.gateNoise = rGate.b();
    return p;
}

void VetusProcessor::prepareEffect (double sampleRate, int)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate);
}

void VetusProcessor::processEffect (float* L, float* R, int n)
{
    engine.setParams (currentParams());
    engine.process (L, R, n);
}

AudioProcessorEditor* VetusProcessor::createEditor() { return new VetusEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VetusProcessor(); }
