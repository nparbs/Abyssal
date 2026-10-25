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
    lastMode = -1;
    delaySamples.reset (sr, 0.12);
    delaySamples.setCurrentAndTargetValue ((float) (0.38 * sr));
    reset();
}

void StereoDelay::reset()
{
    for (int c = 0; c < 2; ++c) { line[c].reset(); lp[c].reset(); hp[c].reset(); colour[c].reset(); }
    wowPhase = flutterPhase = 0;
}

void StereoDelay::updateModeFilters (int mode, float toneHz)
{
    for (auto& f : colour)
    {
        if (mode == 1)      f.setup (Biquad::Type::LowPass, sr, 7500.0, 0.6);                    // tape head roll-off
        else if (mode == 2) f.setup (Biquad::Type::LowPass, sr, std::min (2800.0f, toneHz), 0.75); // BBD anti-alias filter
        else                f.setIdentity();
    }
}

void StereoDelay::process (float* l, float* r, int n, const DelayParams& p, double bpm)
{
    if (p.toneHz != lastTone || p.mode != lastMode)
    {
        lastTone = p.toneHz;
        lastMode = p.mode;
        for (auto& f : lp) f.setup (Biquad::Type::LowPass, sr, p.toneHz, 0.6);
        updateModeFilters (p.mode, p.toneHz);
    }

    float ms = p.timeMs;
    if (p.sync && bpm > 1.0)
        ms = (float) (60000.0 / bpm * DelayDivisions::beats[juce::jlimit (0, DelayDivisions::count - 1, p.division)]);
    delaySamples.setTargetValue (juce::jlimit (1.0f, (float) (2.5 * sr), ms * 0.001f * (float) sr));

    // Tape: slow wow plus faster flutter on the read head. Analog: a gentle clock wobble.
    const double wowInc = 0.55 / sr, flutterInc = 6.3 / sr;
    const float wowDepth = p.mode == 1 ? 0.0028f : p.mode == 2 ? 0.0012f : 0.0f;
    const float flutterDepth = p.mode == 1 ? 0.0005f : 0.0f;
    // Each pass through the loop saturates a little (tape compression / BBD headroom).
    const float drive = p.mode == 1 ? 1.5f : p.mode == 2 ? 1.25f : 0.0f;
    auto saturate = [drive] (float x) { return drive > 0.0f ? std::tanh (drive * x) / drive : x; };

    for (int i = 0; i < n; ++i)
    {
        float d = delaySamples.getNextValue();
        if (p.mode != 0)
        {
            wowPhase += wowInc;
            flutterPhase += flutterInc;
            if (wowPhase >= 1.0) wowPhase -= 1.0;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;
            d *= 1.0f + wowDepth * (float) std::sin (juce::MathConstants<double>::twoPi * wowPhase)
                      + flutterDepth * (float) std::sin (juce::MathConstants<double>::twoPi * flutterPhase);
        }
        const float tl = saturate (colour[0].process (lp[0].process (hp[0].process (line[0].read (d)))));
        const float tr = saturate (colour[1].process (lp[1].process (hp[1].process (line[1].read (d)))));

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

//============================================================================== PlateReverb
void PlateReverb::prepare (double sampleRate)
{
    sr = sampleRate;
    const int diff[4] = { 142, 107, 379, 277 };
    for (int i = 0; i < 4; ++i)
    {
        diffuserLen[i] = (float) scaled (diff[i]);
        diffuser[i].prepare ((int) diffuserLen[i] + 2);
    }
    const int a1[2] = { 672, 908 }, d1[2] = { 4453, 4217 }, a2[2] = { 1800, 2656 }, d2[2] = { 3720, 3163 };
    modDepth = (float) (16.0 * sr / 29761.0);
    for (int c = 0; c < 2; ++c)
    {
        ap1Len[c] = (float) scaled (a1[c]);
        delay1Len[c] = (float) scaled (d1[c]);
        ap2Len[c] = (float) scaled (a2[c]);
        delay2Len[c] = (float) scaled (d2[c]);
        ap1[c].prepare ((int) (ap1Len[c] + modDepth) + 4);
        delay1[c].prepare ((int) delay1Len[c] + 2);
        ap2[c].prepare ((int) ap2Len[c] + 2);
        delay2[c].prepare ((int) delay2Len[c] + 2);
    }
    reset();
}

void PlateReverb::reset()
{
    for (auto& d : diffuser) d.clear();
    for (int c = 0; c < 2; ++c) { ap1[c].clear(); delay1[c].clear(); ap2[c].clear(); delay2[c].clear(); }
    bandwidthState = 0;
    dampState[0] = dampState[1] = tankOut[0] = tankOut[1] = 0;
    modPhase = 0;
}

void PlateReverb::process (const float* inL, const float* inR, float* outL, float* outR, int n,
                           float size, float damping, float width) noexcept
{
    const float decay = 0.2f + 0.65f * size; // up to roughly an 8 s tail
    const float damp = 0.05f + 0.6f * damping;
    const float bandwidth = 0.9995f;
    const float gain = 0.6f;
    auto t = [this] (int samplesAt29761) { return scaled (samplesAt29761); };

    for (int i = 0; i < n; ++i)
    {
        // Input: mono sum, a touch of band-limiting, four diffusers.
        float x = 0.5f * (inL[i] + inR[i]);
        bandwidthState += bandwidth * (x - bandwidthState);
        x = bandwidthState;
        x = allpass (diffuser[0], diffuserLen[0], 0.75f, x);
        x = allpass (diffuser[1], diffuserLen[1], 0.75f, x);
        x = allpass (diffuser[2], diffuserLen[2], 0.625f, x);
        x = allpass (diffuser[3], diffuserLen[3], 0.625f, x);

        modPhase += 1.0 / sr;
        if (modPhase >= 1.0) modPhase -= 1.0;
        const float mod[2] = { modDepth * (float) std::sin (juce::MathConstants<double>::twoPi * modPhase),
                               modDepth * (float) std::cos (juce::MathConstants<double>::twoPi * modPhase) };

        // The figure-of-eight tank: each branch is fed by the input plus the other branch's output.
        float branchOut[2];
        for (int c = 0; c < 2; ++c)
        {
            float v = x + decay * tankOut[1 - c];
            v = allpass (ap1[c], ap1Len[c] + mod[c], -0.7f, v);
            delay1[c].push (v);
            v = delay1[c].tap ((int) delay1Len[c]);
            dampState[c] += (1.0f - damp) * (v - dampState[c]);
            v = dampState[c] * decay;
            v = allpass (ap2[c], ap2Len[c], 0.5f, v);
            delay2[c].push (v);
            branchOut[c] = delay2[c].tap ((int) delay2Len[c]);
        }
        tankOut[0] = branchOut[0];
        tankOut[1] = branchOut[1];

        // Output taps from both branches (Dattorro's table), so each side hears the whole tank.
        const float yl = delay1[1].tap (t (266)) + delay1[1].tap (t (2974)) - ap2[1].tap (t (1913)) + delay2[1].tap (t (1996))
                       - delay1[0].tap (t (1990)) - ap2[0].tap (t (187)) - delay2[0].tap (t (1066));
        const float yr = delay1[0].tap (t (353)) + delay1[0].tap (t (3627)) - ap2[0].tap (t (1228)) + delay2[0].tap (t (2673))
                       - delay1[1].tap (t (2111)) - ap2[1].tap (t (335)) - delay2[1].tap (t (121));

        const float mid = 0.5f * (yl + yr), side = 0.5f * (yl - yr) * width;
        outL[i] = gain * (mid + side);
        outR[i] = gain * (mid - side);
    }
}

//============================================================================== Reverb
void Reverb::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    reverb.setSampleRate (sr);
    plate.prepare (sr);
    shimmerShift.prepare (sr, 20.0);
    shimmerShift.setRatio (2.0f);
    shimmerHp.setup (Biquad::Type::HighPass, sr, 300.0, 0.7);
    shimmerLp.setup (Biquad::Type::LowPass, sr, 7000.0, 0.7);
    wet.setSize (2, maxBlock);
    shimmerBuf.setSize (1, maxBlock);
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
    plate.reset();
    shimmerShift.reset();
    shimmerHp.reset();
    shimmerLp.reset();
    shimmerBuf.clear();
    for (int c = 0; c < 2; ++c) { pre[c].reset(); lowCut[c].reset(); }
}

void Reverb::process (float* l, float* r, int n, const ReverbParams& p)
{
    juce::Reverb::Parameters rp;
    // The shimmer loop adds its own sustain, so its hall is kept short of the near-infinite settings.
    rp.roomSize = p.mode == 2 ? p.size * 0.8f : p.size;
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

    if (p.mode != lastMode)
    {
        // Switching algorithms: start the new one from silence rather than a stale tail.
        lastMode = p.mode;
        reverb.reset();
        plate.reset();
        shimmerBuf.clear();
    }

    if (p.mode == 1)
    {
        plate.process (wl, wr, wl, wr, n, p.size, p.damping, p.width);
    }
    else
    {
        // Shimmer: the previous block's tail, an octave up, goes back in with the dry signal.
        float* sh = shimmerBuf.getWritePointer (0);
        if (p.mode == 2)
            for (int i = 0; i < n; ++i)
            {
                wl[i] += sh[i];
                wr[i] += sh[i];
            }

        reverb.processStereo (wl, wr, n);

        if (p.mode == 2)
        {
            // Loop gain stays well below 1 even with a long tail, so the octaves bloom and then fade.
            const float amount = 0.35f * p.shimmer * (1.0f - 0.4f * p.size);
            for (int i = 0; i < n; ++i)
            {
                const float up = shimmerLp.process (shimmerHp.process (shimmerShift.process (0.5f * (wl[i] + wr[i]))));
                sh[i] = std::tanh (amount * up); // soft limit keeps the loop from running away
            }
        }
    }

    for (int i = 0; i < n; ++i)
    {
        // Low cut on the tail keeps palm mutes from turning into mud.
        l[i] = l[i] * (1.0f - 0.3f * p.mix) + lowCut[0].process (wl[i]) * p.mix * 0.6f;
        r[i] = r[i] * (1.0f - 0.3f * p.mix) + lowCut[1].process (wr[i]) * p.mix * 0.6f;
    }
}

} // namespace abyssal
