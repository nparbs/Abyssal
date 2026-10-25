#pragma once

#include "Common.h"
#include "PitchShifter.h"

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
    static juce::StringArray divisionNames() { return DelayDivisions::names(); }

    static juce::StringArray modeNames() { return { "Digital", "Tape", "Analog" }; }

    bool on = false, sync = false, pingPong = true;
    float timeMs = 380, feedback = 0.35f, toneHz = 5000, mix = 0.25f;
    int division = 3;
    int mode = 0;   // 0 = digital (clean repeats), 1 = tape (wow/flutter, saturation), 2 = analog (dark BBD repeats)
};

class StereoDelay
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const DelayParams& p, double bpm);

private:
    void updateModeFilters (int mode, float toneHz);

    double sr = 48000;
    FracDelay line[2];
    Biquad lp[2], hp[2], colour[2];   // colour: the tape / bucket-brigade's own roll-off
    float lastTone = -1;
    int lastMode = -1;
    double wowPhase = 0, flutterPhase = 0;
    juce::SmoothedValue<float> delaySamples;
};

//============================================================================== Reverb
struct ReverbParams
{
    static juce::StringArray modeNames() { return { "Hall", "Plate", "Shimmer" }; }

    bool on = false;
    float size = 0.5f, damping = 0.5f, predelayMs = 20, width = 1, mix = 0.2f;
    int mode = 0;          // 0 = hall, 1 = plate, 2 = shimmer (hall with octave-up feedback)
    float shimmer = 0.5f;  // shimmer mode: how much of the octave-up signal is fed back
};

/** Plate reverb after Dattorro's figure-of-eight tank ("Effect Design Part 1", JAES 1997): four
    input diffusers, then two cross-coupled branches of modulated all-pass, delay, damping and all-pass. */
class PlateReverb
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, float size, float damping, float width) noexcept;

private:
    struct Line
    {
        std::vector<float> buffer;
        int mask = 0, pos = 0;
        void prepare (int maxDelay) { int s = 1; while (s < maxDelay + 4) s <<= 1; buffer.assign ((size_t) s, 0.0f); mask = s - 1; pos = 0; }
        void clear() { std::fill (buffer.begin(), buffer.end(), 0.0f); }
        void push (float x) noexcept { buffer[(size_t) pos] = x; pos = (pos + 1) & mask; }
        /** The sample pushed `d` pushes ago (1 = the newest). */
        float tap (int d) const noexcept { return buffer[(size_t) ((pos - d) & mask)]; }
        float tapFrac (float d) const noexcept
        {
            const int i = (int) d;
            const float f = d - (float) i;
            return tap (i) + f * (tap (i + 1) - tap (i));
        }
    };

    /** Schroeder all-pass through `line` with delay `len`. */
    static float allpass (Line& line, float len, float g, float x) noexcept
    {
        const float d = line.tapFrac (len);
        const float v = x - g * d;
        line.push (v);
        return d + g * v;
    }

    int scaled (int samplesAt29761) const noexcept { return std::max (2, (int) std::round (samplesAt29761 * sr / 29761.0)); }

    double sr = 48000;
    Line diffuser[4], ap1[2], delay1[2], ap2[2], delay2[2];
    float diffuserLen[4] {}, ap1Len[2] {}, delay1Len[2] {}, ap2Len[2] {}, delay2Len[2] {};
    float bandwidthState = 0, dampState[2] {}, tankOut[2] {};
    double modPhase = 0;
    float modDepth = 16;
};

class Reverb
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (float* l, float* r, int n, const ReverbParams& p);

private:
    double sr = 48000;
    juce::Reverb reverb;
    PlateReverb plate;
    PitchShifter shimmerShift;
    Biquad shimmerHp, shimmerLp;
    FracDelay pre[2];
    Biquad lowCut[2];
    juce::AudioBuffer<float> wet, shimmerBuf;
    int lastMode = -1;
};

} // namespace abyssal
