#pragma once

#include "dsp/Common.h"
#include <atomic>
#include <array>

//==============================================================================
// VETUS: a lo-fi machine with four media. AGE is the macro: it takes the
// chosen medium from "almost new" to "found in a basement".
//
//   TAPE     wow and flutter, head bump, soft saturation, top-end loss, hiss
//   VINYL    33 rpm wow, surface noise, crackle and pops, rumble, narrower image
//   SAMPLER  early 12-bit sampler: lower sample rate with no anti-alias filter
//            (the aliasing is the sound), fewer bits, a touch of grit
//   RADIO    AM band: narrow band-pass, mono, hard clipping, static, drift
//
// NOISE and WOBBLE trim the noise and the pitch movement of the medium.
// GATE NOISE (on by default) only lets the noise through while audio plays,
// so the track ends in true silence.
//
// The wobble is a modulated delay around 6 ms; that centre delay is reported
// as latency so MIX stays aligned with the dry signal.
//==============================================================================
namespace vetus
{

enum class Medium { tape, vinyl, sampler, radio };

struct Params
{
    float age = 0.45f;          // 0..1
    Medium medium = Medium::tape;
    float noise = 0.5f;         // 0..1 (0.5 = the medium's own amount)
    float wobble = 0.5f;        // 0..1
    bool gateNoise = true;
};

class Engine
{
public:
    static constexpr float centreMs = 6.0f;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        latency = (int) std::round (centreMs * 0.001 * sr);
        for (auto& d : lines) { d.prepare ((int) (0.02 * sr)); d.reset(); }
        for (int c = 0; c < 2; ++c)
        {
            for (auto* b : { &hp[c], &lp[c], &lp2[c], &bump[c], &hissHp[c], &hissLp[c] }) b->reset();
            dc[c].set (sr); dc[c].reset();
            held[c] = 0.0f;
        }
        rumbleLp.reset();
        holdPhase = 0.0;
        wowPh = flutPh = 0.0;
        drift = driftTarget = 0.0f;
        env = 0.0f;
        crackle = 0.0f;
        lastKey = -1.0f;
        ageS.setTime (0.05f, sr); ageS.reset (p.age);
        rotation = 0.0;
    }

    int getLatency() const noexcept { return latency; }
    void setParams (const Params& np) noexcept { p = np; }

