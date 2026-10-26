#pragma once

#include <juce_dsp/juce_dsp.h>
#include "dsp/Common.h"
#include <atomic>

//==============================================================================
// ADEPS: "make it fatter", one knob.
//
//   GAIN ─> GLUE COMP ─> PRE-EMPHASIS ─┬─ 4x oversampled ──────────────────────────┐
//                                      │  DRIVE ─> SHAPER ─> VOICE FILTER ─> CLIP  │ ─> out
//                                      └───────────────────────────────────────────┘
//
// FATNESS moves everything at once, each with its own curve: compression
// (ratio 1:1 -> 4:1), drive into the shaper (0 -> +24 dB) and the final soft
// clip. It is a macro, not a single process.
//
// COLOR is the Nobilis twist: instead of a tilt EQ it travels through the
// output stages of three classic analog filter topologies, and each one
// saturates differently (so it changes the harmonics, not just the brightness):
//   LADDER      dark and round: symmetric soft clip (odd harmonics), resonant
//               low-pass around 6.5 kHz, a little extra low end
//   OTA         creamy and neutral: asymmetric (even harmonics), gentle top
//   SALLEN-KEY  bright and aggressive: harder knee, more asymmetry, presence
//               boost and a resonant top
// The values are interpolated, so every position in between is valid.
//==============================================================================
namespace adeps
{

struct Voice
{
    float preLowDb, preHighDb, bias, hardness, lpHz, lpQ, postLowDb, postHighDb;

    static Voice lerp (const Voice& a, const Voice& b, float t) noexcept
    {
        auto L = [t] (float x, float y) { return x + (y - x) * t; };
        return { L (a.preLowDb, b.preLowDb), L (a.preHighDb, b.preHighDb), L (a.bias, b.bias), L (a.hardness, b.hardness),
                 a.lpHz * std::pow (b.lpHz / a.lpHz, t), L (a.lpQ, b.lpQ), L (a.postLowDb, b.postLowDb), L (a.postHighDb, b.postHighDb) };
    }

    static Voice at (float color) noexcept
    {
        static const Voice ladder { 3.0f, -2.0f, 0.00f, 0.30f,  6500.0f, 1.10f,  2.0f, -1.0f };
        static const Voice ota    { 1.0f,  0.0f, 0.18f, 0.40f, 14000.0f, 0.71f,  0.5f,  0.5f };
        static const Voice sk     {-2.0f,  4.0f, 0.32f, 0.85f, 17000.0f, 1.50f, -1.0f,  3.0f };
        color = nob::dsp::clamp01 (color);
        return color < 0.5f ? lerp (ladder, ota, color * 2.0f) : lerp (ota, sk, (color - 0.5f) * 2.0f);
    }
};

// Variable-knee sigmoid: hardness 0 = tanh-like, 1 = nearly hard clip
inline float shapeFn (float x, float p) noexcept
{
    return x / std::pow (1.0f + std::pow (std::abs (x), p), 1.0f / p);
}

struct Params
{
    float fatness = 0.35f;    // 0..1
    float color = 0.35f;      // 0..1
    float gainDb = 0.0f;      // input
    bool autoGain = false;
};

class Engine
{
public:
    static constexpr int osFactorLog2 = 2;   // 4x

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        osr = sr * (1 << osFactorLog2);
        os = std::make_unique<juce::dsp::Oversampling<float>> (2, (size_t) osFactorLog2,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        os->initProcessing ((size_t) maxBlock);
        latency = (int) std::round (os->getLatencyInSamples());
        for (auto* s : { &fatS, &colorS, &gainS }) s->setTime (0.03f, sr);
        fatS.reset (p.fatness); colorS.reset (p.color); gainS.reset (nob::dsp::dbToGain (p.gainDb));
        for (int ch = 0; ch < 2; ++ch)
        {
            env[ch] = 0.0f;
            dc[ch].set (osr); dc[ch].reset();
            for (auto* b : { &preLow[ch], &preHigh[ch], &lp[ch], &postLow[ch], &postHigh[ch] }) b->reset();
        }
        rmsIn = rmsOut = 1.0e-4f;
        makeupDb = 0.0f;
        lastVoiceColor = -1.0f;
        updateVoice (p.color);
    }

    int getLatency() const noexcept { return latency; }
    void setParams (const Params& np) noexcept { p = np; }
    float getMakeupDb() const noexcept { return p.autoGain ? makeupDb : 0.0f; }

