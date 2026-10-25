#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <complex>

namespace abyssal
{

/** Note values for tempo-synced effects (delay, tremolo). */
struct DelayDivisions
{
    static constexpr int count = 8;
    static constexpr float beats[count] = { 4.0f, 2.0f, 1.5f, 1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f };
    static juce::StringArray names() { return { "1/1", "1/2", "1/4.", "1/4", "1/8.", "1/8", "1/8T", "1/16" }; }
};

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

//==============================================================================
/** RBJ-cookbook biquad, transposed direct form II. Coefficients are computed in double precision. */
class Biquad
{
public:
    enum class Type { LowPass, HighPass, Peak, LowShelf, HighShelf, BandPass, Notch, AllPass };

    void setup (Type t, double sampleRate, double freq, double q = 0.7071, double gainDb = 0.0)
    {
        freq = juce::jlimit (5.0, sampleRate * 0.49, freq);
        q    = std::max (q, 0.05);

        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const double cw    = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (t)
        {
            case Type::LowPass:   b0 = (1 - cw) * 0.5; b1 = 1 - cw; b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::HighPass:  b0 = (1 + cw) * 0.5; b1 = -(1 + cw); b2 = b0; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::BandPass:  b0 = alpha; b1 = 0; b2 = -alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::Notch:     b0 = 1; b1 = -2 * cw; b2 = 1; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::AllPass:   b0 = 1 - alpha; b1 = -2 * cw; b2 = 1 + alpha; a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::Peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A;
                break;
            case Type::LowShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + sq);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - sq);
                a0 = (A + 1) + (A - 1) * cw + sq;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - sq;
                break;
            }
            case Type::HighShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + sq);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - sq);
                a0 = (A + 1) - (A - 1) * cw + sq;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - sq;
                break;
            }
        }

        B0 = b0 / a0; B1 = b1 / a0; B2 = b2 / a0; A1 = a1 / a0; A2 = a2 / a0;
    }

    void setIdentity() noexcept { B0 = 1; B1 = B2 = A1 = A2 = 0; }
    void reset() noexcept { z1 = z2 = 0; }

    inline float process (float x) noexcept
    {
        const double y = B0 * x + z1;
        z1 = B1 * x - A1 * y + z2;
        z2 = B2 * x - A2 * y;
        return (float) y;
    }

    void process (float* data, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
            data[i] = process (data[i]);
    }

    /** Magnitude response at a frequency, used by the EQ display. */
    double magnitude (double freq, double sampleRate) const noexcept
    {
        const double w = juce::MathConstants<double>::twoPi * freq / sampleRate;
        const std::complex<double> z1c = std::polar (1.0, -w), z2c = std::polar (1.0, -2 * w);
        return std::abs ((B0 + B1 * z1c + B2 * z2c) / (1.0 + A1 * z1c + A2 * z2c));
    }

private:
    double B0 = 1, B1 = 0, B2 = 0, A1 = 0, A2 = 0;
    double z1 = 0, z2 = 0;
};

//==============================================================================
/** One-pole lowpass / highpass for cheap DC blocking and smoothing. */
class OnePole
{
public:
    void setLowpass (double sampleRate, double freq)
    {
        a = (float) std::exp (-juce::MathConstants<double>::twoPi * freq / sampleRate);
    }
    void reset() noexcept { lp = 0; }
    inline float lowpass (float x) noexcept  { lp = x + a * (lp - x); return lp; }
    inline float highpass (float x) noexcept { return x - lowpass (x); }

private:
    float a = 0, lp = 0;
};

//==============================================================================
/** Smoothly crossfades a module in and out so footswitch clicks don't pop. */
class BypassRamp
{
public:
    void prepare (double sampleRate, bool initiallyOn)
    {
        mix.reset (sampleRate, 0.02);
        mix.setCurrentAndTargetValue (initiallyOn ? 1.0f : 0.0f);
        wasActive = initiallyOn;
    }

    /** Set by the engine so a module is reset when it comes back from full bypass. */
    bool wasActive = false;

    void setOn (bool on) noexcept { mix.setTargetValue (on ? 1.0f : 0.0f); }

    /** True if the module must run this block (on, or fading). */
    bool isActive() const noexcept { return mix.isSmoothing() || mix.getTargetValue() > 0.5f; }
    bool isFading() const noexcept { return mix.isSmoothing(); }

    /** wet already contains processed audio, dry the original; result written into wet. */
    void blend (const float* dry, float* wet, int n) noexcept
    {
        if (! mix.isSmoothing())
        {
            if (mix.getTargetValue() < 0.5f)
                std::copy (dry, dry + n, wet);
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const float m = mix.getNextValue();
            wet[i] = dry[i] + m * (wet[i] - dry[i]);
        }
    }

    /** Stereo variant: advances the ramp once per frame for both channels. */
    void blend (const float* dryL, const float* dryR, float* wetL, float* wetR, int n) noexcept
    {
        if (! mix.isSmoothing())
        {
            if (mix.getTargetValue() < 0.5f)
            {
                std::copy (dryL, dryL + n, wetL);
                std::copy (dryR, dryR + n, wetR);
            }
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const float m = mix.getNextValue();
            wetL[i] = dryL[i] + m * (wetL[i] - dryL[i]);
            wetR[i] = dryR[i] + m * (wetR[i] - dryR[i]);
        }
    }

private:
    juce::SmoothedValue<float> mix;
};

//==============================================================================
/** Fractional delay line with cubic (Hermite) interpolation. Size is rounded to a power of two. */
class FracDelay
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 4) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    inline void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** delay in samples, measured from the most recently pushed sample (0 = newest). */
    inline float read (float delay) const noexcept
    {
        delay = juce::jlimit (1.0f, (float) (mask - 3), delay);
        const int   di   = (int) delay;
        const float frac = delay - (float) di;
        const int   base = writePos - 1 - di;

        const float xm1 = buffer[(size_t) ((base + 1) & mask)];
        const float x0  = buffer[(size_t) ((base)     & mask)];
        const float x1  = buffer[(size_t) ((base - 1) & mask)];
        const float x2  = buffer[(size_t) ((base - 2) & mask)];

        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + x0;
    }

    int capacity() const noexcept { return mask + 1; }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

} // namespace abyssal
