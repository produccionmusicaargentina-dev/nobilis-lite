#pragma once

#include "dsp/Common.h"
#include <atomic>
#include <array>

//==============================================================================
// LATUS: the classic poly-synth "bucket brigade" chorus, made one-knob.
//
// Two modulated delay lines (BBD style) fed with the mono sum, their LFOs in
// opposite phase, one per side: that is what makes the famous wide, liquid
// stereo from a mono source. The modes follow the two-button synths:
//   I         0.51 Hz, deep        (slow, wide, the pad classic)
//   II        0.86 Hz, deep        (faster, more obvious)
//   I + II    9.75 Hz, shallow     (fast shimmer, almost a rotary)
//   ENSEMBLE  three voices at 120 degrees + a light vibrato: the string-machine
//             sound (the Nobilis extra)
//
// WIDTH is the macro: wet level and modulation depth together.
// AGE is the bucket brigade itself: darker anti-alias filters, compander
// saturation and hiss (gated, so silence stays silent).
// BASS MONO keeps everything under 150 Hz dry and centred: safe on bass and
// kick, mono-compatible on the club system.
//==============================================================================
namespace latus
{

enum class Mode { one, two, both, ensemble };

struct Params
{
    float width = 0.6f;     // 0..1
    Mode mode = Mode::one;
    float speed = 1.0f;     // 0.25..4 x
    float age = 0.25f;      // 0..1
    bool bassMono = true;
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        const int maxDelay = (int) std::ceil (0.020 * sr);
        for (auto& d : lines) d.prepare (maxDelay);
        for (auto& x : xo) x.reset();
        for (auto* f : { &preL, &postA[0], &postA[1], &postB[0], &postB[1], &hissLp }) f->reset();
        for (auto& x : xo) x.set (150.0f, sr);
        widthS.setTime (0.05f, sr); widthS.reset (p.width);
        bassS.setTime (0.05f, sr); bassS.reset (p.bassMono ? 1.0f : 0.0f);
        envFollow = 0.0f;
        phase = 0.0;
        vib = 0.0;
        modeMix = { 0, 0, 0, 0 };
        modeMix[(size_t) p.mode] = 1.0f;
        lastAge = -1.0f;
        writePos.store (0);
    }

    void setParams (const Params& np) noexcept { p = np; }

