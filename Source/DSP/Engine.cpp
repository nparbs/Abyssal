#include "Engine.h"

namespace abyssal
{

Engine::Engine() : chorus (ModDelay::Mode::Chorus), flanger (ModDelay::Mode::Flanger) {}

void Engine::prepare (double sampleRate, int maxBlockSize)
{
    sr = sampleRate;
    maxBlock = std::max (32, maxBlockSize);

    mono.setSize (1, maxBlock);
    dry.setSize (2, maxBlock);
    namScratch.setSize (1, maxBlock);
    work.setSize (2, maxBlock);
    fxTmp.setSize (2, maxBlock);
    bypassDry.setSize (2, maxBlock);

    inGain.reset (sr, 0.03);
    outGain.reset (sr, 0.03);
    namMix.reset (sr, 0.05);

    for (int c = 0; c < 2; ++c)
    {
        gate[c].prepare (sr, maxBlock);
        octaver[c].prepare (sr, maxBlock);
        drop[c].prepare (sr, maxBlock);
        comp[c].prepare (sr, maxBlock);
        od[c].prepare (sr, maxBlock);
        geq[c].prepare (sr, maxBlock);
        latencyLine[c].assign (1024, 0.0f);
    }
    latencyWrite = 0;

    amp.prepare (sr, maxBlock);
    nam.prepare (sr, maxBlock);
    cab.prepare (sr, maxBlock);
    peq.prepare (sr, maxBlock);
    chorus.prepare (sr, maxBlock);
    flanger.prepare (sr, maxBlock);
    delay.prepare (sr, maxBlock);
    reverb.prepare (sr, maxBlock);

    for (auto& r : ramps) r.prepare (sr, false);
    peqRamp.prepare (sr, false);
    bypassRamp.prepare (sr, true);
    activeOrder = packedOrder.load();
}

void Engine::reset()
{
    for (int id = 0; id < numFx; ++id) resetPedal (id);
    amp.reset(); nam.reset(); cab.reset(); peq.reset();
}

void Engine::resetPedal (int id)
{
    switch (id)
    {
        case fxGate:      gate[0].reset(); gate[1].reset(); break;
        case fxOctaver:   octaver[0].reset(); octaver[1].reset(); break;
        case fxDrop:      drop[0].reset(); drop[1].reset(); break;
        case fxComp:      comp[0].reset(); comp[1].reset(); break;
        case fxOverdrive: od[0].reset(); od[1].reset(); break;
        case fxGEQ:       geq[0].reset(); geq[1].reset(); break;
        case fxChorus:    chorus.reset(); break;
        case fxFlanger:   flanger.reset(); break;
        case fxDelay:     delay.reset(); break;
        case fxReverb:    reverb.reset(); break;
        default: break;
    }
}

/** Runs a module behind a click-free bypass. `b` is null for mono. */
template <typename Fn, typename ResetFn>
void Engine::runRamp (BypassRamp& ramp, bool on, int n, float* a, float* b, Fn&& processFn, ResetFn&& resetFn)
{
    ramp.setOn (on);
    if (! ramp.isActive()) { ramp.wasActive = false; return; }
    if (! ramp.wasActive)  { resetFn(); ramp.wasActive = true; }

    const bool fading = ramp.isFading();
    float* d0 = dry.getWritePointer (0);
    float* d1 = dry.getWritePointer (1);
    if (fading)
    {
        std::copy (a, a + n, d0);
        if (b != nullptr) std::copy (b, b + n, d1);
    }

    processFn();

    if (fading)
    {
        if (b != nullptr) ramp.blend (d0, d1, a, b, n);
        else              ramp.blend (d0, a, n);
    }
}

static bool pedalOn (int id, const EngineParams& p)
{
    switch (id)
    {
        case fxGate:      return p.gate.on;
        case fxOctaver:   return p.octaver.on;
        case fxDrop:      return p.drop.on;
        case fxComp:      return p.comp.on;
        case fxOverdrive: return p.od.on;
        case fxGEQ:       return p.geq.on;
        case fxChorus:    return p.chorus.on;
        case fxFlanger:   return p.flanger.on;
        case fxDelay:     return p.delay.on;
        case fxReverb:    return p.reverb.on;
        default:          return false;
    }
}

/** Pre-amp position: everything works on the mono buffer. */
void Engine::processPedalMono (int id, int n, const EngineParams& p)
{
    float* m = mono.getWritePointer (0);
    auto reset = [this, id] { resetPedal (id); };

    // Stereo effects in a mono slot: duplicate, process, fold back down.
    auto viaStereo = [&] (auto&& stereoFn)
    {
        float* l = fxTmp.getWritePointer (0);
        float* r = fxTmp.getWritePointer (1);
        std::copy (m, m + n, l);
        std::copy (m, m + n, r);
        stereoFn (l, r);
        for (int i = 0; i < n; ++i) m[i] = 0.5f * (l[i] + r[i]);
    };

    auto& ramp = ramps[id];
    const bool on = pedalOn (id, p);
    switch (id)
    {
        case fxGate:      runRamp (ramp, on, n, m, nullptr, [&] { gate[0].process (m, n, p.gate); }, reset); break;
        case fxOctaver:   runRamp (ramp, on, n, m, nullptr, [&] { octaver[0].process (m, n, p.octaver); }, reset); break;
        case fxDrop:      runRamp (ramp, on, n, m, nullptr, [&] { drop[0].process (m, n, p.drop); }, reset); break;
        case fxComp:      runRamp (ramp, on, n, m, nullptr, [&] { comp[0].process (m, n, p.comp); }, reset); break;
        case fxOverdrive: runRamp (ramp, on, n, m, nullptr, [&] { od[0].process (m, n, p.od); }, reset); break;
        case fxGEQ:       runRamp (ramp, on, n, m, nullptr, [&] { geq[0].process (m, n, p.geq); }, reset); break;
        case fxChorus:    runRamp (ramp, on, n, m, nullptr, [&] { viaStereo ([&] (float* l, float* r) { chorus.process (l, r, n, p.chorus); }); }, reset); break;
        case fxFlanger:   runRamp (ramp, on, n, m, nullptr, [&] { viaStereo ([&] (float* l, float* r) { flanger.process (l, r, n, p.flanger); }); }, reset); break;
        case fxDelay:     runRamp (ramp, on, n, m, nullptr, [&] { viaStereo ([&] (float* l, float* r) { delay.process (l, r, n, p.delay, p.bpm); }); }, reset); break;
        case fxReverb:    runRamp (ramp, on, n, m, nullptr, [&] { viaStereo ([&] (float* l, float* r) { reverb.process (l, r, n, p.reverb); }); }, reset); break;
        default: break;
    }
}

/** Post-amp position: everything works on the stereo work buffer. */
void Engine::processPedalStereo (int id, int n, const EngineParams& p)
{
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    auto reset = [this, id] { resetPedal (id); };

    auto& ramp = ramps[id];
    const bool on = pedalOn (id, p);
    switch (id)
    {
        case fxGate:      runRamp (ramp, on, n, l, r, [&] { gate[0].process (l, n, p.gate); gate[1].process (r, n, p.gate); }, reset); break;
        case fxOctaver:   runRamp (ramp, on, n, l, r, [&] { octaver[0].process (l, n, p.octaver); octaver[1].process (r, n, p.octaver); }, reset); break;
        case fxDrop:      runRamp (ramp, on, n, l, r, [&] { drop[0].process (l, n, p.drop); drop[1].process (r, n, p.drop); }, reset); break;
        case fxComp:      runRamp (ramp, on, n, l, r, [&] { comp[0].process (l, n, p.comp); comp[1].process (r, n, p.comp); }, reset); break;
        case fxOverdrive: runRamp (ramp, on, n, l, r, [&] { od[0].process (l, n, p.od); od[1].process (r, n, p.od); }, reset); break;
        case fxGEQ:       runRamp (ramp, on, n, l, r, [&] { geq[0].process (l, n, p.geq); geq[1].process (r, n, p.geq); }, reset); break;
        case fxChorus:    runRamp (ramp, on, n, l, r, [&] { chorus.process (l, r, n, p.chorus); }, reset); break;
        case fxFlanger:   runRamp (ramp, on, n, l, r, [&] { flanger.process (l, r, n, p.flanger); }, reset); break;
        case fxDelay:     runRamp (ramp, on, n, l, r, [&] { delay.process (l, r, n, p.delay, p.bpm); }, reset); break;
        case fxReverb:    runRamp (ramp, on, n, l, r, [&] { reverb.process (l, r, n, p.reverb); }, reset); break;
        default: break;
    }
}

void Engine::processAmpBlock (int n, const EngineParams& p)
{
    float* m = mono.getWritePointer (0);

    // ---- amp: DSP model or NAM capture, crossfaded when switching
    namMix.setTargetValue (p.useNam && nam.hasModel() ? 1.0f : 0.0f);
    if (! namMix.isSmoothing() && namMix.getTargetValue() > 0.5f)
    {
        float* d = dry.getWritePointer (0);
        std::copy (m, m + n, d);
        if (! nam.process (m, n, p.namInDb, p.namOutDb))
        {
            std::copy (d, d + n, m);
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

    // ---- cabinet (mono -> stereo), then the post EQ
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    cab.process (m, l, r, n, p.cab);
    runRamp (peqRamp, p.peq.on, n, l, r, [&] { peq.process (l, r, n, p.peq); }, [&] { peq.reset(); });
}

void Engine::process (juce::AudioBuffer<float>& buffer, const EngineParams& p)
{
    const int total = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    lastBlockPeak = 0.0f;
    if (numCh == 0 || total == 0)
        return;

    // Pick up a new pedal order; pedals restart cleanly when the chain is rearranged.
    const auto order = packedOrder.load();
    if (order != activeOrder)
    {
        activeOrder = order;
        for (int id = 0; id < numFx; ++id) resetPedal (id);
    }

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
    float* l = work.getWritePointer (0);
    float* r = work.getWritePointer (1);
    const float* inL = io[0];
    const float* inR = numChannels > 1 ? io[1] : io[0];

    // ---- latency-aligned dry copy for host bypass
    {
        const int lat = juce::jlimit (0, 1000, getLatencySamples());
        const int size = (int) latencyLine[0].size();
        float* bl = bypassDry.getWritePointer (0);
        float* br = bypassDry.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            latencyLine[0][(size_t) latencyWrite] = inL[i];
            latencyLine[1][(size_t) latencyWrite] = inR[i];
            const int rd = (latencyWrite - lat + size) % size;
            bl[i] = latencyLine[0][(size_t) rd];
            br[i] = latencyLine[1][(size_t) rd];
            latencyWrite = (latencyWrite + 1) % size;
        }
    }

    bypassRamp.setOn (! p.bypass);
    if (! bypassRamp.isActive())
    {
        bypassRamp.wasActive = false;
        std::copy (bypassDry.getReadPointer (0), bypassDry.getReadPointer (0) + n, io[0]);
        if (numChannels > 1)
            std::copy (bypassDry.getReadPointer (1), bypassDry.getReadPointer (1) + n, io[1]);
        inputPeak = inputPeak.load() * 0.9f;
        outputPeak = outputPeak.load() * 0.9f;
        return;
    }
    if (! bypassRamp.wasActive) { reset(); bypassRamp.wasActive = true; }

    // ---- input select + gain
    inGain.setTargetValue (dbToGain (p.inGainDb));
    float inPk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float x = p.inputMode == 1 ? inR[i] : p.inputMode == 2 ? 0.5f * (inL[i] + inR[i]) : inL[i];
        m[i] = x * inGain.getNextValue();
        inPk = std::max (inPk, std::abs (m[i]));
    }
    inputPeak = std::max (inPk, inputPeak.load() * 0.9f);

    // ---- the chain, in the user's order
    const auto order = ChainOrder::unpack (activeOrder);
    bool postAmp = false;
    for (int id : order.slots)
    {
        if (id == fxAmpBlock) { processAmpBlock (n, p); postAmp = true; }
        else if (postAmp)     processPedalStereo (id, n, p);
        else                  processPedalMono (id, n, p);
    }
    gateGainReduction = ramps[fxGate].isActive() ? gate[0].getGainReductionDb() : 0.0f;

    // ---- output
    outGain.setTargetValue (dbToGain (p.outGainDb));
    float outPk = 0;
    for (int i = 0; i < n; ++i)
    {
        const float g = outGain.getNextValue();
        l[i] *= g;
        r[i] *= g;
        if (p.monoOut)
            l[i] = r[i] = 0.5f * (l[i] + r[i]);
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
    lastBlockPeak = std::max (lastBlockPeak, outPk);
    outputPeak = std::max (outPk, outputPeak.load() * 0.9f);

    // Host bypass crossfade against the latency-aligned dry signal.
    if (bypassRamp.isFading())
        bypassRamp.blend (bypassDry.getReadPointer (0), bypassDry.getReadPointer (1), l, r, n);

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