    void process (float* L, float* R, int n) noexcept
    {
        using namespace nob::dsp;
        const int numCh = R != nullptr ? 2 : 1;
        float* ch[2] = { L, R != nullptr ? R : L };

        // block-rate smoothing of the macro (coefficients follow per block)
        float fat = 0.0f, color = 0.0f;
        for (int i = 0; i < n; ++i) { fat = fatS.next (p.fatness); color = colorS.next (p.color); }
        updateVoice (color);

        const float ratio = 1.0f + 3.0f * fat;
        const float thrDb = -22.0f + 6.0f * (1.0f - fat);
        const float atk = std::exp (-1.0f / (0.004f * (float) sr)), rel = std::exp (-1.0f / (0.090f * (float) sr));
        float inSq = 0.0f, grMax = 0.0f;

        // ---- base rate: input gain, glue compressor, pre-emphasis ----------------------
        const float gTarget = dbToGain (p.gainDb);
        for (int i = 0; i < n; ++i)
        {
            const float g = gainS.next (gTarget);
            float lvl = 0.0f;
            for (int c = 0; c < numCh; ++c) { ch[c][i] *= g; lvl = std::max (lvl, std::abs (ch[c][i])); }
            inSq += lvl * lvl;
            // peak-ish envelope, linked stereo
            float& e = env[0];
            e = lvl > e ? lvl + (e - lvl) * atk : lvl + (e - lvl) * rel;
            const float eDb = gainToDb (e);
            const float over = eDb - thrDb;
            const float grDb = over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
            grMax = std::max (grMax, grDb);
            const float cg = dbToGain (-grDb);
            for (int c = 0; c < numCh; ++c)
                ch[c][i] = preHigh[c].process (preLow[c].process (ch[c][i] * cg));
        }
        uiInDb.store (gainToDb (std::sqrt (inSq / (float) std::max (1, n))), std::memory_order_relaxed);
        uiGr.store (grMax, std::memory_order_relaxed);

        // ---- 4x: drive, shaper, voice filter, clip ------------------------------------------
        juce::dsp::AudioBlock<float> block (ch, (size_t) numCh, (size_t) n);
        auto up = os->processSamplesUp (block);
        const float drive = dbToGain (24.0f * std::pow (fat, 1.3f));
        const float bias = voice.bias * (0.3f + 0.7f * fat);
        const float pw = 2.0f + 10.0f * voice.hardness;
        const float b0 = shapeFn (bias, pw);
        const float clipAmt = clamp01 ((fat - 0.15f) / 0.6f);
        const float ceiling = 0.966f;                 // -0.3 dBFS
        const int upN = (int) up.getNumSamples();
        for (int c = 0; c < numCh; ++c)
        {
            float* x = up.getChannelPointer ((size_t) c);
            for (int i = 0; i < upN; ++i)
            {
                float y = shapeFn (x[i] * drive + bias, pw) - b0;
                y = dc[c].process (y);
                y = postHigh[c].process (postLow[c].process (lp[c].process (y)));
                const float clipped = ceiling * std::tanh (y / ceiling);
                x[i] = y + (clipped - y) * clipAmt;
            }
        }
        os->processSamplesDown (block);

        // ---- level follower for AUTO GAIN (slow, so it never pumps) ---------------------------
        float outSq = 0.0f;
        for (int i = 0; i < n; ++i) { float v = 0.0f; for (int c = 0; c < numCh; ++c) v = std::max (v, std::abs (ch[c][i])); outSq += v * v; }
        const float k = std::exp (-(float) n / (0.6f * (float) sr));
        rmsIn  = inSq  / (float) std::max (1, n) + (rmsIn  - inSq  / (float) std::max (1, n)) * k;
        rmsOut = outSq / (float) std::max (1, n) + (rmsOut - outSq / (float) std::max (1, n)) * k;
        if (rmsIn > 1.0e-7f && rmsOut > 1.0e-7f)
            makeupDb = std::clamp (10.0f * std::log10 (rmsIn / rmsOut), -24.0f, 12.0f);
        uiFat.store (fat, std::memory_order_relaxed);
        uiOutDb.store (gainToDb (std::sqrt (outSq / (float) std::max (1, n))), std::memory_order_relaxed);
    }

    // transfer curve for the display (no filters, base settings)
    float transfer (float x) const noexcept
    {
        const float fat = p.fatness;
        const auto v = Voice::at (p.color);
        const float drive = nob::dsp::dbToGain (24.0f * std::pow (fat, 1.3f));
        const float bias = v.bias * (0.3f + 0.7f * fat);
        const float pw = 2.0f + 10.0f * v.hardness;
        float y = shapeFn (x * drive + bias, pw) - shapeFn (bias, pw);
        const float clipAmt = nob::dsp::clamp01 ((fat - 0.15f) / 0.6f);
        return y + (0.966f * std::tanh (y / 0.966f) - y) * clipAmt;
    }

    std::atomic<float> uiInDb { -100.0f }, uiOutDb { -100.0f }, uiGr { 0.0f }, uiFat { 0.0f };

private:
    void updateVoice (float color) noexcept
    {
        if (std::abs (color - lastVoiceColor) < 1.0e-4f) return;
        lastVoiceColor = color;
        voice = Voice::at (color);
        for (int c = 0; c < 2; ++c)
        {
            preLow[c].lowShelf (120.0f, 0.7f, voice.preLowDb, sr);
            preHigh[c].highShelf (2800.0f, 0.7f, voice.preHighDb, sr);
            lp[c].lowPass (voice.lpHz, voice.lpQ, osr);
            postLow[c].lowShelf (100.0f, 0.7f, voice.postLowDb, osr);
            postHigh[c].highShelf (5000.0f, 0.7f, voice.postHighDb, osr);
        }
    }

    double sr = 48000.0, osr = 192000.0;
    Params p;
    Voice voice = Voice::at (0.35f);
    float lastVoiceColor = -1.0f;
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int latency = 0;
    nob::dsp::Smooth fatS, colorS, gainS;
    float env[2] {};
    nob::dsp::Biquad preLow[2], preHigh[2], lp[2], postLow[2], postHigh[2];
    nob::dsp::DcBlock dc[2];
    float rmsIn = 1.0e-4f, rmsOut = 1.0e-4f, makeupDb = 0.0f;
};

} // namespace adeps
