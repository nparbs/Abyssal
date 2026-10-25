#pragma once

#include "Common.h"

namespace abyssal
{

/** Audio-thread side of the tuner: low-passes the input, decimates it to roughly 10 kHz and keeps
    the most recent few hundred milliseconds in a ring buffer the UI can read at any time. A torn
    read only ever costs one slightly wrong reading, so there is no locking. */
class TunerTap
{
public:
    static constexpr int ringSize = 4096; // ~0.37 s at 11 kHz

    void prepare (double sampleRate);
    void push (const float* data, int n) noexcept;

    /** Copies the newest `n` decimated samples (oldest first). Any thread. */
    void read (float* dest, int n) const noexcept;
    double getRate() const noexcept { return rate.load(); }

private:
    Biquad lp1, lp2;
    int factor = 4, phase = 0;
    std::atomic<double> rate { 12000.0 };
    float ring[ringSize] {};
    std::atomic<int> writePos { 0 };
};

/** YIN pitch detection (de Cheveigné & Kawahara) tuned for guitar and bass: covers down to about
    25 Hz so drop-tuned 7/8-strings and basses read correctly. */
class PitchDetector
{
public:
    struct Result
    {
        float frequency = 0.0f; // 0 = no clear pitch
        float clarity = 0.0f;   // 1 = perfectly periodic
    };

    Result detect (const float* x, int n, double sampleRate, float minHz = 25.0f, float maxHz = 1500.0f);

private:
    std::vector<float> diff;
};

/** Note name, octave and cents offset for a frequency, given the A4 reference. */
struct NoteReading
{
    int midiNote = -1;
    float cents = 0.0f;
    juce::String name;  // e.g. "F#"
    int octave = 0;

    static NoteReading fromFrequency (float hz, float a4 = 440.0f);
};

} // namespace abyssal
