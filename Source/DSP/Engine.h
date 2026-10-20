#pragma once

#include "Common.h"
#include "Chain.h"
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
    int   inputMode = 0;      // 0 = left, 1 = right, 2 = mono sum
    bool  monoOut = false;
    bool  bypass = false;

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

/** The complete signal chain. Pedals before the amp run in mono; after the cab everything is
    stereo. Mono pedals keep a second instance for the right channel when placed post-amp, and
    stereo effects placed pre-amp are summed back to mono. */
class Engine
{
public:
    Engine();

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const EngineParams& p);

    /** Message-thread safe: takes effect at the next block. */
    void setChainOrder (const ChainOrder& order) noexcept { packedOrder = order.pack(); }

    int getLatencySamples() const noexcept { return amp.getLatencySamples(); }

    NamAmp&  getNam() noexcept     { return nam; }
    Cabinet& getCabinet() noexcept { return cab; }
    const NamAmp&  getNamConst() const noexcept     { return nam; }
    const Cabinet& getCabinetConst() const noexcept { return cab; }

    /** Peak meters (linear) read by the UI. */
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f }, gateGainReduction { 0.0f };
    /** Output peak of the most recent block (audio thread only). */
    float lastBlockPeak = 0.0f;

private:
    void processChunk (float* const* channels, int numChannels, int n, const EngineParams& p);
    void processAmpBlock (int n, const EngineParams& p);
    void processPedalMono (int id, int n, const EngineParams& p);
    void processPedalStereo (int id, int n, const EngineParams& p);
    void resetPedal (int id);

    template <typename Fn, typename ResetFn>
    void runRamp (BypassRamp& ramp, bool on, int n, float* a, float* b, Fn&& processFn, ResetFn&& resetFn);

    double sr = 48000.0;
    int maxBlock = 512;

    juce::AudioBuffer<float> mono, dry, namScratch, work, fxTmp, bypassDry;
    juce::SmoothedValue<float> inGain, outGain;

    NoiseGate  gate[2];
    Octaver    octaver[2];
    DropTune   drop[2];
    Compressor comp[2];
    Overdrive  od[2];
    GraphicEQ  geq[2];
    AmpModel   amp;
    NamAmp     nam;
    Cabinet    cab;
    ParamEQ    peq;
    ModDelay   chorus, flanger;
    StereoDelay delay;
    Reverb     reverb;

    BypassRamp ramps[numFx], peqRamp, bypassRamp;
    juce::SmoothedValue<float> namMix; // crossfade between DSP amp and NAM capture

    std::atomic<juce::uint64> packedOrder { ChainOrder::defaultOrder().pack() };
    juce::uint64 activeOrder = 0;

    // Delays the dry signal by the plugin latency so host bypass stays phase-aligned.
    std::vector<float> latencyLine[2];
    int latencyWrite = 0;
};

} // namespace abyssal
