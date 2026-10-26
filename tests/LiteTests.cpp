#include <functional>
// Offline tests for one Lite plugin (the build makes one executable per plugin).
//
//   PULSUS_Tests checks                 engine + plugin checks, CPU
//   PULSUS_Tests render <folder>        WAV of the demo loop, dry and through every factory preset
//   PULSUS_Tests snapshot <png> [nox|lux] [preset name] [scale]
//   PULSUS_Tests card <folder> [nox|lux] [preset name]     the share card
//
// Built with NOB_REQUIRE_LOGIN=0 (no server needed).

#include <juce_audio_utils/juce_audio_utils.h>
#include "LiteProcessor.h"
#include "ui/LiteEditor.h"
#include "Product.h"

using namespace juce;

namespace
{
int failures = 0;
void check (bool ok, const String& what)
{
    std::cout << (ok ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

//==============================================================================
// A host clock for tempo-synced plugins
struct TestPlayHead : public AudioPlayHead
{
    double bpm = 124.0, sr = 48000.0;
    int64 sample = 0;
    bool playing = true;
    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsPlaying (playing);
        p.setTimeInSamples (sample);
        p.setPpqPosition ((double) sample / sr * bpm / 60.0);
        return p;
    }
};

//==============================================================================
// Demo loop: 8 bars at 124 BPM. Kick, hats, clap, a saw-pad progression and a bass,
// written in code so the renders are reproducible and need no sample files.
AudioBuffer<float> makeDemoLoop (double sr, double bpm = 124.0, int bars = 8)
{
    const double beatSec = 60.0 / bpm;
    const int len = (int) std::round (bars * 4 * beatSec * sr);
    AudioBuffer<float> b (2, len);
    b.clear();
    Random rnd (1234);
    auto add = [&] (int ch, int i, float v) { if (i >= 0 && i < len) b.addSample (ch, i, v); };

    const float chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 }, { 52, 55, 59 } };   // Am F G Em
    const float bassNotes[4] = { 45, 41, 43, 40 };
    auto hz = [] (float midi) { return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f); };

    // pad + bass (one chord per bar)
    std::array<double, 6> ph {};
    double bassPh = 0.0, lpL = 0.0, lpR = 0.0, bassLp = 0.0;
    for (int i = 0; i < len; ++i)
    {
        const double t = i / sr;
        const int bar = (int) (t / (4 * beatSec)) % 4;
        float padL = 0, padR = 0;
        for (int v = 0; v < 3; ++v)
            for (int d = 0; d < 2; ++d)
            {
                auto& p = ph[(size_t) (v * 2 + d)];
                p += hz (chords[bar][v]) * (d == 0 ? 0.997 : 1.003) / sr;
                p -= std::floor (p);
                const float saw = (float) (2.0 * p - 1.0);
                (d == 0 ? padL : padR) += saw * 0.06f;
            }
        lpL += 0.08 * (padL - lpL);
        lpR += 0.08 * (padR - lpR);
        add (0, i, (float) lpL); add (1, i, (float) lpR);

        const double inBeat = std::fmod (t, beatSec / 2.0);              // bass on 8ths
        bassPh += hz (bassNotes[bar]) / sr; bassPh -= std::floor (bassPh);
        const float env = (float) std::exp (-inBeat * 9.0);
        const float bassRaw = (float) ((bassPh < 0.5 ? 1.0 : -1.0)) * env;
        bassLp += 0.05 * (bassRaw - bassLp);
        add (0, i, (float) bassLp * 0.22f); add (1, i, (float) bassLp * 0.22f);
    }
    // drums
    const int beats = bars * 4;
    for (int bt = 0; bt < beats; ++bt)
    {
        const int s0 = (int) std::round (bt * beatSec * sr);
        for (int i = 0; i < (int) (0.35 * sr); ++i)                       // kick
        {
            const double t = i / sr;
            const double f = 48.0 + 110.0 * std::exp (-t * 30.0);
            const float v = (float) (std::sin (2 * MathConstants<double>::pi * f * t) * std::exp (-t * 7.0)) * 0.75f;
            add (0, s0 + i, v); add (1, s0 + i, v);
        }
        if (bt % 2 == 1)                                                   // clap
            for (int i = 0; i < (int) (0.18 * sr); ++i)
            {
                const double t = i / sr;
                const float v = (rnd.nextFloat() * 2 - 1) * (float) std::exp (-t * 22.0) * 0.28f;
                add (0, s0 + i, v); add (1, s0 + i, v * 0.9f);
            }
        for (int h = 0; h < 2; ++h)                                        // hats on 8ths
        {
            const int hs = s0 + (int) std::round (h * beatSec * 0.5 * sr);
            float prev = 0.0f;
            for (int i = 0; i < (int) (0.05 * sr); ++i)
            {
                const float n = rnd.nextFloat() * 2 - 1;
                const float hp = n - prev; prev = n;
                const float v = hp * (float) std::exp (-(i / sr) * 70.0) * (h == 1 ? 0.10f : 0.06f);
                add (0, hs + i, v * 0.8f); add (1, hs + i, v);
            }
        }
    }
    b.applyGain (0.8f);
    return b;
}

