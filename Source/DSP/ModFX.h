#pragma once

#include "Common.h"
#include "PitchShifter.h"

namespace abyssal
{

//============================================================================== Wah
struct WahParams
{
    bool on = false;
    bool autoMode = false;   // false: the Position control is the pedal (automate it, or map a CC)
    float position = 0.5f;   // heel (0) .. toe (1)
    float sensitivity = 0.5f, resonance = 0.5f;
};

/** Cry-baby style wah: a resonant band-pass swept from about 350 Hz (heel) to 2.2 kHz (toe), with a
    little low-pass blended in as the inductor-based circuit does. Auto mode follows the playing. */
class Wah
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* data, int n, const WahParams& p) noexcept;
    float getPosition() const noexcept { return smoothedPos; }

private:
    double sr = 48000;
    float ic1 = 0, ic2 = 0;             // TPT state-variable filter state
    float env = 0, smoothedPos = 0.5f;
    float envAttack = 0, envRelease = 0, posSmooth = 0;
};

//============================================================================== Whammy
struct WhammyParams
{
    static juce::StringArray intervalNames() { return { "+2 Oct", "+1 Oct", "+5th", "+4th", "-4th", "-5th", "-1 Oct", "-2 Oct" }; }
    static constexpr float intervalSemis[8] = { 24, 12, 7, 5, -5, -7, -12, -24 };

    bool on = false;
    int interval = 1;
    float position = 1.0f;  // 0 = no shift, 1 = the full interval (sweep it like the treadle)
    float mix = 1.0f;       // 1 = only the shifted signal (classic whammy), lower = harmony
};

class Whammy
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* data, int n, const WhammyParams& p) noexcept;
    int getLatencySamples() const noexcept { return shifter.getLatencySamples(); }

private:
    PitchShifter shifter;
    juce::SmoothedValue<float> semis;
};

//============================================================================== Phaser
struct PhaserParams
{
    bool on = false;
    float rate = 0.5f, depth = 0.7f, feedback = 0.4f, mix = 0.5f;
    int stages = 0;         // 0 = 4 stages, 1 = 8 stages
};

/** Swept first-order all-pass chain with feedback. `lfoOffset` (in cycles) lets the right channel's
    instance run out of phase with the left for width. */
class Phaser
{
public:
    void prepare (double sampleRate, float lfoOffset = 0.0f);
    void reset();
    void process (float* data, int n, const PhaserParams& p) noexcept;

private:
    double sr = 48000;
    float phase = 0, offset = 0, fb = 0;
    float state[8] {};
};

//============================================================================== Tremolo
struct TremoloParams
{
    bool on = false;
    float rate = 5.0f, depth = 0.6f, shape = 0.3f; // shape: 0 = smooth sine, 1 = hard chop
    bool sync = false, pan = false;                // pan: the right channel runs opposite (post-amp)
    int division = 5;
};

class Tremolo
{
public:
    void prepare (double sampleRate, bool rightChannel = false);
    void reset();
    void process (float* data, int n, const TremoloParams& p, double bpm) noexcept;

private:
    double sr = 48000;
    bool right = false;
    double phase = 0;
    juce::SmoothedValue<float> depthSmooth;
};

//============================================================================== Doubler
struct DoublerParams
{
    bool on = false;
    float width = 0.8f, timeMs = 14.0f, detune = 0.4f;
};

/** Fakes a double-tracked part: each side is blended with its own slightly late, slowly drifting
    copy, so left and right stop being identical. */
class Doubler
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float* l, float* r, int n, const DoublerParams& p) noexcept;

private:
    double sr = 48000;
    FracDelay line[2];
    double phase[4] {};
    juce::Random random { 0x5eed };
    float drift[2] {}, driftTarget[2] {};
    int driftCountdown = 0;
};

} // namespace abyssal
