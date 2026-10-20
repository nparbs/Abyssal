#pragma once

#include "Common.h"
#include "Pedals.h"
#include "AmpModel.h"
#include "NamAmp.h"
#include "Cabinet.h"
#include "PostFX.h"

namespace abyssal
{

/** Plain snapshot of every parameter, filled once per block. Keeps the DSP independent of the APVTS. */
struct EngineParams
{
    float inGainDb = 0, outGainDb = 0;

    GateParams      gate;
    OctaverParams   octaver;
    DropTuneParams  drop;
    CompParams      comp;
    OverdriveParams od;
    GraphicEQParams geq;
    AmpParams       amp;
    bool            useNam = false;
    float           namInDb = 0, namOutDb = 0;
    CabParams       cab;
    ParamEQParams   peq;
    ModParams       chorus, flanger;
    DelayParams     delay;
    ReverbParams    reverb;
    double          bpm = 120.0;
};

/** The complete signal chain. Mono up to the cabinet, stereo afterwards. */
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const EngineParams& p);

    int getLatencySamples() const noexcept { return amp.getLatencySamples(); }

    NamAmp&  getNam() noexcept     { return nam; }
    Cabinet& getCabinet() noexcept { return cab; }
    const NamAmp&  getNamConst() const noexcept     { return nam; }
    const Cabinet& getCabinetConst() const noexcept { return cab; }

    /** Peak meters (linear) read by the UI. */
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gateGainReduction { 0.0f };

private:
    double sr = 48000.0;
    int maxBlock = 512;

    juce::AudioBuffer<float> mono, dryScratch, namScratch, work, stereoDry;
    juce::SmoothedValue<float> inGain, outGain;

    NoiseGate  gate;
    Octaver    octaver;
    DropTune   drop;
    Compressor comp;
    Overdrive  od;
    GraphicEQ  geq;
    AmpModel   amp;
    NamAmp     nam;
    Cabinet    cab;
    ParamEQ    peq;
    ModDelay   chorus, flanger;
    StereoDelay delay;
    Reverb     reverb;

    BypassRamp gateRamp, octRamp, dropRamp, compRamp, odRamp, geqRamp,
               peqRamp, chorusRamp, flangerRamp, delayRamp, reverbRamp;
    juce::SmoothedValue<float> namMix; // crossfade between DSP amp and NAM capture

    template <typename Module, typename Params>
    void runMono (Module& m, BypassRamp& ramp, bool on, const Params& params, int n);

    template <typename Fn, typename ResetFn>
    void runStereo (BypassRamp& ramp, bool on, int n, Fn&& process, ResetFn&& resetFn);

    void processChunk (float* const* channels, int numChannels, int n, const EngineParams& p);
};

} // namespace abyssal