    void process (float* L, float* R, int n) noexcept
    {
        using namespace nob::dsp;
        const bool stereo = R != nullptr;
        float age = 0.0f;
        for (int i = 0; i < n; ++i) age = ageS.next (p.age);
        updateFilters (age);

        const auto m = p.medium;
        const float a = age;
        // wow / flutter depths in ms
        float wowMs = 0.0f, flutMs = 0.0f, wowHz = 0.5f, flutHz = 6.5f;
        float drive = 1.0f, hardClip = 0.0f, width = 1.0f;
        float hiss = 0.0f, crackRate = 0.0f, crackAmt = 0.0f, rumble = 0.0f, staticAmt = 0.0f;
        switch (m)
        {
            case Medium::tape:    wowMs = 1.6f * a; flutMs = 0.10f + 0.18f * a; wowHz = 0.45f; flutHz = 7.2f;
                                  drive = 1.0f + 3.0f * a; hiss = 0.0009f + 0.006f * a; width = 1.0f - 0.15f * a; break;
            case Medium::vinyl:   wowMs = 0.9f * a; flutMs = 0.03f; wowHz = 0.555f; flutHz = 9.0f;
                                  drive = 1.0f + 1.0f * a; hiss = 0.0006f + 0.002f * a; crackRate = 1.5f + 28.0f * a; crackAmt = 0.06f + 0.30f * a;
                                  rumble = 0.004f + 0.01f * a; width = 1.0f - 0.45f * a; break;
            case Medium::sampler: wowMs = 0.0f; flutMs = 0.0f; drive = 1.0f + 0.8f * a; width = 1.0f; break;
            case Medium::radio:   wowMs = 0.5f * a; flutMs = 0.05f * a; wowHz = 0.18f; flutHz = 4.0f;
                                  drive = 2.0f + 8.0f * a; hardClip = 0.5f + 0.5f * a; width = 0.0f; staticAmt = 0.004f + 0.02f * a;
                                  crackRate = 0.5f + 6.0f * a; crackAmt = 0.03f + 0.08f * a; break;
        }
        const float wob = 2.0f * p.wobble;                  // 0.5 = medium default (x1)
        const float nz = 2.0f * p.noise;
        wowMs *= wob; flutMs *= wob;

        // sampler: rate and bits
        const float holdRate = 26000.0f * std::pow (8000.0f / 26000.0f, a);           // 26 kHz -> 8 kHz
        const float bits = 14.0f - 8.0f * a;                                           // 14 -> 6 bits
        const float q = std::pow (2.0f, bits - 1.0f);
        const double holdStep = (double) holdRate / sr;
        uiRate.store (holdRate, std::memory_order_relaxed);
        uiBits.store (bits, std::memory_order_relaxed);

        const float msToS = (float) sr * 0.001f;
        const float relEnv = std::exp (-1.0f / (0.35f * (float) sr));
        float sumWow = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // slow random drift (tape stretch, radio tuning)
            if ((++driftCount & 1023) == 0) driftTarget = noise.next();
            drift += (driftTarget - drift) * 0.00002f * (float) (48000.0 / sr);

            wowPh += wowHz / sr;  if (wowPh >= 1.0) wowPh -= 1.0;
            flutPh += flutHz / sr; if (flutPh >= 1.0) flutPh -= 1.0;
            const float wow = (float) std::sin (2.0 * 3.14159265358979 * wowPh) * 0.75f + drift * 0.5f;
            const float flut = (float) std::sin (2.0 * 3.14159265358979 * flutPh) + 0.3f * noise.next();
            const float dly = (centreMs + wowMs * wow + flutMs * flut) * msToS;
            sumWow += wow;

            float x[2] = { L[i], stereo ? R[i] : L[i] };
            const float lvl = std::max (std::abs (x[0]), std::abs (x[1]));
            env = lvl > env ? lvl : lvl + (env - lvl) * relEnv;
            const float gate = p.gateNoise ? clamp01 (env * 40.0f) : 1.0f;

            // image: narrower with age (mono for radio)
            const float mid = 0.5f * (x[0] + x[1]), side = 0.5f * (x[0] - x[1]) * width;
            x[0] = mid + side; x[1] = mid - side;

            // sampler: sample-and-hold without anti-aliasing, then fewer bits
            if (m == Medium::sampler)
            {
                holdPhase += holdStep;
                if (holdPhase >= 1.0)
                {
                    holdPhase -= std::floor (holdPhase);
                    for (int c = 0; c < 2; ++c) held[c] = std::round (x[c] * q) / q;
                }
                x[0] = held[0]; x[1] = held[1];
            }

            // shared crackle / static events (same on both sides: they live on the medium)
            float event = 0.0f;
            if (crackRate > 0.0f && noise.uni() < crackRate / (float) sr)
                crackle = (noise.uni() < 0.12f ? 2.5f : 1.0f) * crackAmt * (0.4f + 0.6f * noise.uni()) * (noise.next() > 0 ? 1.0f : -1.0f);
            event = crackle;
            crackle *= 0.86f;                                    // a click lasts a few samples
            const float rum = rumble > 0.0f ? rumbleLp.process (noise.next()) * rumble * 8.0f : 0.0f;

            for (int c = 0; c < 2; ++c)
            {
                lines[(size_t) c].push (x[c]);
                float y = lines[(size_t) c].read (std::max (2.0f, dly));

                // medium tone
                y = bump[c].process (hp[c].process (y));
                // saturation: soft (tape, vinyl), hard (radio)
                const float soft = std::tanh (y * drive) / drive * (1.0f + 0.15f * (drive - 1.0f));
                const float hard = std::clamp (y * drive, -1.0f, 1.0f) * 0.5f;
                y = soft + (hard - soft) * hardClip;
                y = lp2[c].process (lp[c].process (y));
                y = dc[c].process (y);

                // noise floor of the medium
                const float hissN = hissLp[c].process (hissHp[c].process (noise.next()));
                y += (hiss + staticAmt) * nz * gate * hissN * 3.0f;
                y += (event + rum) * nz * gate;
                (c == 0 ? L : R)[i] = y;
                if (! stereo) break;
            }
        }

