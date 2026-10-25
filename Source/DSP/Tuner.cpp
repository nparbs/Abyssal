#include "Tuner.h"

namespace abyssal
{

//============================================================================== TunerTap
void TunerTap::prepare (double sampleRate)
{
    factor = std::max (1, (int) (sampleRate / 10000.0));
    rate = sampleRate / factor;
    // Two cascaded lowpasses keep harmonics above the new Nyquist from folding back.
    lp1.setup (Biquad::Type::LowPass, sampleRate, 1800.0, 0.54);
    lp2.setup (Biquad::Type::LowPass, sampleRate, 1800.0, 1.31);
    lp1.reset();
    lp2.reset();
    phase = 0;
}

void TunerTap::push (const float* data, int n) noexcept
{
    int w = writePos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float y = lp2.process (lp1.process (data[i]));
        if (++phase >= factor)
        {
            phase = 0;
            ring[w] = y;
            w = (w + 1) & (ringSize - 1);
        }
    }
    writePos.store (w, std::memory_order_release);
}

void TunerTap::read (float* dest, int n) const noexcept
{
    n = std::min (n, ringSize);
    const int w = writePos.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i)
        dest[i] = ring[(w - n + i + ringSize) & (ringSize - 1)];
}

//============================================================================== PitchDetector
PitchDetector::Result PitchDetector::detect (const float* x, int n, double sampleRate, float minHz, float maxHz)
{
    const int tauMax = std::min (n / 2, (int) (sampleRate / minHz));
    const int tauMin = std::max (2, (int) (sampleRate / maxHz));
    const int window = n - tauMax;
    if (tauMax <= tauMin + 2 || window < 32)
        return {};

    // Too quiet to bother (also avoids locking onto the noise floor).
    float energy = 0.0f;
    for (int i = 0; i < n; ++i) energy += x[i] * x[i];
    if (energy / (float) n < 1.0e-8f)
        return {};

    // Difference function, then its cumulative mean normalised form.
    diff.assign ((size_t) tauMax + 1, 0.0f);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float sum = 0.0f;
        for (int i = 0; i < window; ++i)
        {
            const float d = x[i] - x[i + tau];
            sum += d * d;
        }
        diff[(size_t) tau] = sum;
    }

    diff[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        running += diff[(size_t) tau];
        diff[(size_t) tau] = running > 0.0f ? diff[(size_t) tau] * (float) tau / running : 1.0f;
    }

    // First dip below the threshold, walked down to its local minimum. Falls back to the global
    // minimum if nothing is clearly periodic.
    constexpr float threshold = 0.15f;
    int best = -1;
    for (int tau = tauMin; tau < tauMax; ++tau)
    {
        if (diff[(size_t) tau] < threshold)
        {
            while (tau + 1 < tauMax && diff[(size_t) tau + 1] < diff[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (diff[(size_t) tau] < diff[(size_t) best]) best = tau;
        if (diff[(size_t) best] > 0.35f)
            return {};
    }

    // Sub-sample refinement. A parabola through three lags is biased when the period is only a few
    // dozen samples (high notes read several cents sharp), so instead evaluate the difference
    // function at fractional lags - the input is band-limited well below Nyquist, so cubic
    // interpolation is accurate - and golden-section search the minimum within +-1 sample.
    const int refineWindow = n - tauMax - 3;
    auto diffAt = [x, refineWindow] (double lag)
    {
        const int li = (int) lag;
        const float f = (float) (lag - li);
        double sum = 0.0;
        for (int i = 1; i < refineWindow; ++i)
        {
            const float* p = x + i + li;
            const float c1 = 0.5f * (p[1] - p[-1]);
            const float c2 = p[-1] - 2.5f * p[0] + 2.0f * p[1] - 0.5f * p[2];
            const float c3 = 0.5f * (p[2] - p[-1]) + 1.5f * (p[0] - p[1]);
            const float shifted = ((c3 * f + c2) * f + c1) * f + p[0];
            const double d = x[i] - shifted;
            sum += d * d;
        }
        return sum;
    };

    double lo = std::max (1.0, best - 1.0), hi = std::min ((double) tauMax - 1.0, best + 1.0);
    constexpr double golden = 0.6180339887498949;
    double a = hi - golden * (hi - lo), b = lo + golden * (hi - lo);
    double fa = diffAt (a), fb = diffAt (b);
    for (int it = 0; it < 24; ++it)
    {
        if (fa < fb) { hi = b; b = a; fb = fa; a = hi - golden * (hi - lo); fa = diffAt (a); }
        else         { lo = a; a = b; fa = fb; b = lo + golden * (hi - lo); fb = diffAt (b); }
    }
    const double tau = 0.5 * (lo + hi);

    return { (float) (sampleRate / tau), juce::jlimit (0.0f, 1.0f, 1.0f - diff[(size_t) best]) };
}

//============================================================================== NoteReading
NoteReading NoteReading::fromFrequency (float hz, float a4)
{
    NoteReading r;
    if (hz <= 0.0f || a4 <= 0.0f)
        return r;

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const float midi = 69.0f + 12.0f * std::log2 (hz / a4);
    r.midiNote = (int) std::round (midi);
    r.cents = (midi - (float) r.midiNote) * 100.0f;
    r.name = names[((r.midiNote % 12) + 12) % 12];
    r.octave = r.midiNote / 12 - 1;
    return r;
}

} // namespace abyssal
