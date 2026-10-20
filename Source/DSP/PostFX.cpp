#include "PostFX.h"

namespace abyssal
{

//============================================================================== ParamEQ
void ParamEQ::design (const ParamEQParams& p, double sampleRate, Biquad (&f)[8])
{
    using T = Biquad::Type;
    // 4th-order Butterworth cuts (two cascaded biquads) keep the low end tight for metal mixes.
    f[0].setup (T::HighPass, sampleRate, p.hpfHz, 0.5412);
    f[1].setup (T::HighPass, sampleRate, p.hpfHz, 1.3066);
    for (int b = 0; b < ParamEQParams::numBands; ++b)
        f[2 + b].setup (T::Peak, sampleRate, p.band[b].freq, p.band[b].q, p.band[b].gainDb);
    f[6].setup (T::LowPass, sampleRate, p.lpfHz, 0.5412);
    f[7].setup (T::LowPass, sampleRate, p.lpfHz, 1.3066);
}

void ParamEQ::prepare (double sampleRate, int) { sr = sampleRate; dirty = true; reset(); }
void ParamEQ::reset() { for (auto& ch : filters) for (auto& f : ch) f.reset(); }

void ParamEQ::process (float* l, float* r, int n, const ParamEQParams& p)
{
    if (dirty || ! (p == cached))
    {
        cached = p;
        dirty = false;
        design (p, sr, filters[0]);
        design (p, sr, filters[1]);
    }

    for (auto& f : filters[0]) f.process (l, n);
    for (auto& f : filters[1]) f.process (r, n);
}

float ParamEQ::responseDb (const ParamEQParams& p, double freq, double sampleRate)
{
    Biquad f[8];
    design (p, sampleRate, f);
    double mag = 1.0;
    for (auto& b : f) mag *= b.magnitude (freq, sampleRate);
    return (float) (20.0 * std::log10 (std::max (mag, 1.0e-9)));
}

//============================================================================== ModDelay
void ModDelay::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        line[c].prepare ((int) (0.04 * sr));
        fbHp[c].setup (Biquad::Type::HighPass, sr, 120.0, 0.7);
    }
    reset();
}

void ModDelay::reset()
{
    for (int c = 0; c < 2; ++c) { line[c].reset(); fbState[c] = 0; fbHp[c].reset(); }
    lfoPhase = 0;
}

void ModDelay::process (float* l, float* r, int n, const ModParams& p)
{
    const float inc = (float) (p.rate / sr);
    const float msToSamples = (float) (sr * 0.001);
    const float fb = mode == Mode::Flanger ? p.feedback : 0.0f;
    float* ch[] = { l, r };

    for (int i = 0; i < n; ++i)
    {
        lfoPhase += inc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;

        for (int c = 0; c < 2; ++c)
        {
            // Right channel LFO runs 90 degrees ahead for stereo width.
            const float ph  = lfoPhase + (c == 1 ? 0.25f : 0.0f);
            const float lfo = std::sin (juce::MathConstants<float>::twoPi * ph);

            float delayMs;
            if (mode == Mode::Chorus) delayMs = 9.0f + p.depth * 6.0f * lfo;
            else                      delayMs = 0.4f + p.depth * 4.5f * (0.5f + 0.5f * lfo);

            const float dry = ch[c][i];
            const float wet = line[c].read (delayMs * msToSamples);
            line[c].push (dry + fb * fbHp[c].process (wet));
            ch[c][i] = mode == Mode::Chorus ? dry * (1.0f - 0.5f * p.mix) + wet * p.mix
                                            : dry * (1.0f - 0.5f * p.mix) + wet * p.mix * 0.7f;
        }
    }
}

//============================================================================== StereoDelay
void StereoDelay::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        line[c].prepare ((int) (2.6 * sr));
        hp[c].setup (Biquad::Type::HighPass, sr, 110.0, 0.7);
    }
    lastTone = -1;
    delaySamples.reset (sr, 0.12);
    delaySamples.setCurrentAndTargetValue ((float) (0.38 * sr));
    reset();
}

void StereoDelay::reset()
{
    for (int c = 0; c < 2; ++c) { line[c].reset(); lp[c].reset(); hp[c].reset(); }
}

void StereoDelay::process (float* l, float* r, int n, const DelayParams& p, double bpm)
{
    if (p.toneHz != lastTone)
    {
        lastTone = p.toneHz;
        for (auto& f : lp) f.setup (Biquad::Type::LowPass, sr, p.toneHz, 0.6);
    }

    float ms = p.timeMs;
    if (p.sync && bpm > 1.0)
        ms = (float) (60000.0 / bpm * DelayParams::divisionBeats[juce::jlimit (0, 7, p.division)]);
    delaySamples.setTargetValue (juce::jlimit (1.0f, (float) (2.5 * sr), ms * 0.001f * (float) sr));

    for (int i = 0; i < n; ++i)
    {
        const float d  = delaySamples.getNextValue();
        const float tl = lp[0].process (hp[0].process (line[0].read (d)));
        const float tr = lp[1].process (hp[1].process (line[1].read (d)));

        if (p.pingPong)
        {
            // Mono input enters the left line; feedback crosses sides so repeats bounce L/R.
            line[0].push (0.5f * (l[i] + r[i]) + p.feedback * tr);
            line[1].push (p.feedback * tl);
        }
        else
        {
            line[0].push (l[i] + p.feedback * tl);
            line[1].push (r[i] + p.feedback * tr);
        }

        l[i] += tl * p.mix;
        r[i] += tr * p.mix;
    }
}

//============================================================================== Reverb
void Reverb::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    reverb.setSampleRate (sr);
    wet.setSize (2, maxBlock);
    for (int c = 0; c < 2; ++c)
    {
        pre[c].prepare ((int) (0.25 * sr));
        lowCut[c].setup (Biquad::Type::HighPass, sr, 180.0, 0.7);
    }
    reset();
}

void Reverb::reset()
{
    reverb.reset();
    for (int c = 0; c < 2; ++c) { pre[c].reset(); lowCut[c].reset(); }
}

void Reverb::process (float* l, float* r, int n, const ReverbParams& p)
{
    juce::Reverb::Parameters rp;
    rp.roomSize = p.size;
    rp.damping = p.damping;
    rp.width = p.width;
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    reverb.setParameters (rp);

    float* wl = wet.getWritePointer (0);
    float* wr = wet.getWritePointer (1);
    const float preSamples = p.predelayMs * 0.001f * (float) sr;

    for (int i = 0; i < n; ++i)
    {
        pre[0].push (l[i]);
        pre[1].push (r[i]);
        wl[i] = preSamples < 1.0f ? l[i] : pre[0].read (preSamples);
        wr[i] = preSamples < 1.0f ? r[i] : pre[1].read (preSamples);
    }

    reverb.processStereo (wl, wr, n);

    for (int i = 0; i < n; ++i)
    {
        // Low cut on the tail keeps palm mutes from turning into mud.
        l[i] = l[i] * (1.0f - 0.3f * p.mix) + lowCut[0].process (wl[i]) * p.mix * 0.6f;
        r[i] = r[i] * (1.0f - 0.3f * p.mix) + lowCut[1].process (wr[i]) * p.mix * 0.6f;
    }
}

} // namespace abyssal
