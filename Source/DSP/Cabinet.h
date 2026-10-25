#pragma once

#include "Common.h"

namespace abyssal
{

struct CabSlotParams
{
    bool on = true;
    int source = 0;
    float levelDb = 0, pan = 0, mic = 0.3f;
    int micType = 0;        // 0 = dynamic, 1 = ribbon, 2 = condenser
    float distance = 0;     // 0 = on the grille, 1 = about 50 cm back
    bool invert = false;
};

struct CabParams
{
    bool on = true;
    CabSlotParams slot[2];
    bool align = true;      // line the two slots' IRs (and mic distances) up in time
};

/** Two convolution slots (built-in synthesized cabs or user IRs), each with a mic type, position and
    distance, level, pan and polarity. Converts the mono amp signal to stereo. */
class Cabinet
{
public:
    static constexpr int numBuiltIns = 6;
    static constexpr int userSource  = numBuiltIns;
    static juce::StringArray getSourceNames();
    static juce::StringArray getMicNames();

    /** Longest mic distance, in metres. */
    static constexpr double maxDistanceMetres = 0.5;

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (const float* mono, float* outL, float* outR, int n, const CabParams& p);

    /** Message-thread API. */
    void setSource (int slot, int source);
    juce::String loadUserIR (int slot, const juce::File& file);
    juce::File getUserIR (int slot) const { return userFile[slot]; }
    int getLoadedSource (int slot) const { return loadedSource[slot]; }
    /** Alignment changes how the IRs are trimmed, so both slots reload. */
    void setAlign (bool shouldAlign);
    bool getAlign() const noexcept { return align; }

    /** Synthesizes one of the built-in cabinet impulse responses. */
    static juce::AudioBuffer<float> makeBuiltInIR (int type, double sampleRate);

    /** Index of an IR's first significant sample (where it reaches -20 dB of its peak). */
    static int findOnset (const juce::AudioBuffer<float>& ir);

    /** Samples cut from the front of each slot's IR when it was loaded (for tests). */
    int getTrim (int slot) const noexcept { return trimmed[slot]; }

private:
    void loadSource (int slot);
    void reloadBoth();
    void updateMicFilters (int slot, const CabSlotParams& sp);

    double sr = 48000;
    int maxBlock = 512;

    juce::dsp::Convolution conv[2];
    Biquad micPresence[2], micShelf[2];
    Biquad micTypeA[2], micTypeB[2], micTypeC[2];   // mic character (identity for the dynamic)
    Biquad distLow[2], distHigh[2];                  // proximity loss and air absorption with distance
    FracDelay distDelay[2];                          // time of flight + floor reflection
    float cachedMic[2] { -1, -1 }, cachedDist[2] { -1, -1 };
    int cachedType[2] { -1, -1 };
    juce::AudioBuffer<float> scratch;
    juce::SmoothedValue<float> gainL[2], gainR[2];

    std::atomic<int> loadedSource[2] { { -1 }, { -1 } };
    int wantedSource[2] { 0, 0 };
    juce::File userFile[2];
    juce::AudioBuffer<float> userIR[2];  // as read from disk (mono, untrimmed)
    double userRate[2] { 48000, 48000 };
    bool align = true;
    int trimmed[2] { 0, 0 };
};

} // namespace abyssal
