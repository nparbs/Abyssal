#pragma once

#include "Common.h"
#include "Chain.h"
#include "Pedals.h"
#include "AmpModel.h"
#include "NamAmp.h"
#include "Cabinet.h"
#include "PostFX.h"
#include "Tuner.h"
#include "ModFX.h"

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
    bool            namCalibrate = false;   // match the capture's recorded input level...
    float           namInterfaceDbu = 12;   // ...to the interface's level at 0 dBFS
    NamToneParams   namTone;
    CabParams       cab;
    ParamEQParams   peq;
    ModParams       chorus, flanger;
    DelayParams     delay;
    ReverbParams    reverb;
    WahParams       wah;
    WhammyParams    whammy;
    PhaserParams    phaser;
    TremoloParams   tremolo;
    DoublerParams   doubler;
    double          bpm = 120.0;
    bool            mute = false;           // tuner mute
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

    /** Latency of the chain as currently configured: the amp (or the NAM resampler) plus drop tune
        when it's shifting. Updated every block; the processor reports it to the host. */
    int getLatencySamples() const noexcept { return latency.load(); }

    /** The input gain applied to a NAM capture by input calibration (0 when off or unknown). */
    static float namCalibrationDb (const EngineParams& p, const NamAmp& nam) noexcept;

    /** The tuner reads the input from here while it's enabled. */
    TunerTap& getTunerTap() noexcept { return tunerTap; }
    std::atomic<bool> tunerEnabled { false };

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
    int computeLatency (const EngineParams& p) const noexcept;
    void processAmpBlock (int n, const EngineParams& p);
    void processPedalMono (int id, int n, const EngineParams& p);
    void processPedalStereo (int id, int n, const EngineParams& p);
    void resetPedal (int id);

    template <typename Fn, typename ResetFn>
    void runRamp (BypassRamp& ramp, bool on, int n, float* a, float* b, Fn&& processFn, ResetFn&& resetFn);

    double sr = 48000.0;
    int maxBlock = 512;

    juce::AudioBuffer<float> mono, dry, namScratch, work, fxTmp, bypassDry, gateKey;
    juce::SmoothedValue<float> inGain, outGain, muteGain;

    NoiseGate  gate[2];
    Octaver    octaver[2];
    DropTune   drop[2];
    Compressor comp[2];
    Overdrive  od[2];
    GraphicEQ  geq[2];
    Wah        wah[2];
    Whammy     whammy[2];
    Phaser     phaser[2];
    Tremolo    tremolo[2];
    Doubler    doubler;
    AmpModel   amp;
    NamAmp     nam;
    NamTone    namTone;
    BypassRamp namToneRamp;
    Cabinet    cab;
    ParamEQ    peq;
    ModDelay   chorus, flanger;
    StereoDelay delay;
    Reverb     reverb;
    TunerTap   tunerTap;

    BypassRamp ramps[numFx], peqRamp, bypassRamp;
    juce::SmoothedValue<float> namMix; // crossfade between DSP amp and NAM capture

    std::atomic<juce::uint64> packedOrder { ChainOrder::defaultOrder().pack() };
    juce::uint64 activeOrder = 0;

    // Delays the dry signal by the plugin latency so host bypass stays phase-aligned.
    static constexpr int latencyLineSize = 8192;
    std::vector<float> latencyLine[2];
    int latencyWrite = 0;
    std::atomic<int> latency { 0 };
};

} // namespace abyssal