void writeWav (const File& f, const AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<OutputStream> os (f.createOutputStream().release());
    if (os == nullptr) return;
    auto opts = AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (b.getNumChannels()).withBitsPerSample (24);
    if (auto w = wav.createWriterFor (os, opts))
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

std::unique_ptr<nob::LiteProcessor> makeProcessor()
{
    std::unique_ptr<AudioProcessor> p (createPluginFilter());
    return std::unique_ptr<nob::LiteProcessor> (dynamic_cast<nob::LiteProcessor*> (p.release()));
}

// Runs a buffer through the processor in blocks, with the host clock
void runThrough (nob::LiteProcessor& p, AudioBuffer<float>& b, double sr, int block, TestPlayHead* ph = nullptr)
{
    MidiBuffer midi;
    for (int s = 0; s < b.getNumSamples(); s += block)
    {
        const int n = jmin (block, b.getNumSamples() - s);
        AudioBuffer<float> chunk (b.getArrayOfWritePointers(), b.getNumChannels(), s, n);
        if (ph) ph->sample = s;
        p.processBlock (chunk, midi);
    }
}

int findPreset (nob::LiteProcessor& p, const String& name)
{
    const auto& list = p.presets->getPresets();
    for (int i = 0; i < (int) list.size(); ++i)
        if (list[(size_t) i].name.equalsIgnoreCase (name)) return i;
    return -1;
}

void setParam (nob::LiteProcessor& p, const String& id, float realValue)
{
    if (auto* rp = p.apvts.getParameter (id))
        rp->setValueNotifyingHost (rp->convertTo0to1 (realValue));
}

//==============================================================================
void runChecks()
{
    const double sr = 48000.0;
    std::cout << nob::product().name << " checks" << std::endl;
    auto loop = makeDemoLoop (sr, 124.0, 2);

    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto p = makeProcessor();
        TestPlayHead ph; ph.sr = rate;
        p->setPlayHead (&ph);
        p->setPlayConfigDetails (2, 2, rate, 512);
        p->prepareToPlay (rate, 512);

        // MIX = 0: the output is the input, delayed by the reported latency
        setParam (*p, "mix", 0.0f);
        AudioBuffer<float> x (2, 8192);
        Random r (3);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < x.getNumSamples(); ++i) x.setSample (ch, i, r.nextFloat() * 2 - 1);
        auto y = x;
        runThrough (*p, y, rate, 512, &ph);
        auto y2 = x;
        runThrough (*p, y2, rate, 512, &ph);
        const int lat = p->getLatencySamples();
        float maxErr = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = lat + 2000; i < x.getNumSamples(); ++i)
                maxErr = jmax (maxErr, std::abs (y2.getSample (ch, i) - x.getSample (ch, i - lat)));
        check (maxErr < 1.0e-5f, "MIX 0 = dry, aligned (latency " + String (lat) + ") @ " + String (rate) + "  err " + String (maxErr));

        // every factory preset, irregular block sizes: finite output, sane level
        setParam (*p, "mix", 1.0f);
        const auto& list = p->presets->getPresets();
        bool finite = true;
        float worst = 0.0f;
        for (int i = 0; i < (int) list.size(); ++i)
        {
            p->presets->load (i);
            auto b = loop;
            MidiBuffer midi;
            Random rb (i);
            for (int s = 0; s < b.getNumSamples();)
            {
                const int n = jmin (1 + rb.nextInt (700), b.getNumSamples() - s);
                AudioBuffer<float> chunk (b.getArrayOfWritePointers(), 2, s, n);
                ph.sample = s;
                p->processBlock (chunk, midi);
                s += n;
            }
            for (int ch = 0; ch < 2; ++ch)
                for (int k = 0; k < b.getNumSamples(); ++k)
                {
                    const float v = b.getSample (ch, k);
                    if (! std::isfinite (v)) finite = false;
                    worst = jmax (worst, std::abs (v));
                }
        }
        check (finite, "all " + String ((int) list.size()) + " presets: finite output, irregular blocks @ " + String (rate));
        check (worst < 4.0f, "all presets: peak below +12 dBFS (" + String (Decibels::gainToDecibels (worst), 1) + " dBFS)");
        p->releaseResources();
    }

    // random parameters, fast automation
    {
        auto p = makeProcessor();
        TestPlayHead ph;
        p->setPlayHead (&ph);
        p->setPlayConfigDetails (2, 2, sr, 256);
        p->prepareToPlay (sr, 256);
        Random r (99);
        bool finite = true;
        AudioBuffer<float> b (2, 256);
        MidiBuffer midi;
        for (int blk = 0; blk < 4000; ++blk)
        {
            if (blk % 7 == 0)
                for (auto* prm : p->getParameters())
                    if (prm != p->getBypassParameter()) prm->setValueNotifyingHost (r.nextFloat());
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) b.setSample (ch, i, (r.nextFloat() * 2 - 1) * 0.8f);
            ph.sample = blk * 256;
            p->processBlock (b, midi);
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 256; ++i) if (! std::isfinite (b.getSample (ch, i))) finite = false;
        }
        check (finite, "random automation for 21 s: no NaN / inf");
    }

    // bypass: bit-exact (after the crossfade), latency kept
    {
        auto p = makeProcessor();
        TestPlayHead ph;
        p->setPlayHead (&ph);
        p->setPlayConfigDetails (2, 2, sr, 512);
        p->prepareToPlay (sr, 512);
        setParam (*p, "bypass", 1.0f);
        AudioBuffer<float> x (2, 48000);
        Random r (5);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < x.getNumSamples(); ++i) x.setSample (ch, i, r.nextFloat() * 2 - 1);
        auto y = x;
        runThrough (*p, y, sr, 512, &ph);
        const int lat = p->getLatencySamples();
        float err = 0.0f;
        for (int ch = 0; ch < 2; ++ch) for (int i = 24000; i < x.getNumSamples(); ++i) err = jmax (err, std::abs (y.getSample (ch, i) - x.getSample (ch, i - lat)));
        check (err < 1.0e-6f, "bypass = input (delayed by latency), err " + String (err));
    }

    // mono and mono -> stereo
    {
        auto p = makeProcessor();
        AudioProcessor::BusesLayout mono, m2s;
        mono.inputBuses.add (AudioChannelSet::mono()); mono.outputBuses.add (AudioChannelSet::mono());
        m2s.inputBuses.add (AudioChannelSet::mono());  m2s.outputBuses.add (AudioChannelSet::stereo());
        check (p->setBusesLayout (mono), "mono > mono layout");
        TestPlayHead ph;
        p->setPlayHead (&ph);
        p->prepareToPlay (sr, 512);
        AudioBuffer<float> b (1, 4096); b.clear(); b.setSample (0, 100, 1.0f);
        runThrough (*p, b, sr, 512, &ph);
        bool fin = true; for (int i = 0; i < 4096; ++i) if (! std::isfinite (b.getSample (0, i))) fin = false;
        check (fin, "mono processing");
        p->releaseResources();
        check (p->setBusesLayout (m2s), "mono > stereo layout");
        p->prepareToPlay (sr, 512);
        AudioBuffer<float> s (2, 4096); s.clear();
        for (int i = 0; i < 4096; ++i) s.setSample (0, i, std::sin ((float) i * 0.05f) * 0.5f);
        runThrough (*p, s, sr, 512, &ph);
        check (s.getMagnitude (1, 0, 4096) > 0.01f, "mono > stereo: right channel carries signal");
    }

    // state: save, scramble, restore
    {
        auto p = makeProcessor();
        Random r (11);
        for (auto* prm : p->getParameters()) prm->setValueNotifyingHost (r.nextFloat());
        setParam (*p, "bypass", 0.0f);
        auto before = p->presets->captureCurrent();
        MemoryBlock mb;
        p->getStateInformation (mb);
        for (auto* prm : p->getParameters()) prm->setValueNotifyingHost (r.nextFloat());
        p->setStateInformation (mb.getData(), (int) mb.getSize());
        check (p->presets->sameValues (before, p->presets->captureCurrent()), "state save / restore (" + String (p->getParameters().size()) + " parameters)");
    }

    // CPU
    {
        auto p = makeProcessor();
        TestPlayHead ph;
        p->setPlayHead (&ph);
        p->setPlayConfigDetails (2, 2, sr, 512);
        p->prepareToPlay (sr, 512);
        const auto& list = p->presets->getPresets();
        p->presets->load ((int) list.size() - 1);
        auto b = makeDemoLoop (sr, 124.0, 4);
        const double t0 = Time::getMillisecondCounterHiRes();
        runThrough (*p, b, sr, 512, &ph);
        const double ms = Time::getMillisecondCounterHiRes() - t0;
        const double audioMs = b.getNumSamples() / sr * 1000.0;
        std::cout << "  CPU   " << String (100.0 * ms / audioMs, 2) << " % of one core (48 kHz stereo, preset \"" << list.back().name << "\")" << std::endl;
    }

    std::cout << (failures == 0 ? "ALL CHECKS PASSED" : String (failures) + " CHECK(S) FAILED") << std::endl;
}

