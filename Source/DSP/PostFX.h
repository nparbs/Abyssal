#pragma once

#include "Common.h"

namespace abyssal
{

//============================================================================== Parametric EQ
struct ParamEQParams
{
    static constexpr int numBands = 4;
    struct Band { float freq = 1000, gainDb = 0, q = 1; bool operator== (const Band&) const = default; };

    bool on = true;
    float hpfHz = 80, lpfHz = 12000;
    Band band[numBands];

    bool operator== (const ParamEQParams&) const = default;
};

class ParamEQ
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const ParamEQParams& p);

    /** Total response in dB of a parameter set, for drawing the EQ curve. */
    static float responseDb (const ParamEQParams& p, double freq, double sampleRate);

private:
    static void design (const ParamEQParams& p, double sampleRate, Biquad (&filters)[8]);
    double sr = 48000;
    Biquad filters[2][8]; // [channel][hp1, hp2, band x4, lp1, lp2]
    ParamEQParams cached;
    bool dirty = true;
};

//============================================================================== Chorus / flanger
struct ModParams { bool on = false; float rate = 0.8f, depth = 0.4f, feedback = 0, mix = 0.4f; };

class ModDelay
{
public:
    enum class Mode { Chorus, Flanger };
    explicit ModDelay (Mode m) : mode (m) {}

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const ModParams& p);

private:
    Mode mode;
    double sr = 48000;
    FracDelay line[2];
    float lfoPhase = 0, fbState[2] {};
    Biquad fbHp[2];
};

//============================================================================== Delay
struct DelayParams
{
    static juce::StringArray divisionNames() { return { "1/1", "1/2", "1/4.", "1/4", "1/8.", "1/8", "1/8T", "1/16" }; }
    static constexpr float divisionBeats[8] = { 4.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };

    bool on = false, sync = false, pingPong = true;
    float timeMs = 380, feedback = 0.35f, toneHz = 5000, mix = 0.25f;
    int division = 3;
};

class StereoDelay
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const DelayParams& p, double bpm);

private:
    double sr = 48000;
    FracDelay line[2];
    Biquad lp[2], hp[2];
    float lastTone = -1;
    juce::SmoothedValue<float> delaySamples;
};

//============================================================================== Reverb
struct ReverbParams { bool on = false; float size = 0.5f, damping = 0.5f, predelayMs = 20, width = 1, mix = 0.2f; };

class Reverb
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const ReverbParams& p);

private:
    double sr = 48000;
    juce::Reverb reverb;
    FracDelay pre[2];
    Biquad lowCut[2];
    juce::AudioBuffer<float> wet;
};

} // namespace abyssal
