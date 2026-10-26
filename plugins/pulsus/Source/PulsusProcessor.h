#pragma once

#include "LiteProcessor.h"
#include "PulsusEngine.h"

//==============================================================================
// Parameter IDs are stable: presets and DAW sessions store them. Never rename
// one; add new ones at the end with a sensible default. Choice lists may only grow.
namespace pulsus::ID
{
    inline constexpr const char* depth   = "depth";
    inline constexpr const char* rate    = "rate";
    inline constexpr const char* shape   = "shape";
    inline constexpr const char* shift   = "shift";
    inline constexpr const char* breathe = "breathe";
    inline constexpr const char* band    = "band";
    inline constexpr const char* xover   = "xover";
}

namespace pulsus
{
const juce::StringArray& rateNames();
const juce::StringArray& shapeNames();
const juce::StringArray& bandNames();
}

class PulsusProcessor : public nob::LiteProcessor
{
public:
    PulsusProcessor();
    juce::AudioProcessorEditor* createEditor() override;

    pulsus::Engine engine;
    pulsus::Params currentParams() const noexcept;

protected:
    void prepareEffect (double sampleRate, int maxBlock) override;
    void processEffect (float* L, float* R, int numSamples) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Raw rDepth, rRate, rShape, rShift, rBreathe, rBand, rXover;
};
