#pragma once

#include "Common.h"

namespace abyssal
{

/** Two-tap delay-line pitch shifter that chooses its splice points.

    A read tap sweeps through a delay line at (1 - ratio) samples per sample, which shifts the
    pitch. Every so often it has to jump back (shifting down) or forward (up). A plain shifter jumps
    by a fixed window and crossfades continuously, so the two taps beat against each other (the
    "warble") and the delay averages half the window. Here the jump distance is picked by
    cross-correlating the audio at the two positions: the new tap starts where the waveform
    lines up with the old one, a short crossfade hides the seam, and the tap jumps as soon as a good
    match is in range. The average delay ends up around one period of the note instead of a fixed
    window, and chords cancel far less during the crossfades. */
class PitchShifter
{
public:
    /** `maxPeriodMs` is the longest period the splice search covers (lowest note it aligns). */
    void prepare (double sampleRate, double maxPeriodMs = 30.0);
    void reset();
    void setRatio (float r) noexcept;
    float process (float x) noexcept;

    /** Typical delay of the shifted signal over the guitar range, in samples (reported to hosts). */
    int getLatencySamples() const noexcept { return nominalLatency; }
    /** Running average of the actual delay, in samples (for tests). */
    double getAverageDelay() const noexcept { return averageDelay; }

private:
    float tapAt (double delay) const noexcept;
    float at (int delay) const noexcept { return buffer[(size_t) ((writePos - 1 - delay) & mask)]; }
    /** Best jump (in samples, > 0) from `delay` towards shorter (down) or longer (up) delays, or 0
        when no good match is in range yet. */
    int findSplice (double delay, bool towardsShorter, int maxLag, bool force) const noexcept;

    std::vector<float> buffer;
    int mask = 0, writePos = 0;

    double sr = 48000;
    float ratio = 1.0f;
    int minDelay = 48, maxLag = 1440, minLagFloor = 24, fadeLen = 192, corrLen = 480, retryInterval = 128;
    int nominalLatency = 0;
    int fade = 288;         // length of the crossfade in progress
    int nextFade = 288;     // crossfade length for the current ratio (<= fadeLen)

    double delay[2] { 48, 48 };
    int active = 0;
    int fadePos = -1;       // >= 0 while crossfading from the other tap to `active`
    int retryCountdown = 0;
    int searchedRoom = 0;   // splice range covered by the last unsuccessful search
    double averageDelay = 0;
};

} // namespace abyssal
