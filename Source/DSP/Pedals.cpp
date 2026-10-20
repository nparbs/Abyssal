#include "Pedals.h"

namespace abyssal
{

static inline float timeCoef (double sampleRate, double ms)
{
    return (float) std::exp (-1.0 / (std::max (ms, 0.01) * 0.001 * sampleRate));
}

//============================================================================== NoiseGate
void NoiseGate::prepare (double sampleRate, int) { sr = sampleRate; reset(); }
void NoiseGate::reset() { env = 0; gain = 0; holdCounter = 0; open = false; gainReductionDb = 0; }

void NoiseGate::process (float* data, int n, const GateParams& p)
{
    const float detRelease = timeCoef (sr, 25.0);
    const float attack     = timeCoef (sr, 0.5);
    const float release    = timeCoef (sr, p.releaseMs);
    const float openLin    = dbToGain (p.threshDb);
    const float closeLin   = dbToGain (p.threshDb - 6.0f); // hysteresis prevents chattering on sustain tails
    const float holdLen    = (float) (0.015 * sr);

    for (int i = 0; i < n; ++i)
    {
        const float a = std::abs (data[i]);
        env = a > env ? a : env * detRelease;

        if (env > openLin)            { open = true; holdCounter = holdLen; }
        else if (env < closeLin)      { if (holdCounter > 0) holdCounter -= 1.0f; else open = false; }

        const float target = open ? 1.0f : 0.0f;
        const float coef   = target > gain ? attack : release;
        gain = target + (gain - target) * coef;
        data[i] *= gain;
    }

    gainReductionDb = gainToDb (gain);
}

//============================================================================== PitchShifter
void PitchShifter::prepare (double sampleRate, double windowMs)
{
    window = (float) (windowMs * 0.001 * sampleRate);
    delay.prepare ((int) (window * 2.0f) + 16);
    reset();
}

void PitchShifter::reset() { delay.reset(); phase = 0; }

float PitchShifter::process (float x) noexcept
{
    delay.push (x);

    phase += (1.0f - ratio) / window;
    if (phase >= 1.0f) phase -= 1.0f;
    if (phase < 0.0f)  phase += 1.0f;

    float phase2 = phase + 0.5f;
    if (phase2 >= 1.0f) phase2 -= 1.0f;

    // Each tap fades to zero exactly where its delay wraps, and the sin² pair always sums to 1.
    const float s1 = std::sin (juce::MathConstants<float>::pi * phase);
    const float s2 = std::sin (juce::MathConstants<float>::pi * phase2);
    return s1 * s1 * delay.read (phase * window + 2.0f)
         + s2 * s2 * delay.read (phase2 * window + 2.0f);
}

//============================================================================== Octaver
void Octaver::prepare (double sampleRate, int)
{
    oct1.prepare (sampleRate, 55.0);
    oct2.prepare (sampleRate, 70.0);
    oct1.setRatio (0.5f);
    oct2.setRatio (0.25f);
    preLp.setup (Biquad::Type::LowPass, sampleRate, 1400.0, 0.6);
    postLp1.setup (Biquad::Type::LowPass, sampleRate, 2200.0, 0.6);
    postLp2.setup (Biquad::Type::LowPass, sampleRate, 1200.0, 0.6);
    reset();
}

void Octaver::reset() { oct1.reset(); oct2.reset(); preLp.reset(); postLp1.reset(); postLp2.reset(); }

void Octaver::process (float* data, int n, const OctaverParams& p)
{
    for (int i = 0; i < n; ++i)
    {
        const float x  = data[i];
        const float lp = preLp.process (x); // feeding the shifters a darker signal reduces warble
        const float o1 = postLp1.process (oct1.process (lp));
        const float o2 = postLp2.process (oct2.process (lp));
        data[i] = p.dry * x + 1.4f * (p.down1 * o1 + p.down2 * o2);
    }
}

//============================================================================== DropTune
void DropTune::prepare (double sampleRate, int) { shifter.prepare (sampleRate, 35.0); reset(); }
void DropTune::reset() { shifter.reset(); }

void DropTune::process (float* data, int n, const DropTuneParams& p)
{
    shifter.setRatio (std::pow (2.0f, (float) p.semitones / 12.0f));
    for (int i = 0; i < n; ++i)
    {
        const float x = data[i];
        const float s = shifter.process (x); // keep the shifter running at 0 st so changes stay seamless
        data[i] = p.semitones == 0 ? x : x + p.mix * (s - x);
    }
}

//============================================================================== Compressor
void Compressor::prepare (double sampleRate, int) { sr = sampleRate; reset(); }
void Compressor::reset() { envDb = -120.0f; }

void Compressor::process (float* data, int n, const CompParams& p)
{
    const float att = timeCoef (sr, p.attackMs);
    const float rel = timeCoef (sr, p.releaseMs);
    const float knee = 6.0f;
    const float slope = 1.0f / std::max (1.0f, p.ratio) - 1.0f;

    for (int i = 0; i < n; ++i)
    {
        const float lvl = gainToDb (std::abs (data[i]));
        envDb = lvl + (envDb - lvl) * (lvl > envDb ? att : rel);

        const float over = envDb - p.threshDb;
        float gr = 0.0f;
        if (over >= knee * 0.5f)      gr = slope * over;
        else if (over > -knee * 0.5f) gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

        data[i] *= dbToGain (gr + p.makeupDb);
    }
}

//============================================================================== Overdrive
void Overdrive::prepare (double sampleRate, int)
{
    sr = sampleRate;
    clipHp.setup (Biquad::Type::HighPass, sr, 720.0, 0.5);
    clipLp.setup (Biquad::Type::LowPass, sr, 5600.0, 0.6);
    inHp.setup (Biquad::Type::HighPass, sr, 30.0, 0.7);
    toneShelf.setup (Biquad::Type::HighPass, sr, 110.0, 0.6); // the "tightening" low cut
    lastTone = -1;
    reset();
}

void Overdrive::reset() { clipHp.reset(); clipLp.reset(); inHp.reset(); toneLp.reset(); toneShelf.reset(); }

void Overdrive::process (float* data, int n, const OverdriveParams& p)
{
    if (p.tone != lastTone)
    {
        lastTone = p.tone;
        toneLp.setup (Biquad::Type::LowPass, sr, 600.0 * std::pow (10.0, p.tone / 10.0), 0.6);
    }

    const float pot   = p.drive / 10.0f;
    const float gain  = (51.0f + 500.0f * pot * pot) / 4.7f; // TS-style feedback gain (audio-taper pot)
    const float th    = 0.35f;
    const float level = dbToGain ((p.level - 6.0f) * 3.0f);

    for (int i = 0; i < n; ++i)
    {
        const float x  = inHp.process (data[i]);
        const float v  = clipLp.process (clipHp.process (x)) * gain;
        const float y  = x + th * std::tanh (v / th);
        data[i] = toneShelf.process (toneLp.process (y)) * level;
    }
}

//============================================================================== GraphicEQ
void GraphicEQ::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (int b = 0; b < GraphicEQParams::numBands; ++b)
    {
        cached[b] = 1000.0f; // force recompute
        bands[b].reset();
    }
}

void GraphicEQ::reset() { for (auto& b : bands) b.reset(); }

void GraphicEQ::process (float* data, int n, const GraphicEQParams& p)
{
    for (int b = 0; b < GraphicEQParams::numBands; ++b)
    {
        if (p.gainsDb[b] != cached[b])
        {
            cached[b] = p.gainsDb[b];
            bands[b].setup (Biquad::Type::Peak, sr, GraphicEQParams::freqs[b], 1.4, p.gainsDb[b]);
        }
    }

    const float level = dbToGain (p.levelDb);
    for (int i = 0; i < n; ++i)
    {
        float x = data[i];
        for (int b = 0; b < GraphicEQParams::numBands; ++b)
            if (cached[b] != 0.0f)
                x = bands[b].process (x);
        data[i] = x * level;
    }
}

} // namespace abyssal
