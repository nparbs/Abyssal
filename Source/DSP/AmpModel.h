#pragma once

#include "Common.h"

namespace abyssal
{

struct AmpParams
{
    int   channel = 2; // 0 = Clean, 1 = Crunch, 2 = Lead
    float gain = 6, bass = 5, mid = 5, treble = 6, presence = 5, depth = 4, master = 5;
    bool  bright = false, tight = true;

    bool operator== (const AmpParams&) const = default;
};

/** Physically-inspired high-gain amp: cascaded asymmetric triode stages with interstage
    coupling/Miller filtering, a tone stack, and a sagging power stage with depth/presence. */
class AmpModel
{
public:
    static constexpr int maxStages = 4;

    struct Voicing
    {
        int   stages;
        float minGainDb, maxGainDb;      // first-stage gain range swept by the Gain knob
        float stageGain[maxStages];      // fixed gain into each later stage
        float bias[maxStages];           // operating-point offset (adds even harmonics)
        float hpHz[maxStages];           // coupling-cap highpass before each stage
        float lpHz[maxStages];           // Miller-cap lowpass after each stage
        float looseHz, tightHz;          // input highpass with the Tight switch off / on
        float preMidHz, preMidDb;        // mid emphasis before clipping (articulation)
        float fizzHz;                    // post-preamp lowpass
        float bassHz, midHz, trebleHz;   // tone stack centres
        float sag, powerDrive, outTrimDb;
    };

    static const Voicing& getVoicing (int channel);

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* data, int n, const AmpParams& p);
    int  getLatencySamples() const noexcept { return latency; }

private:
    void updateFilters (const AmpParams& p);

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    double sr = 48000, osr = 192000;
    int latency = 0;

    AmpParams current;
    bool needsUpdate = true;

    Biquad inputHp, brightShelf, preMid;
    Biquad stageHp[maxStages], stageLp[maxStages];
    Biquad fizzLp, fizzLp2;
    Biquad bass, mid, treble;
    Biquad depth, presence;
    Biquad dcBlock;

    juce::SmoothedValue<float> stage1Gain, powerGain, outGain;
    float sagEnv = 0, sagAttack = 0, sagRelease = 0;
};

} // namespace abyssal
