#include "Engine.h"

namespace abyssal
{

Engine::Engine() : chorus (ModDelay::Mode::Chorus), flanger (ModDelay::Mode::Flanger) {}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = std::max (32, maxBlockSize);

    mono.setSize (1, maxBlock);
    dryScratch.setSize (1, maxBlock);
    namScratch.setSize (1, maxBlock);
    work.setSize (2, maxBlock);
    stereoDry.setSize (2, maxBlock);

    inGain.reset (sr, 0.03);
    outGain.reset (sr, 0.03);
    namMix.reset (sr, 0.05);

    gate.prepare (sr, maxBlock);
    octaver.prepare (sr, maxBlock);
    drop.prepare (sr, maxBlock);
    comp.prepare (sr, maxBlock);
    od.prepare (sr, maxBlock);
    geq.prepare (sr, maxBlock);
    amp.prepare (sr, maxBlock);
    nam.prepare (sr, maxBlock);
    cab.prepare (sr, maxBlock);
    peq.prepare (sr, maxBlock);
    chorus.prepare (sr, maxBlock);
    flanger.prepare (sr, maxBlock);
    delay.prepare (sr, maxBlock);
    reverb.prepare (sr, maxBlock);

    for (auto* r : { &gateRamp, &octRamp, &dropRamp, &compRamp, &odRamp, &geqRamp,
                     &peqRamp, &chorusRamp, &flangerRamp, &delayRamp, &reverbRamp })
        r->prepare (sr, false);
}

void Engine::reset()
{
    gate.reset(); octaver.reset(); drop.reset(); comp.reset(); od.reset(); geq.reset();
    amp.reset(); nam.reset(); cab.reset(); peq.reset();
    chorus.reset(); flanger.reset(); delay.reset(); reverb.reset();
}

template <typename Module, typename Params>
void Engine::runMono (Module& m, BypassRamp& ramp, bool on, const Params& params, int n)
{
    ramp.setOn (on);
    if (! ramp.isActive()) { ramp.wasActive = false; return; }
    if (! ramp.wasActive)  { m.reset(); ramp.wasActive = true; }

    float* d = mono.getWritePointer (0);
    const bool fading = ramp.isFading();
    if (fading)
        std::copy (d, d + n, dryScratch.getWritePointer (0));

    m.process (d, n, params);

    if (fading)
        ramp.blend (dryScratch.getReadPointer (0), d, n);
}

template <typename Fn, typename ResetFn>
void Engine::runStereo (BypassRamp& ramp, bool on, int n, Fn&& processFn, ResetFn&& resetFn)
{
    ramp.setOn (on);
    if (! ramp.isActive()) { ramp.wasActive = false; return; }
    if (! ramp.wasActive)  { resetFn(); ramp.wasActive = true; }

    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    const bool fading = ramp.isFading();
    if (fading)
    {
        std::copy (l, l + n, stereoDry.getWritePointer (0));
        std::copy (r, r + n, stereoDry.getWritePointer (1));
    }

    processFn (l, r, n);

    if (fading)
        ramp.blend (stereoDry.getReadPointer (0), stereoDry.getReadPointer (1), l, r, n);
}

void Engine::process (juce::AudioBuffer<float>& buffer, const EngineParams& p)
{
    const int total = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numCh == 0 || total == 0)
        return;

    float* chans[2] = { buffer.getWritePointer (0), buffer.getWritePointer (numCh > 1 ? 1 : 0) };

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = std::min (maxBlock, total - start);
        float* sub[2] = { chans[0] + start, chans[1] + start };
        processChunk (sub, numCh, n, p);
    }
}