//==============================================================================
void renderAll (const File& dir)
{
    const double sr = 48000.0;
    dir.createDirectory();
    auto loop = makeDemoLoop (sr);
    writeWav (dir.getChildFile ("00 dry.wav"), loop, sr);
    auto p = makeProcessor();
    TestPlayHead ph;
    p->setPlayHead (&ph);
    p->setPlayConfigDetails (2, 2, sr, 512);
    p->prepareToPlay (sr, 512);
    const auto& list = p->presets->getPresets();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        p->presets->load (i);
        p->reset();
        auto b = loop;
        runThrough (*p, b, sr, 512, &ph);
        // compensate the latency so the files line up with the dry one
        const int lat = p->getLatencySamples();
        if (lat > 0)
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = b.getWritePointer (ch);
                std::memmove (d, d + lat, sizeof (float) * (size_t) (b.getNumSamples() - lat));
                FloatVectorOperations::clear (d + b.getNumSamples() - lat, lat);
            }
        const auto name = String (i).paddedLeft ('0', 2) + " " + File::createLegalFileName (list[(size_t) i].name) + ".wav";
        writeWav (dir.getChildFile (name), b, sr);
        std::cout << "  " << name << "   peak " << String (Decibels::gainToDecibels (b.getMagnitude (0, b.getNumSamples())), 1) << " dBFS" << std::endl;
    }
}

