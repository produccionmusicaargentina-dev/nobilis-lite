#pragma once

#include "LiteProcessor.h"
#include "VetusEngine.h"

// Stable parameter IDs (never rename; add new ones at the end)
namespace vetus::ID
{
    inline constexpr const char* age    = "age";
    inline constexpr const char* medium = "medium";
    inline constexpr const char* noise  = "noise";
    inline constexpr const char* wobble = "wobble";
    inline constexpr const char* gate   = "gate_noise";
}

namespace vetus { const juce::StringArray& mediumNames(); }

class VetusProcessor : public nob::LiteProcessor
{
public:
    VetusProcessor();
    juce::AudioProcessorEditor* createEditor() override;

    vetus::Engine engine;
    vetus::Params currentParams() const noexcept;

protected:
    void prepareEffect (double sampleRate, int maxBlock) override;
    void processEffect (float* L, float* R, int numSamples) override;
    int getEffectLatency() const override { return engine.getLatency(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Raw rAge, rMedium, rNoise, rWobble, rGate;
};
