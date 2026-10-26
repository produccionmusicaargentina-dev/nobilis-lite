#include "AdepsProcessor.h"
#include "AdepsEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout AdepsProcessor::createLayout()
{
    using namespace adeps;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::fatness, 1 }, "Fatness", NormalisableRange<float> (0.0f, 1.0f), 0.35f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::pctText)));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::color, 1 }, "Color", NormalisableRange<float> (0.0f, 1.0f), 0.35f,
                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                {
                    const char* names[] = { "Ladder", "Ladder / OTA", "OTA", "OTA / Sallen-Key", "Sallen-Key" };
                    return String (names[jlimit (0, 4, roundToInt (v * 4.0f))]) + "  " + String (roundToInt (v * 100.0f));
                })));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ID::gain, 1 }, "Gain", NormalisableRange<float> (-18.0f, 18.0f, 0.01f), 0.0f,
                AudioParameterFloatAttributes().withStringFromValueFunction (nob::dbText).withLabel ("dB")));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::autoGain, 1 }, "Auto Gain", false));
    nob::LiteProcessor::addCommonParameters (l);
    return l;
}

AdepsProcessor::AdepsProcessor() : nob::LiteProcessor (createLayout())
{
    rFat = raw (adeps::ID::fatness); rColor = raw (adeps::ID::color); rGain = raw (adeps::ID::gain); rAuto = raw (adeps::ID::autoGain);
}

adeps::Params AdepsProcessor::currentParams() const noexcept
{
    adeps::Params p;
    p.fatness = rFat.f(); p.color = rColor.f(); p.gainDb = rGain.f(); p.autoGain = rAuto.b();
    return p;
}

void AdepsProcessor::prepareEffect (double sampleRate, int maxBlockSize)
{
    engine.setParams (currentParams());
    engine.prepare (sampleRate, maxBlockSize);
}

void AdepsProcessor::processEffect (float* L, float* R, int n)
{
    engine.setParams (currentParams());
    engine.process (L, R, n);
}

AudioProcessorEditor* AdepsProcessor::createEditor() { return new AdepsEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AdepsProcessor(); }
