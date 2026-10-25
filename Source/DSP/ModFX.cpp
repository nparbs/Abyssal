#include "ModFX.h"

namespace abyssal
{

//============================================================================== Wah
void Wah::prepare (double sampleRate)
{
    sr = sampleRate;
    envAttack  = (float) std::exp (-1.0 / (0.004 * sr));
    envRelease = (float) std::exp (-1.0 / (0.12 * sr));
    posSmooth  = (float) std::exp (-1.0 / (0.02 * sr));
    reset();
}

void Wah::reset() { ic1 = ic2 = 0; env = 0; }

void Wah::process (float* data, int n, const WahParams& p) noexcept
{
    const float q = 2.0f + 8.0f * p.resonance;
    const float k = 1.0f / q;
    float a1 = 0, a2 = 0, a3 = 0;

    for (int i = 0; i < n; ++i)
    {
        const float x = data[i];

        float target = p.position;
        if (p.autoMode)
        {
            const float a = std::abs (x);
            env = a + (env - a) * (a > env ? envAttack : envRelease);
            target = juce::jlimit (0.0f, 1.0f, (gainToDb (env) + 45.0f - 30.0f * (1.0f - p.sensitivity)) / 35.0f);
        }
        smoothedPos = target + (smoothedPos - target) * posSmooth;

        if ((i & 15) == 0) // the sweep is slow next to the audio rate: update the filter every 16 samples
        {
            const double fc = 350.0 * std::pow (2200.0 / 350.0, (double) smoothedPos);
            const float g = (float) std::tan (juce::MathConstants<double>::pi * std::min (fc, sr * 0.45) / sr);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        // Topology-preserving state-variable filter (stays stable while it sweeps).
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        // k * v1 is the unity-peak band-pass; more resonance means a stronger (louder) peak, as on the pedal.
        data[i] = k * v1 * (1.2f + 1.3f * p.resonance) + 0.2f * v2;
    }
}

//============================================================================== Whammy
void Whammy::prepare (double sampleRate)
{
    shifter.prepare (sampleRate, 30.0);
    semis.reset (sampleRate, 0.03); // a little glide, like the treadle's travel
    reset();
}

void Whammy::reset()
{
    shifter.reset();
    semis.setCurrentAndTargetValue (semis.getTargetValue());
}

void Whammy::process (float* data, int n, const WhammyParams& p) noexcept
{
    semis.setTargetValue (WhammyParams::intervalSemis[juce::jlimit (0, 7, p.interval)] * juce::jlimit (0.0f, 1.0f, p.position));
    for (int i = 0; i < n; ++i)
    {
        if ((i & 15) == 0 || semis.isSmoothing())
            shifter.setRatio (std::pow (2.0f, semis.getNextValue() / 12.0f));
        const float x = data[i];
        data[i] = x + p.mix * (shifter.process (x) - x);
    }
}

//============================================================================== Phaser
void Phaser::prepare (double sampleRate, float lfoOffset)
{
    sr = sampleRate;
    offset = lfoOffset;
    reset();
}

void Phaser::reset()
{
    phase = 0;
    fb = 0;
    std::fill (std::begin (state), std::end (state), 0.0f);
}

void Phaser::process (float* data, int n, const PhaserParams& p) noexcept
{
    const int stages = p.stages == 0 ? 4 : 8;
    const float inc = (float) (p.rate / sr);
    float a = 0;

    for (int i = 0; i < n; ++i)
    {
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;

        if ((i & 7) == 0)
        {
            // Sweep 200 Hz .. 3.2 kHz (log), narrowed towards the middle by Depth.
            const float lfo = 0.5f + 0.5f * std::sin (juce::MathConstants<float>::twoPi * (phase + offset));
            const float sweep = 0.5f + (lfo - 0.5f) * p.depth;
            const double fc = 200.0 * std::pow (16.0, (double) sweep);
            const double t = std::tan (juce::MathConstants<double>::pi * fc / sr);
            a = (float) ((t - 1.0) / (t + 1.0));
        }

        const float x = data[i];
        float y = x + p.feedback * fb;
        for (int s = 0; s < stages; ++s)
        {
            const float out = a * y + state[s];
            state[s] = y - a * out;
            y = out;
        }
        fb = y;
        data[i] = x * (1.0f - 0.5f * p.mix) + y * 0.5f * p.mix;
    }
}

//============================================================================== Tremolo
void Tremolo::prepare (double sampleRate, bool rightChannel)
{
    sr = sampleRate;
    right = rightChannel;
    depthSmooth.reset (sr, 0.02);
    reset();
}

void Tremolo::reset() { phase = 0; }

void Tremolo::process (float* data, int n, const TremoloParams& p, double bpm) noexcept
{
    double hz = p.rate;
    if (p.sync && bpm > 1.0)
        hz = bpm / 60.0 / DelayDivisions::beats[juce::jlimit (0, DelayDivisions::count - 1, p.division)];
    const double inc = hz / sr;
    const float hardness = 1.0f + 8.0f * p.shape;
    const float norm = 1.0f / std::tanh (hardness);
    const double offset = right && p.pan ? 0.5 : 0.0; // auto-pan: the sides take turns
    depthSmooth.setTargetValue (p.depth);

    for (int i = 0; i < n; ++i)
    {
        phase += inc;
        if (phase >= 1.0) phase -= 1.0;
        const float s = (float) std::sin (juce::MathConstants<double>::twoPi * (phase + offset));
        const float w = std::tanh (hardness * s) * norm;       // sine morphing into a square
        data[i] *= 1.0f - depthSmooth.getNextValue() * (0.5f - 0.5f * w);
    }
}

//============================================================================== Doubler
void Doubler::prepare (double sampleRate)
{
    sr = sampleRate;
    for (auto& l : line) l.prepare ((int) (0.08 * sr));
    reset();
}

void Doubler::reset()
{
    for (auto& l : line) l.reset();
    std::fill (std::begin (phase), std::end (phase), 0.0);
    drift[0] = drift[1] = driftTarget[0] = driftTarget[1] = 0;
    driftCountdown = 0;
}

void Doubler::process (float* l, float* r, int n, const DoublerParams& p) noexcept
{
    const float ms = (float) (0.001 * sr);
    const float k = 0.42f * p.width;
    // The blend of two decorrelated signals loses level; make it up so the doubler doesn't sound quieter.
    const float makeup = 1.0f / std::sqrt ((1.0f - k) * (1.0f - k) + k * k);
    const double rates[4] = { 0.31, 0.23, 1.7, 2.3 };
    const float driftSmooth = (float) std::exp (-1.0 / (0.25 * sr));
    float* ch[] = { l, r };

    for (int i = 0; i < n; ++i)
    {
        // A real second take drifts against the first: random wander plus slow and quicker wobble.
        if (--driftCountdown <= 0)
        {
            driftCountdown = (int) (0.3 * sr);
            for (auto& t : driftTarget) t = random.nextFloat() * 2.0f - 1.0f;
        }
        for (int j = 0; j < 4; ++j)
        {
            phase[j] += rates[j] / sr;
            if (phase[j] >= 1.0) phase[j] -= 1.0;
        }

        for (int c = 0; c < 2; ++c)
        {
            drift[c] = driftTarget[c] + (drift[c] - driftTarget[c]) * driftSmooth;
            const float wobble = 0.7f * (float) std::sin (juce::MathConstants<double>::twoPi * phase[c])
                               + 0.3f * (float) std::sin (juce::MathConstants<double>::twoPi * phase[c + 2]);
            const float base = p.timeMs * (c == 0 ? 1.0f : 1.37f);
            const float d = (base + p.detune * (1.2f * drift[c] + 0.5f * wobble)) * ms;

            const float x = ch[c][i];
            line[c].push (x);
            ch[c][i] = (x * (1.0f - k) + line[c].read (std::max (1.0f, d)) * k) * makeup;
        }
    }
}

} // namespace abyssal