        rotation += (double) n / sr * (1.0 + 0.04 * (double) (sumWow / (float) std::max (1, n)) * (double) wob);
        uiRotation.store ((float) rotation, std::memory_order_relaxed);
        uiWow.store (wowMs * 0.5f, std::memory_order_relaxed);
        uiAge.store (a, std::memory_order_relaxed);
    }

    std::atomic<float> uiRotation { 0.0f }, uiWow { 0.0f }, uiAge { 0.0f }, uiRate { 26000.0f }, uiBits { 14.0f };

private:
    void updateFilters (float a) noexcept
    {
        const float key = a + 10.0f * (float) p.medium;
        if (std::abs (key - lastKey) < 1.0e-4f) return;
        lastKey = key;
        float hpHz = 20.0f, lpHz = 20000.0f, bumpDb = 0.0f, lpQ = 0.707f;
        float hissHpHz = 1500.0f, hissLpHz = 12000.0f;
        switch (p.medium)
        {
            case Medium::tape:    hpHz = 25.0f + 35.0f * a; lpHz = 19000.0f * std::pow (0.32f, a); bumpDb = 3.5f * a; break;
            case Medium::vinyl:   hpHz = 22.0f + 20.0f * a; lpHz = 17000.0f * std::pow (0.42f, a); bumpDb = 1.0f * a; hissHpHz = 800.0f; break;
            case Medium::sampler: hpHz = 25.0f; lpHz = 0.48f * 26000.0f * std::pow (8000.0f / 26000.0f, a) + 1200.0f; lpQ = 0.6f; break;
            case Medium::radio:   hpHz = 280.0f + 300.0f * a; lpHz = 4200.0f - 1700.0f * a; lpQ = 1.1f; hissHpHz = 600.0f; hissLpHz = 5000.0f; break;
        }
        for (int c = 0; c < 2; ++c)
        {
            hp[c].highPass (hpHz, 0.707f, sr);
            lp[c].lowPass (lpHz, lpQ, sr);
            lp2[c].lowPass (std::min (lpHz * 1.3f, (float) sr * 0.45f), 0.6f, sr);
            bump[c].peak (85.0f, 1.0f, bumpDb, sr);
            hissHp[c].highPass (hissHpHz, 0.707f, sr);
            hissLp[c].lowPass (hissLpHz, 0.707f, sr);
        }
        rumbleLp.lowPass (28.0f, 0.707f, sr);
    }

    double sr = 48000.0, holdPhase = 0.0, wowPh = 0.0, flutPh = 0.0, rotation = 0.0;
    Params p;
    int latency = 0;
    std::array<nob::dsp::DelayLine, 2> lines;
    nob::dsp::Biquad hp[2], lp[2], lp2[2], bump[2], hissHp[2], hissLp[2], rumbleLp;
    nob::dsp::DcBlock dc[2];
    nob::dsp::Noise noise { 0x7e75u };
    nob::dsp::Smooth ageS;
    float held[2] {}, drift = 0.0f, driftTarget = 0.0f, env = 0.0f, crackle = 0.0f, lastKey = -1.0f;
    unsigned driftCount = 0;
};

} // namespace vetus
