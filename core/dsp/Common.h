#pragma once

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <memory>

//==============================================================================
// Small DSP pieces shared by the Lite engines. No JUCE: the engines are plain
// C++ so the tests can drive them sample by sample.
//==============================================================================
namespace nob::dsp
{

constexpr float pi = 3.14159265358979f;

inline float clamp01 (float x) noexcept { return std::min (1.0f, std::max (0.0f, x)); }
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (std::max (1.0e-9f, g)); }

// One-pole smoother (time constant in seconds)
struct Smooth
{
    float y = 0.0f, a = 0.0f;
    void setTime (float seconds, double sr) noexcept { a = seconds <= 0.0f ? 0.0f : std::exp (-1.0f / (seconds * (float) sr)); }
    void reset (float v) noexcept { y = v; }
    float next (float x) noexcept { y = x + (y - x) * a; return y; }
};

// Topology-preserving-transform state variable filter (Zavalishin). Stable
// under fast modulation, which is what the breathing filters need.
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float g = 0.0f, k = 1.414f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void set (float freq, float q, double sr) noexcept
    {
        freq = std::min (freq, (float) sr * 0.49f);
        g = std::tan (pi * freq / (float) sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    struct Out { float lp, bp, hp; };
    Out tick (float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }
    float lp (float x) noexcept { return tick (x).lp; }
    float hp (float x) noexcept { return tick (x).hp; }
};

// Linkwitz-Riley 4th-order crossover: two Butterworth sections per side.
// low + high is an all-pass (flat magnitude), so splitting costs no tone.
struct Crossover
{
    Svf lp1, lp2, hp1, hp2;
    void reset() noexcept { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    void set (float freq, double sr) noexcept
    {
        for (auto* f : { &lp1, &lp2, &hp1, &hp2 }) f->set (freq, 0.70710678f, sr);
    }
    void split (float x, float& low, float& high) noexcept
    {
        low = lp2.lp (lp1.lp (x));
        high = hp2.hp (hp1.hp (x));
    }
};

// RBJ biquad (transposed direct form II) for static tone shaping
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() noexcept { z1 = z2 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setNorm (float B0, float B1, float B2, float A0, float A1, float A2) noexcept
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void lowShelf (float f, float q, float db, double sr) noexcept
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2.0f * pi * f / (float) sr, c = std::cos (w), s = std::sin (w);
        const float al = s / (2.0f * q), sq = 2.0f * std::sqrt (A) * al;
        setNorm (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                 (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (float f, float q, float db, double sr) noexcept
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2.0f * pi * f / (float) sr, c = std::cos (w), s = std::sin (w);
        const float al = s / (2.0f * q), sq = 2.0f * std::sqrt (A) * al;
        setNorm (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                 (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
    void peak (float f, float q, float db, double sr) noexcept
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2.0f * pi * f / (float) sr, c = std::cos (w), s = std::sin (w);
        const float al = s / (2.0f * q);
        setNorm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowPass (float f, float q, double sr) noexcept
    {
        const float w = 2.0f * pi * std::min (f, (float) sr * 0.49f) / (float) sr, c = std::cos (w), s = std::sin (w), al = s / (2.0f * q);
        setNorm ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void highPass (float f, float q, double sr) noexcept
    {
        const float w = 2.0f * pi * f / (float) sr, c = std::cos (w), s = std::sin (w), al = s / (2.0f * q);
        setNorm ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
};

// One-pole DC blocker (after asymmetric shaping)
struct DcBlock
{
    float x1 = 0.0f, y1 = 0.0f, r = 0.9995f;
    void set (double sr) noexcept { r = 1.0f - (2.0f * pi * 8.0f / (float) sr); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// Fast, decent white noise (xorshift32), -1..1
struct Noise
{
    uint32_t s = 0x9e3779b9u;
    explicit Noise (uint32_t seed = 0x9e3779b9u) : s (seed ? seed : 1u) {}
    float next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
    float uni() noexcept { return 0.5f * (next() + 1.0f); }     // 0..1
};

// Fractional delay line with 4-point Hermite interpolation
struct DelayLine
{
    float* buf = nullptr;
    int size = 0, mask = 0, w = 0;
    std::unique_ptr<float[]> mem;

    void prepare (int maxSamples)
    {
        size = 1;
        while (size < maxSamples + 8) size <<= 1;
        mask = size - 1;
        mem.reset (new float[(size_t) size]());
        buf = mem.get();
        w = 0;
    }
    void reset() noexcept { if (buf) std::fill (buf, buf + size, 0.0f); w = 0; }
    void push (float x) noexcept { buf[w] = x; w = (w + 1) & mask; }
    // delay in samples (>= 1)
    float read (float d) const noexcept
    {
        const float rp = (float) w - d;
        const int i = (int) std::floor (rp);
        const float f = rp - (float) i;
        const float xm1 = buf[(i - 1) & mask], x0 = buf[i & mask], x1 = buf[(i + 1) & mask], x2 = buf[(i + 2) & mask];
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

} // namespace nob::dsp
