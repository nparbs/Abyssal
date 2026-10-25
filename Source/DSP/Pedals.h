#pragma once

#include "Common.h"
#include "PitchShifter.h"

namespace abyssal
{

//============================================================================== Noise gate
struct GateParams
{
    bool on = true;
    float threshDb = -60, releaseMs = 80;
    bool keyFromInput = false; // detect from the guitar input instead of the signal at the pedal
};

class NoiseGate
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    /** `key` (optional) is the detector signal; null uses `data` itself. */
    void process (float* data, int n, const GateParams& p, const float* key = nullptr);
    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    double sr = 48000;
    float env = 0, gain = 1, holdCounter = 0;
    bool open = false;
    float gainReductionDb = 0;
};

//============================================================================== Pitch shifting
struct OctaverParams { bool on = false; float dry = 1, down1 = 0.5f, down2 = 0; };

class Octaver
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* data, int n, const OctaverParams& p);

private:
    PitchShifter oct1, oct2;
    Biquad preLp, postLp1, postLp2;
};

struct DropTuneParams { bool on = false; int semitones = -2; float mix = 1; };

class DropTune
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* data, int n, const DropTuneParams& p);
    int getLatencySamples() const noexcept { return shifter.getLatencySamples(); }

private:
    PitchShifter shifter;
};

//============================================================================== Compressor
struct CompParams { bool on = false; float threshDb = -20, ratio = 4, attackMs = 5, releaseMs = 100, makeupDb = 0; };

class Compressor
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* data, int n, const CompParams& p);

private:
    double sr = 48000;
    float envDb = -120;
};

//============================================================================== Overdrive (TS-style mid boost)
struct OverdriveParams { bool on = true; float drive = 1, tone = 6, level = 8; };

class Overdrive
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* data, int n, const OverdriveParams& p);

private:
    double sr = 48000;
    Biquad clipHp, clipLp, inHp, toneLp, toneShelf;
    float lastTone = -1;
};

//============================================================================== Graphic EQ
struct GraphicEQParams
{
    static constexpr int numBands = 9;
    static constexpr float freqs[numBands] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    bool on = false;
    float gainsDb[numBands] {};
    float levelDb = 0;
};

class GraphicEQ
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* data, int n, const GraphicEQParams& p);

private:
    double sr = 48000;
    Biquad bands[GraphicEQParams::numBands];
    float cached[GraphicEQParams::numBands];
};

} // namespace abyssal
