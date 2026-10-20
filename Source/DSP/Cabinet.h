#pragma once

#include "Common.h"

namespace abyssal
{

struct CabSlotParams { bool on = true; int source = 0; float levelDb = 0, pan = 0, mic = 0.3f; };
struct CabParams     { bool on = true; CabSlotParams slot[2]; };

/** Two convolution slots (built-in synthesized cabs or user .wav IRs), each with a mic-position
    tone control, level and pan. Converts the mono amp signal to stereo. */
class Cabinet
{
public:
    static constexpr int numBuiltIns = 3;
    static constexpr int userSource  = numBuiltIns;
    static juce::StringArray getSourceNames();

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (const float* mono, float* outL, float* outR, int n, const CabParams& p);

    /** Message-thread API. */
    void setSource (int slot, int source);
    juce::String loadUserIR (int slot, const juce::File& file);
    juce::File getUserIR (int slot) const { return userFile[slot]; }
    int getLoadedSource (int slot) const { return loadedSource[slot]; }

    /** Synthesizes one of the built-in cabinet impulse responses. */
    static juce::AudioBuffer<float> makeBuiltInIR (int type, double sampleRate);

private:
    void loadSource (int slot);

    double sr = 48000;
    int maxBlock = 512;

    juce::dsp::Convolution conv[2];
    Biquad micPresence[2], micShelf[2];
    float cachedMic[2] { -1, -1 };
    juce::AudioBuffer<float> scratch;
    juce::SmoothedValue<float> gainL[2], gainR[2];

    std::atomic<int> loadedSource[2] { { -1 }, { -1 } };
    int wantedSource[2] { 0, 0 };
    juce::File userFile[2];
};

} // namespace abyssal