//==============================================================================
std::unique_ptr<nob::ui::LiteEditor> openEditor (nob::LiteProcessor& p, bool dark, const String& preset)
{
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    if (preset.isNotEmpty())
        if (const int i = findPreset (p, preset); i >= 0) p.presets->load (i);
    // run some audio so meters and screens show something alive
    static TestPlayHead ph;
    p.setPlayHead (&ph);
    auto loop = makeDemoLoop (48000.0, 124.0, 1);
    AudioBuffer<float> part (2, 48000 + 7300);
    for (int ch = 0; ch < 2; ++ch) part.copyFrom (ch, 0, loop, ch, 0, part.getNumSamples());
    runThrough (p, part, 48000.0, 512, &ph);

    std::unique_ptr<nob::ui::LiteEditor> ed (dynamic_cast<nob::ui::LiteEditor*> (p.createEditorAndMakeActive()));
    ed->setTheme (dark);
    // let timers tick a few times so screens catch up
    for (int k = 0; k < 6; ++k)
    {
        runThrough (p, part, 48000.0, 512, &ph);
        MessageManager::getInstance()->runDispatchLoopUntil (40);
    }
    return ed;
}

} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    StringArray args;
    for (int i = 1; i < argc; ++i) args.add (String::fromUTF8 (argv[i]));
    const auto cmd = args[0];

    if (cmd == "render" && args.size() > 1)
    {
        renderAll (File::getCurrentWorkingDirectory().getChildFile (args[1]));
        return 0;
    }
    if ((cmd == "snapshot" || cmd == "card") && args.size() > 1)
    {
        auto p = makeProcessor();
        auto ed = openEditor (*p, args[2] != "lux", args[3]);
        const auto out = File::getCurrentWorkingDirectory().getChildFile (args[1]);
        if (cmd == "card")
        {
            const auto f = ed->exportShareCard (out);
            std::cout << "card: " << f.getFullPathName() << std::endl;
        }
        else
        {
            const float scale = args.size() > 4 ? (float) args[4].getDoubleValue() : 1.0f;
            // marketing pictures: no "dev mode / ADMIN" account chip
            std::function<void (Component&)> hideAccount = [&] (Component& c)
            {
                for (auto* ch : c.getChildren())
                {
                    if (ch->getName() == "account") ch->setVisible (false);
                    else hideAccount (*ch);
                }
            };
            hideAccount (*ed);
            auto img = ed->snapshot (scale);
            out.deleteFile();
            FileOutputStream os (out);
            PNGImageFormat().writeImageToStream (img, os);
            std::cout << "snapshot: " << out.getFullPathName() << std::endl;
        }
        ed.reset();
        return 0;
    }
    runChecks();
    return failures == 0 ? 0 : 1;
}