void Engine::processChunk (float* const* io, int numChannels, int n, const EngineParams& p)
{
    float* m = mono.getWritePointer (0);

    // ---- input (guitar is taken from the first input channel)
    inGain.setTargetValue (dbToGain (p.inGainDb));
    float inPk = 0;
    for (int i = 0; i < n; ++i)
    {
        m[i] = io[0][i] * inGain.getNextValue();
        inPk = std::max (inPk, std::abs (m[i]));
    }
    inputPeak = std::max (inPk, inputPeak.load() * 0.9f);

    // ---- pre-amp pedals
    runMono (gate,    gateRamp, p.gate.on,    p.gate,    n);
    gateGainReduction = gateRamp.isActive() ? gate.getGainReductionDb() : 0.0f;
    runMono (octaver, octRamp,  p.octaver.on, p.octaver, n);
    runMono (drop,    dropRamp, p.drop.on,    p.drop,    n);
    runMono (comp,    compRamp, p.comp.on,    p.comp,    n);
    runMono (od,      odRamp,   p.od.on,      p.od,      n);
    runMono (geq,     geqRamp,  p.geq.on,     p.geq,     n);

    // ---- amp: DSP model or NAM capture, crossfaded when switching
    namMix.setTargetValue (p.useNam && nam.hasModel() ? 1.0f : 0.0f);
    if (! namMix.isSmoothing() && namMix.getTargetValue() > 0.5f)
    {
        std::copy (m, m + n, dryScratch.getWritePointer (0));
        if (! nam.process (m, n, p.namInDb, p.namOutDb))
        {
            std::copy (dryScratch.getReadPointer (0), dryScratch.getReadPointer (0) + n, m);
            amp.process (m, n, p.amp);
        }
    }
    else if (! namMix.isSmoothing())
    {
        amp.process (m, n, p.amp);
    }
    else
    {
        float* nm = namScratch.getWritePointer (0);
        std::copy (m, m + n, nm);
        amp.process (m, n, p.amp);
        if (! nam.process (nm, n, p.namInDb, p.namOutDb))
            std::copy (m, m + n, nm);
        for (int i = 0; i < n; ++i)
        {
            const float k = namMix.getNextValue();
            m[i] += k * (nm[i] - m[i]);
        }
    }

    // ---- cabinet (mono -> stereo)
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    cab.process (m, l, r, n, p.cab);

    // ---- post FX
    runStereo (peqRamp, p.peq.on, n,
               [&] (float* a, float* b, int k) { peq.process (a, b, k, p.peq); }, [&] { peq.reset(); });
    runStereo (chorusRamp, p.chorus.on, n,
               [&] (float* a, float* b, int k) { chorus.process (a, b, k, p.chorus); }, [&] { chorus.reset(); });
    runStereo (flangerRamp, p.flanger.on, n,
               [&] (float* a, float* b, int k) { flanger.process (a, b, k, p.flanger); }, [&] { flanger.reset(); });
    runStereo (delayRamp, p.delay.on, n,
               [&] (float* a, float* b, int k) { delay.process (a, b, k, p.delay, p.bpm); }, [&] { delay.reset(); });
    runStereo (reverbRamp, p.reverb.on, n,
               [&] (float* a, float* b, int k) { reverb.process (a, b, k, p.reverb); }, [&] { reverb.reset(); });

    // ---- output
    outGain.setTargetValue (dbToGain (p.outGainDb));
    float outPk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        l[i] *= g;
        r[i] *= g;
        outPk = std::max (outPk, std::max (std::abs (l[i]), std::abs (r[i])));
    }

    // Guard against a blown-up filter ever reaching the speakers.
    if (! std::isfinite (outPk))
    {
        reset();
        std::fill (l, l + n, 0.0f);
        std::fill (r, r + n, 0.0f);
        outPk = 0;
    }
    outputPeak = std::max (outPk, outputPeak.load() * 0.9f);

    if (numChannels > 1)
    {
        std::copy (l, l + n, io[0]);
        std::copy (r, r + n, io[1]);
    }
    else
    {
        for (int i = 0; i < n; ++i)
            io[0][i] = 0.5f * (l[i] + r[i]);
    }
}

} // namespace abyssal
