#pragma once

#include "dsp/Common.h"
#include <atomic>
#include <cmath>

//==============================================================================
// PULSUS: tempo-synced ducking ("sidechain pump" without a sidechain).
//
//   gain(t) = 1 - DEPTH * duck(phase)
//
// duck() is one of four curves, evaluated on the phase inside each step
// (1/32 note ... 4 bars), shifted by SHIFT. The step clock follows the host
// playhead when it runs and free-runs from the last position otherwise.
//
// Beyond a plain pump:
//   BAND     FULL / LOWS / HIGHS. Linkwitz-Riley split at XOVER: ducking only
//            the lows of a pad or bass leaves room for the kick without
//            making the whole track "shrink".
//   BREATHE  a low-pass that closes in the dip and opens on the recovery,
//            like a synth filter envelope. Much more organic than volume alone.
//==============================================================================
namespace pulsus
{

enum class Shape { pump, soft, ramp, chop, count };
enum class Band  { full, lows, highs };

inline double rateBeats (int rateIndex, double beatsPerBar)
{
    switch (rateIndex)
    {
        case 0: return 0.125;              // 1/32
        case 1: return 0.25;               // 1/16
        case 2: return 1.0 / 3.0;          // 1/8T
        case 3: return 0.5;                // 1/8
        case 4: return 2.0 / 3.0;          // 1/4T
        case 5: return 1.0;                // 1/4
        case 6: return 2.0;                // 1/2
        case 7: return beatsPerBar;        // 1 bar
        case 8: return beatsPerBar * 2.0;  // 2 bars
        default: return beatsPerBar * 4.0; // 4 bars
    }
}

// Duck amount 0..1 at phase u (0..1) of a step lasting stepSec seconds.
inline float duckCurve (Shape shape, float u, float stepSec) noexcept
{
    using namespace nob::dsp;
    const float edge = std::min (0.25f, 0.0035f / std::max (1.0e-4f, stepSec));    // ~3.5 ms anti-click ramp
    if (shape == Shape::chop)
    {
        // trance gate: sound for the first half, cut for the second (smoothed edges)
        if (u < 0.5f - edge) return 0.0f;
        if (u < 0.5f)        return 0.5f - 0.5f * std::cos (pi * (u - (0.5f - edge)) / edge);
        if (u < 1.0f - edge) return 1.0f;
        return 0.5f + 0.5f * std::cos (pi * (u - (1.0f - edge)) / edge);
    }
    if (u < edge)
        return 0.5f - 0.5f * std::cos (pi * u / edge);                // fast, click-free drop
    const float v = (u - edge) / (1.0f - edge);
    switch (shape)
    {
        case Shape::pump:
        {
            // sidechain-compressor recovery: stays down a moment, then swings back
            constexpr float k = 4.6f;
            static const float tail = std::exp (-k);
            const float held = std::max (0.0f, v - 0.06f) / 0.94f;
            return (std::exp (-k * held) - tail) / (1.0f - tail);
        }
        case Shape::soft:  return 0.5f + 0.5f * std::cos (pi * v);    // smooth swell
        case Shape::ramp:  return 1.0f - v;                           // linear fade-in
        case Shape::chop:
        case Shape::count:
        default:           return 0.0f;
    }
}

struct Params
{
    float depth = 0.7f;          // 0..1
    int rate = 5;                // index, see rateBeats()
    Shape shape = Shape::pump;
    float shift = 0.0f;          // 0..1 of a step
    float breathe = 0.0f;        // 0..1
    Band band = Band::full;
    float xover = 150.0f;        // Hz
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        gainSmooth.setTime (0.0004f, sr);
        gainSmooth.reset (1.0f);
        wLow.setTime (0.03f, sr);  wLow.reset (1.0f);
        wHigh.setTime (0.03f, sr); wHigh.reset (1.0f);
        breatheSmooth.setTime (0.02f, sr); breatheSmooth.reset (0.0f);
        for (auto& x : xo) x.reset();
        for (auto& f : breath) f.reset();
        lastXover = -1.0f;
        beat = 0.0;
    }

