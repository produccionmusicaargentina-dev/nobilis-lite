#include "LiteProcessor.h"
#include "Localisation.h"
#include "Diagnostics.h"

using namespace juce;

namespace nob
{

NormalisableRange<float> logRange (float min, float max)
{
    return NormalisableRange<float> (min, max,
        [] (float a, float b, float t) { return a * std::pow (b / a, t); },
        [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
}

String pctText (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; }
String dbText (float v, int)  { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; }

//==============================================================================
void LiteProcessor::addCommonParameters (AudioProcessorValueTreeState::ParameterLayout& layout, float outputMaxDb)
{
    using namespace juce;
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { CommonID::mix, 1 }, "Mix",
                    NormalisableRange<float> (0.0f, 1.0f), 1.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (pctText)));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { CommonID::output, 1 }, "Output",
                    NormalisableRange<float> (-24.0f, outputMaxDb, 0.01f), 0.0f,
                    AudioParameterFloatAttributes().withStringFromValueFunction (dbText).withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { CommonID::bypass, 1 }, "Bypass", false));
}

LiteProcessor::LiteProcessor (AudioProcessorValueTreeState::ParameterLayout layout)
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  AudioChannelSet::stereo(), true)
                        .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", std::move (layout))
{
    diag::step (String (product().name) + " processor: constructor");
    i18n::applySavedLanguage();

    rMix = raw (CommonID::mix);
    rOutput = raw (CommonID::output);
    rBypass = raw (CommonID::bypass);
    bypassParam = dynamic_cast<AudioParameterBool*> (apvts.getParameter (CommonID::bypass));

    apvts.state.setProperty (uiThemeKey, true, nullptr);
    apvts.state.setProperty (uiWidthKey, 0, nullptr);

    presets = std::make_unique<PresetManager> (apvts);
    presets->getExtra = [] { return ValueTree ("EXTRA"); };
    presets->applyExtra = [] (const ValueTree&) {};
    history = std::make_unique<ParamHistory> (*presets);
}

LiteProcessor::~LiteProcessor()
{
    cancelPendingUpdate();
}

LiteProcessor::Raw LiteProcessor::raw (const String& id) const
{
    Raw r;
    r.p = apvts.getRawParameterValue (id);
    jassert (r.p != nullptr);
    return r;
}

bool LiteProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool okOut = out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
    const bool okIn  = in == AudioChannelSet::mono() || in == AudioChannelSet::stereo();
    return okOut && okIn && in.size() <= out.size();
}

void LiteProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    diag::step ("prepareToPlay sr=" + String (sampleRate) + " block=" + String (samplesPerBlock));
    prepared = false;
    baseRate = sampleRate;
    maxBlock = jmax (32, samplesPerBlock);

    prepareEffect (sampleRate, maxBlock);
    latency = getEffectLatency();

    dryLine.setSize (2, nextPowerOfTwo (latency + maxBlock + 1));
    dryLine.clear();
    dryWrite = 0;

    for (auto* s : { &outGain, &mixAmount, &passAmount })
        s->reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (Decibels::decibelsToGain (rOutput.f() + getEffectMakeupDb()));
    mixAmount.setCurrentAndTargetValue (rMix.f());
    passAmount.setCurrentAndTargetValue (rBypass.b() || ! isLicensed() ? 1.0f : 0.0f);

    needsMonoToStereo = getMainBusNumInputChannels() == 1 && getMainBusNumOutputChannels() == 2;
    setLatencySamples (latency);
    prepared = true;
}

void LiteProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    NOB_STEP_ONCE ("processBlock: first call, samples=" + String (buffer.getNumSamples()));
    const int numSamples = buffer.getNumSamples();
    if (! prepared || numSamples <= 0)
        return;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        if (! (needsMonoToStereo && ch == 1))
            buffer.clear (ch, 0, numSamples);
    if (needsMonoToStereo && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // hosts may send blocks bigger than announced: process in prepared-size chunks
    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int len = jmin (maxBlock, numSamples - start);
        AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), jmin (2, buffer.getNumChannels()), start, len);
        processChunk (chunk, len);
    }
}

