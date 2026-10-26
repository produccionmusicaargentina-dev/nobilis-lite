#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PresetManager.h"
#include "Product.h"
#include "license/LicenseManager.h"

//==============================================================================
// The part every Lite plugin shares, around its own effect:
//
//   IN ──┬─> [plugin wet path (may add latency)] ─> OUTPUT ─┐
//        │                                                  ├─> MIX ─> BYPASS / LICENCE ─> OUT
//        └──────────── dry, delayed by the same latency ────┘
//
// MIX, OUTPUT and BYPASS are parameters of every plugin ("mix", "output",
// "bypass"). Without a signed-in Nobilis account the plugin passes the audio
// through untouched (same as IGNIS / ACIES) and shows the sign-in card.
//
// Layouts: mono > mono, stereo > stereo and mono > stereo (the chorus widens a
// mono source). With a mono input on a stereo output, L is copied to R first.
//==============================================================================
namespace nob
{

namespace CommonID
{
    inline constexpr const char* mix    = "mix";
    inline constexpr const char* output = "output";
    inline constexpr const char* bypass = "bypass";
}

class LiteProcessor : public juce::AudioProcessor,
                      private juce::AsyncUpdater
{
public:
    explicit LiteProcessor (juce::AudioProcessorValueTreeState::ParameterLayout layout);
    ~LiteProcessor() override;

    // Adds MIX / OUTPUT / BYPASS. Call it at the end of each plugin's layout.
    static void addCommonParameters (juce::AudioProcessorValueTreeState::ParameterLayout&, float outputMaxDb = 12.0f);

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    std::unique_ptr<PresetManager> presets;
    std::unique_ptr<ParamHistory> history;
    juce::SharedResourcePointer<nobilis::LicenseManager> licence;

    // meters for the UI (peak, linear; the editor decays them)
    std::atomic<float> inPeak { 0.0f }, outPeak { 0.0f };
    double getSampleRateSafe() const noexcept { return baseRate; }
    bool isLicensed() const noexcept { return licence->isUnlocked(); }

    // UI state stored with the session (not automatable)
    static constexpr const char* uiThemeKey = "uiDarkTheme";
    static constexpr const char* uiWidthKey = "uiWidth";

    // Raw parameter pointer, resolved once (no lookups on the audio thread)
    struct Raw
    {
        std::atomic<float>* p = nullptr;
        float f() const noexcept { return p->load (std::memory_order_relaxed); }
        int   i() const noexcept { return juce::roundToInt (p->load (std::memory_order_relaxed)); }
        bool  b() const noexcept { return p->load (std::memory_order_relaxed) > 0.5f; }
    };
    Raw raw (const juce::String& id) const;

protected:
    //==========================================================================
    // What each plugin implements
    virtual void prepareEffect (double sampleRate, int maxBlock) = 0;
    // In place, at most maxBlock samples. R == nullptr for a mono bus.
    virtual void processEffect (float* L, float* R, int numSamples) = 0;
    // Latency of the wet path in samples (must be fixed after prepareEffect)
    virtual int getEffectLatency() const { return 0; }
    // Extra output gain decided by the effect (e.g. AUTO GAIN), in dB, read once per block
    virtual float getEffectMakeupDb() const { return 0.0f; }
    // Called on the message thread after a preset or session load (to reset visual state etc.)
    virtual void stateRestored() {}

    double baseRate = 48000.0;

private:
    void handleAsyncUpdate() override { setLatencySamples (latency); }
    void processChunk (juce::AudioBuffer<float>& io, int n);

    juce::AudioBuffer<float> dryLine;
    int dryWrite = 0, latency = 0, maxBlock = 512;
    juce::SmoothedValue<float> outGain, mixAmount, passAmount;
    Raw rMix, rOutput, rBypass;
    juce::AudioParameterBool* bypassParam = nullptr;
    std::atomic<bool> prepared { false };
    bool needsMonoToStereo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LiteProcessor)
};

// Helpers for parameter layouts
juce::NormalisableRange<float> logRange (float min, float max);
juce::String pctText (float v, int);       // 0..1 -> "42 %"
juce::String dbText (float v, int);        // "+3.0 dB"

} // namespace nob