    void setParams (const Params& np) noexcept { p = np; }

    // Clock: call once per block before process(). hostBeat < 0 = no host position.
    void setTransport (double hostBeat, double bpmIn, double beatsPerBarIn, bool playing) noexcept
    {
        bpm = bpmIn > 1.0 ? bpmIn : 120.0;
        beatsPerBar = beatsPerBarIn > 0.0 ? beatsPerBarIn : 4.0;
        if (playing && hostBeat >= 0.0)
            beat = hostBeat;                // follow the DAW exactly
        synced.store (playing && hostBeat >= 0.0, std::memory_order_relaxed);
    }

    void process (float* L, float* R, int n) noexcept
    {
        if (std::abs (p.xover - lastXover) > 1.0e-3f)
        {
            for (auto& x : xo) x.set (p.xover, sr);
            lastXover = p.xover;
        }
        const double stepBeats = rateBeats (p.rate, beatsPerBar);
        const double beatsPerSample = bpm / 60.0 / sr;
        const float stepSec = (float) (stepBeats * 60.0 / bpm);
        const float tL = p.band == Band::highs ? 0.0f : 1.0f;
        const float tH = p.band == Band::lows ? 0.0f : 1.0f;

        float lastDuck = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double pos = beat / stepBeats - (double) p.shift;
            const float u = (float) (pos - std::floor (pos));
            const float d = duckCurve (p.shape, u, stepSec);
            lastDuck = d;
            const float duck = gainSmooth.next (p.depth * d);       // smoothed amount of reduction
            const float wl = wLow.next (tL), wh = wHigh.next (tH);
            const float gl = 1.0f - duck * wl, gh = 1.0f - duck * wh;

            const float br = breatheSmooth.next (p.breathe);
            const bool filt = br > 1.0e-4f;
            if (filt)
            {
                // cutoff glides from 20 kHz down to ~250 Hz at full dip
                const float dn = nob::dsp::clamp01 (duck / std::max (0.05f, p.depth));
                const float fc = 20000.0f * std::pow (250.0f / 20000.0f, br * dn);
                for (auto& f : breath) f.set (fc, 0.85f, sr);
            }

            for (int ch = 0; ch < (R != nullptr ? 2 : 1); ++ch)
            {
                float* x = ch == 0 ? L : R;
                float lo, hi;
                xo[ch].split (x[i], lo, hi);
                float y = lo * gl + hi * gh;
                if (filt) y = breath[ch].lp (y);
                x[i] = y;
            }
            beat += beatsPerSample;
        }

        // UI taps
        const double pos = beat / stepBeats - (double) p.shift;
        uiPhase.store ((float) (pos - std::floor (pos)), std::memory_order_relaxed);
        const double window = std::max (stepBeats, beatsPerBar);
        const double wpos = beat / window;
        uiWindowPos.store ((float) (wpos - std::floor (wpos)), std::memory_order_relaxed);
        uiDuck.store (lastDuck * p.depth, std::memory_order_relaxed);
        uiBpm.store ((float) bpm, std::memory_order_relaxed);
        uiBeatsPerBar.store ((float) beatsPerBar, std::memory_order_relaxed);
    }

    // UI (lock-free reads)
    std::atomic<float> uiPhase { 0.0f }, uiWindowPos { 0.0f }, uiDuck { 0.0f }, uiBpm { 120.0f }, uiBeatsPerBar { 4.0f };
    std::atomic<bool> synced { false };

private:
    double sr = 48000.0, bpm = 120.0, beatsPerBar = 4.0, beat = 0.0;
    Params p;
    nob::dsp::Smooth gainSmooth, wLow, wHigh, breatheSmooth;
    nob::dsp::Crossover xo[2];
    nob::dsp::Svf breath[2];
    float lastXover = -1.0f;
};

} // namespace pulsus
