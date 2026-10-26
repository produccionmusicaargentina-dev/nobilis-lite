#pragma once

#include "LiteProcessor.h"
#include "LatusEngine.h"

// Stable parameter IDs (never rename; add new ones at the end)
namespace latus::ID
{
    inline constexpr const char* width    = "width";
    inline constexpr const char* mode     = "mode";
    inline constexpr const char* speed    = "speed";
    inline constexpr const char* age      = "age";
    inline constexpr const char* bassMono = "bass_mono";
}

namespace latus { const juce::StringArray& modeNames(); }

class LatusProcessor : public nob::LiteProcessor
{
public:
    LatusProcessor();
    juce::AudioProcessorEditor* createEditor() override;

    latus::Engine engine;
    latus::Params currentParams() const noexcept;

protected:
    void prepareEffect (double sampleRate, int maxBlock) override;
    void processEffect (float* L, float* R, int numSamples) override;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    Raw rWidth, rMode, rSpeed, rAge, rBass;
};
