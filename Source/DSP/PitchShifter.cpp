#include "PitchShifter.h"

namespace abyssal
{

void PitchShifter::prepare (double sampleRate, double maxPeriodMs)
{
    sr = sampleRate;
    const double scale = sr / 48000.0;
    minDelay      = std::max (4, (int) std::round (1.0 * 0.001 * sr));   // 1 ms headroom for interpolation and fades
    maxLag        = (int) std::round (maxPeriodMs * 0.001 * sr);
    minLagFloor   = (int) std::round (0.5 * 0.001 * sr);
    fadeLen       = (int) std::round (6.0 * 0.001 * sr);
    corrLen       = (int) std::round (10.0 * 0.001 * sr);
    retryInterval = (int) std::round (128 * scale);
    setRatio (ratio);

    int size = 1;
    while (size < minDelay + 2 * maxLag + fadeLen + corrLen + 64) size <<= 1;
    buffer.assign ((size_t) size, 0.0f);
    mask = size - 1;

    // Typical average delay: about 5-9 ms on single notes, about 15 ms on chords (their combined
    // period is longer). See AbyssalRender --dsptest.
    nominalLatency = minDelay + (int) std::round (9.0 * 0.001 * sr);
    reset();
}

void PitchShifter::setRatio (float r) noexcept
{
    ratio = r;
    // Big shifts move the taps quickly: shorten the crossfade so a splice still fits in the search
    // range (two octaves up would otherwise never find room to jump).
    const double travel = std::abs (1.0 - r);
    nextFade = travel < 1.0e-6 ? fadeLen : juce::jlimit (std::max (8, fadeLen / 8), fadeLen, (int) (maxLag / (4.0 * travel)));
}

void PitchShifter::reset()
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
    delay[0] = delay[1] = minDelay + maxLag * 0.5;
    active = 0;
    fadePos = -1;
    retryCountdown = 0;
    searchedRoom = 0;
    fade = nextFade;
    averageDelay = delay[0];
}

float PitchShifter::tapAt (double d) const noexcept
{
    const int di = (int) d;
    const float f = (float) (d - di);
    const float xm1 = at (di - 1), x0 = at (di), x1 = at (di + 1), x2 = at (di + 2);
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

int PitchShifter::findSplice (double d, bool towardsShorter, int lagLimit, bool force) const noexcept
{
    const int base = (int) std::round (d);
    // A splice needs time to complete before the next one: never jump by less than the tap travels
    // during two crossfades.
    const double travel = std::abs (1.0 - ratio);
    const int minLag = std::max (minLagFloor, (int) std::ceil (2.0 * nextFade * travel));
    if (lagLimit < minLag)
        return 0;

    // Normalised correlation between the last corrLen samples each tap position has produced.
    auto score = [&] (int lag, int step)
    {
        const int other = towardsShorter ? base - lag : base + lag;
        double ab = 0, aa = 0, bb = 0;
        for (int k = 0; k < corrLen; k += step)
        {
            const float a = at (base + k), b = at (other + k);
            ab += a * b; aa += a * a; bb += b * b;
        }
        return ab / std::sqrt (aa * bb + 1.0e-12);
    };

    constexpr double goodMatch = 0.9;

    // Coarse search (every other lag, every 4th sample at 48 kHz; the strides scale with the rate so
    // the cost doesn't), then refine around the winner. Jumping forward (shifting up) adds delay, so
    // there the shortest good match wins, not the best one.
    const int lagStride = std::max (2, (int) std::round (2.0 * sr / 48000.0));
    const int sampleStride = std::max (4, (int) std::round (4.0 * sr / 48000.0));
    int best = minLag;
    double bestScore = -2.0;
    for (int lag = minLag; lag <= lagLimit; lag += lagStride)
    {
        const double s = score (lag, sampleStride);
        if (s > bestScore) { bestScore = s; best = lag; }
        if (! towardsShorter && s >= goodMatch + 0.03) break;
    }
    int refined = best;
    double refinedScore = -2.0;
    for (int lag = std::max (minLag, best - lagStride); lag <= std::min (lagLimit, best + lagStride); ++lag)
        if (const double s = score (lag, sampleStride / 2); s > refinedScore) { refinedScore = s; refined = lag; }

    return (refinedScore >= goodMatch || force) ? refined : 0;
}

float PitchShifter::process (float x) noexcept
{
    buffer[(size_t) writePos] = x;
    writePos = (writePos + 1) & mask;

    const double step = 1.0 - ratio; // delay change per sample (> 0 shifting down)
    float out;

    if (fadePos >= 0)
    {
        const int from = 1 - active;
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) fadePos / (float) fade);
        out = tapAt (delay[from]) * (1.0f - w) + tapAt (delay[active]) * w;
        delay[from] += step;
        if (++fadePos >= fade)
            fadePos = -1;
    }
    else
    {
        out = tapAt (delay[active]);

        // Look for a splice when there's room for one, retrying every few milliseconds until a good
        // match comes into range; at the end of the range, take the best there is.
        const double d = delay[active];
        const bool down = step > 1.0e-6, up = step < -1.0e-6;
        if ((down || up) && --retryCountdown <= 0)
        {
            retryCountdown = retryInterval;
            const int travelDuringFade = (int) std::ceil (std::abs (step) * nextFade) + 2;
            int lag = 0;
            if (down)
            {
                // Search again only once the range has grown noticeably: a full search per retry
                // would cost far more than the splice itself.
                const int room = (int) (d - minDelay) - travelDuringFade;
                const bool last = room >= maxLag;
                if (last || room >= searchedRoom + std::max (minLagFloor * 2, searchedRoom / 8))
                {
                    searchedRoom = room;
                    lag = findSplice (d, true, std::min (room, maxLag), last);
                }
            }
            else if (d < minDelay + travelDuringFade + std::abs (step) * retryInterval + 2)
            {
                lag = findSplice (d, false, maxLag, true); // must jump now, before the tap runs out of delay
            }
            if (lag > 0)
            {
                const int next = 1 - active;
                delay[next] = down ? d - lag : d + lag;
                active = next;
                fadePos = 0;
                fade = nextFade; // the length only changes between splices, never mid-crossfade
                searchedRoom = 0;
            }
        }
    }

    delay[active] += step;
    delay[active] = juce::jlimit ((double) 1.0, (double) (mask - corrLen - 4), delay[active]);
    averageDelay += 0.0002 * (delay[active] - averageDelay);
    return out;
}

} // namespace abyssal