void LiteProcessor::processChunk (AudioBuffer<float>& io, int n)
{
    const int numCh = io.getNumChannels();
    if (numCh == 0) return;
    const bool mono = numCh == 1;
    float* L = io.getWritePointer (0);
    float* R = mono ? nullptr : io.getWritePointer (1);

    float pkIn = 0.0f;
    for (int i = 0; i < n; ++i) pkIn = jmax (pkIn, std::abs (L[i]), R != nullptr ? std::abs (R[i]) : 0.0f);
    inPeak.store (jmax (inPeak.load (std::memory_order_relaxed), pkIn), std::memory_order_relaxed);

    // dry copy into the latency line
    const int lineLen = dryLine.getNumSamples();
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = (ch == 0 || mono) ? L : R;
        auto* dst = dryLine.getWritePointer (ch);
        for (int i = 0, w = dryWrite; i < n; ++i, w = (w + 1) & (lineLen - 1))
            dst[w] = src[i];
    }

    // the effect always runs (keeps its state warm, so bypass / licence changes do not click)
    processEffect (L, R, n);

    outGain.setTargetValue (Decibels::decibelsToGain (rOutput.f() + getEffectMakeupDb()));
    mixAmount.setTargetValue (rMix.f());
    passAmount.setTargetValue (rBypass.b() || ! isLicensed() ? 1.0f : 0.0f);

    const auto* dl = dryLine.getReadPointer (0);
    const auto* dr = dryLine.getReadPointer (1);
    int rd = (dryWrite - latency + lineLen) & (lineLen - 1);
    float pkOut = 0.0f;
    for (int i = 0; i < n; ++i, rd = (rd + 1) & (lineLen - 1))
    {
        const float g = outGain.getNextValue(), m = mixAmount.getNextValue(), pass = passAmount.getNextValue();
        float wl = dl[rd] + (L[i] * g - dl[rd]) * m;
        L[i] = wl + (dl[rd] - wl) * pass;
        pkOut = jmax (pkOut, std::abs (L[i]));
        if (R != nullptr)
        {
            float wr = dr[rd] + (R[i] * g - dr[rd]) * m;
            R[i] = wr + (dr[rd] - wr) * pass;
            pkOut = jmax (pkOut, std::abs (R[i]));
        }
    }
    dryWrite = (dryWrite + n) & (lineLen - 1);
    outPeak.store (jmax (outPeak.load (std::memory_order_relaxed), pkOut), std::memory_order_relaxed);
}

// Host bypass without our parameter: keep the latency so nothing moves in time
void LiteProcessor::processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer&)
{
    if (! prepared) return;
    const int n = buffer.getNumSamples();
    if (needsMonoToStereo && buffer.getNumChannels() > 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    const int lineLen = dryLine.getNumSamples();
    if (n > lineLen - latency - 1)
        return;                          // absurd block size: leave the audio as is
    for (int ch = 0; ch < jmin (2, buffer.getNumChannels()); ++ch)
    {
        auto* x = buffer.getWritePointer (ch);
        auto* line = dryLine.getWritePointer (ch);
        int w = dryWrite;
        for (int i = 0; i < n; ++i, w = (w + 1) & (lineLen - 1))
        {
            line[w] = x[i];
            x[i] = line[(w - latency + lineLen) & (lineLen - 1)];
        }
    }
    dryWrite = (dryWrite + n) & (lineLen - 1);
}

//==============================================================================
void LiteProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void LiteProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    diag::step ("setStateInformation (" + String (sizeInBytes) + " bytes)");
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            if (presets != nullptr)
                presets->syncAfterStateRestore();
            stateRestored();
        }
}

} // namespace nob