    void process (float* L, float* R, int n) noexcept
    {
        using namespace nob::dsp;
        updateFilters();
        const bool stereo = R != nullptr;

        // modes cross-fade (switching never clicks)
        const float mk = 1.0f - std::exp (-(float) n / (0.04f * (float) sr));
        for (size_t m = 0; m < 4; ++m)
            modeMix[m] += (((size_t) p.mode == m ? 1.0f : 0.0f) - modeMix[m]) * mk;

        const double rateHz = (modeMix[0] * 0.513 + modeMix[1] * 0.863 + modeMix[2] * 9.75 + modeMix[3] * 0.62) * p.speed;
        const float msToS = (float) sr * 0.001f;
        const float hissAmt = 0.00012f + 0.0012f * p.age * p.age;      // ~ -78 dB .. -56 dB
        const float drive = 1.0f + 2.5f * p.age;
        const float relEnv = std::exp (-1.0f / (0.25f * (float) sr));

        for (int i = 0; i < n; ++i)
        {
            const float w = widthS.next (p.width);
            const float bm = bassS.next (p.bassMono ? 1.0f : 0.0f);
            const float xl = L[i], xr = stereo ? R[i] : L[i];

            // split: lows stay dry (and go mono when BASS MONO is on)
            float lowL, highL, lowR, highR;
            xo[0].split (xl, lowL, highL);
            xo[1].split (xr, lowR, highR);
            const float lowMono = 0.5f * (lowL + lowR);
            const float inMono = 0.5f * ((xl + xr) - bm * (lowL + lowR));          // chorus input: mono sum (without the lows)

            // envelope for the gated hiss
            const float a = std::abs (inMono);
            envFollow = a > envFollow ? a : a + (envFollow - a) * relEnv;
            const float gate = clamp01 (envFollow * 60.0f);

            // bucket brigade input: anti-alias filter, compander drive
            float bbdIn = preL.process (inMono);
            bbdIn = std::tanh (bbdIn * drive) / drive;
            bbdIn += hissAmt * gate * hissLp.process (noise.next());
            for (auto& d : lines) d.push (bbdIn);

            // LFOs (triangle), the two sides in opposite phase
            phase += rateHz / sr;
            if (phase >= 1.0) phase -= 1.0;
            vib += 6.1 / sr;
            if (vib >= 1.0) vib -= 1.0;
            auto tri = [] (double ph) { const double t = ph - std::floor (ph); return (float) (t < 0.5 ? 4.0 * t - 1.0 : 3.0 - 4.0 * t); };
            const float t0 = tri (phase), t1 = tri (phase + 0.5);
            const float t120 = tri (phase + 1.0 / 3.0), t240 = tri (phase + 2.0 / 3.0);
            const float v = (float) std::sin (2.0 * 3.14159265358979 * vib);

            // depth: classic modes sweep 1.66 .. 5.35 ms; I+II is shallow and fast
            const float depthScale = 0.45f + 0.55f * w;
            const float centreMs = 3.5f, swingMs = 1.85f * depthScale;
            const float fastCentre = 3.3f, fastSwing = 0.32f * depthScale;
            const float slow = modeMix[0] + modeMix[1], fast = modeMix[2], ens = modeMix[3];

            const float dA = (slow * (centreMs + swingMs * t0) + fast * (fastCentre + fastSwing * t0)
                              + ens * (5.0f + 1.6f * depthScale * t0 + 0.12f * v)) * msToS;
            const float dB = (slow * (centreMs + swingMs * t1) + fast * (fastCentre + fastSwing * t1)
                              + ens * (5.0f + 1.6f * depthScale * t120 + 0.12f * v)) * msToS;
            const float dC = (5.0f + 1.6f * depthScale * t240 + 0.12f * v) * msToS;

            float wa = postB[0].process (postA[0].process (lines[0].read (std::max (2.0f, dA))));
            float wb = postB[1].process (postA[1].process (lines[1].read (std::max (2.0f, dB))));
            const float wc = lines[2].read (std::max (2.0f, dC));
            // ensemble: third voice shared between the sides
            wa += ens * 0.5f * wc;
            wb += ens * 0.5f * wc;

            const float wetGain = w * (1.0f - 0.25f * ens);
            const float norm = 1.0f / (1.0f + 0.35f * w);
            const float keepL = xl - bm * lowL + bm * lowMono;      // dry, with mono lows when BASS MONO is on
            const float keepR = xr - bm * lowR + bm * lowMono;
            const float outL = (keepL + wa * wetGain) * norm;
            const float outR = (keepR + wb * wetGain) * norm;

            if (stereo) { L[i] = outL; R[i] = outR; }
            else        L[i] = 0.5f * (outL + outR);

            // scope tap (decimated)
            if ((++tapCount & 15) == 0)
            {
                const int wp = writePos.load (std::memory_order_relaxed);
                scopeL[(size_t) wp].store (outL, std::memory_order_relaxed);
                scopeR[(size_t) wp].store (outR, std::memory_order_relaxed);
                writePos.store ((wp + 1) % scopeSize, std::memory_order_relaxed);
            }
        }
        uiPhase.store ((float) phase, std::memory_order_relaxed);
    }

    // UI: a lock-free ring of recent output points (for the stereo scope)
    static constexpr int scopeSize = 512;
    std::array<std::atomic<float>, scopeSize> scopeL {}, scopeR {};
    std::atomic<int> writePos { 0 };
    std::atomic<float> uiPhase { 0.0f };

private:
    void updateFilters() noexcept
    {
        if (std::abs (p.age - lastAge) < 1.0e-4f) return;
        lastAge = p.age;
        const float fc = 9500.0f * std::pow (0.45f, p.age);             // 9.5 kHz -> ~4.3 kHz
        preL.lowPass (fc, 0.707f, sr);
        for (int k = 0; k < 2; ++k)
        {
            postA[k].lowPass (fc, 0.54f, sr);
            postB[k].lowPass (fc * 1.15f, 1.31f, sr);                   // 4th-order Butterworth-ish
        }
        hissLp.lowPass (6000.0f, 0.707f, sr);
    }

    double sr = 48000.0, phase = 0.0, vib = 0.0;
    Params p;
    std::array<nob::dsp::DelayLine, 3> lines;
    nob::dsp::Crossover xo[2];
    nob::dsp::Biquad preL, postA[2], postB[2], hissLp;
    nob::dsp::Smooth widthS, bassS;
    nob::dsp::Noise noise { 0x51f15eedu };
    std::array<float, 4> modeMix {};
    float envFollow = 0.0f, lastAge = -1.0f;
    unsigned tapCount = 0;
};

} // namespace latus
