#include "AmpModel.h"

namespace abyssal
{

// Asymmetric triode transfer curve: smooth grid-conduction side, softer cutoff side.
static inline float triode (float x) noexcept
{
    return x > 0.0f ? std::tanh (x) : x / (1.0f - 0.5f * x);
}

const AmpModel::Voicing& AmpModel::getVoicing (int channel)
{
    // Tone stack parts: C1 (treble), C2 (bass), C3 (mid), treble / bass / mid pots, slope resistor.
    static const Voicing voicings[numChannels] =
    {
        // Clean: two gentle stages, wide bandwidth, lots of headroom. Bassman-style stack.
        { 2, -8.0f, 18.0f, { 1, 1.8f, 1, 1 }, { 0.20f, -0.10f, 0, 0 },
          { 20, 25, 20, 20 }, { 16000, 12000, 12000, 12000 },
          40, 90, 800, 0.0f, 9500,
          { 250e-12, 20e-9, 20e-9, 250e3, 1e6, 25e3, 56e3 },
          0.25f, 0.6f, 6.0f },

        // Crunch: three stages, British-style midrange push. Plexi / 800-style stack.
        { 3, 0.0f, 30.0f, { 1, 4.0f, 2.5f, 1 }, { 0.12f, -0.18f, 0.15f, 0 },
          { 20, 80, 45, 20 }, { 13000, 8500, 7000, 7000 },
          45, 120, 750, 3.0f, 7000,
          { 470e-12, 22e-9, 22e-9, 220e3, 1e6, 22e3, 33e3 },
          0.35f, 0.9f, -1.0f },

        // Lead: four cascaded stages, tight interstage coupling, modern metal voicing.
        { 4, 6.0f, 38.0f, { 1, 6.0f, 4.0f, 2.2f }, { 0.10f, -0.15f, 0.20f, -0.10f },
          { 20, 170, 90, 40 }, { 12000, 9000, 7200, 6400 },
          55, 150, 900, 4.0f, 6000,
          { 330e-12, 22e-9, 22e-9, 250e3, 1e6, 25e3, 39e3 },
          0.45f, 1.2f, -3.5f },

        // Abyss: tight, saturated modern lead. More first-stage gain, steeper interstage low cuts so
        // the extra gain doesn't turn to mud, an upper-mid push into the clipper, and a small bass
        // cap so the Bass knob works on the punch (~100 Hz) instead of the boom.
        { 4, 10.0f, 42.0f, { 1, 7.0f, 5.0f, 3.0f }, { 0.08f, -0.12f, 0.18f, -0.12f },
          { 20, 230, 130, 70 }, { 11000, 8200, 6600, 5800 },
          70, 185, 1000, 5.0f, 5600,
          { 470e-12, 15e-9, 22e-9, 250e3, 1e6, 25e3, 47e3 },
          0.3f, 1.15f, -4.0f },
    };

    return voicings[juce::jlimit (0, numChannels - 1, channel)];
}

void AmpModel::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    osr = sr * (double) oversampling->getOversamplingFactor();
    latency = (int) std::round (oversampling->getLatencyInSamples());

    stage1Gain.reset (osr, 0.03);
    powerGain.reset (osr, 0.03);
    outGain.reset (osr, 0.03);

    sagAttack  = (float) std::exp (-1.0 / (0.004 * osr));
    sagRelease = (float) std::exp (-1.0 / (0.120 * osr));

    dcBlock.setup (Biquad::Type::HighPass, osr, 12.0, 0.7);

    needsUpdate = true;
    updateFilters (current);
    stage1Gain.setCurrentAndTargetValue (stage1Gain.getTargetValue());
    powerGain.setCurrentAndTargetValue (powerGain.getTargetValue());
    outGain.setCurrentAndTargetValue (outGain.getTargetValue());
    reset();
}

void AmpModel::reset()
{
    if (oversampling) oversampling->reset();
    for (auto* b : { &inputHp, &brightShelf, &preMid, &fizzLp, &fizzLp2, &depth, &presence, &powerVoiceA, &powerVoiceB, &dcBlock })
        b->reset();
    toneStack.reset();
    for (int i = 0; i < maxStages; ++i) { stageHp[i].reset(); stageLp[i].reset(); }
    sagEnv = 0;
}

void AmpModel::updateFilters (const AmpParams& p)
{
    if (! needsUpdate && p == current)
        return;

    const bool channelChanged = needsUpdate || p.channel != current.channel;
    const bool powerChanged = needsUpdate || p.power != current.power;
    current = p;
    needsUpdate = false;

    const auto& v = getVoicing (p.channel);
    using T = Biquad::Type;

    inputHp.setup (T::HighPass, osr, p.tight ? v.tightHz : v.looseHz, 0.7);

    // Bright cap: strong at low gain, mostly swamped at high gain like the real thing.
    const float brightDb = p.bright ? 7.0f * (1.0f - 0.6f * p.gain / 10.0f) : 0.0f;
    brightShelf.setup (T::HighShelf, osr, 1600.0, 0.7, brightDb);
    preMid.setup (T::Peak, osr, v.preMidHz, 0.8, v.preMidDb);

    for (int i = 0; i < maxStages; ++i)
    {
        stageHp[i].setup (T::HighPass, osr, v.hpHz[i], 0.5);
        stageLp[i].setup (T::LowPass, osr, v.lpHz[i], 0.6);
    }
    fizzLp.setup (T::LowPass, osr, v.fizzHz, 0.6);
    fizzLp2.setup (T::LowPass, osr, v.fizzHz * 1.6, 0.7);

    // Passive tone stack: the controls interact like the real circuit. The makeup gain is fixed per
    // channel (unity-ish with every knob at 5), so turning the knobs changes the level like an amp does.
    toneStack.setup (v.stack, osr, p.treble, p.mid, p.bass);
    if (channelChanged)
        stackMakeup = dbToGain (ToneStack::noonMakeupDb (v.stack));

    // Power amp negative-feedback controls.
    depth.setup    (T::Peak, osr, 95.0, 1.1, p.depth * 1.0f);
    presence.setup (T::HighShelf, osr, 3600.0, 0.7, p.presence * 1.2f - 3.0f);

    // Power tubes: EL34s push the upper mids and break up earlier; 6L6s are rounder in the mids,
    // firmer in the lows and cleaner for longer.
    if (p.power == 0)
    {
        powerVoiceA.setup (T::Peak, osr, 1800.0, 0.8, 2.0);
        powerVoiceB.setIdentity();
    }
    else
    {
        powerVoiceA.setup (T::LowShelf, osr, 120.0, 0.7, 1.5);
        powerVoiceB.setup (T::Peak, osr, 650.0, 0.9, -1.5);
    }

    const float gainDb = v.minGainDb + (v.maxGainDb - v.minGainDb) * (p.gain / 10.0f);
    stage1Gain.setTargetValue (dbToGain (gainDb));
    powerGain.setTargetValue (v.powerDrive * (p.power == 0 ? 1.0f : 0.75f) * dbToGain ((p.master - 5.0f) * 2.4f));
    // Level compensation so the master behaves like volume without huge jumps between channels.
    // 6L6s are driven less hard, so they come out a little quieter; make up for it.
    outGain.setTargetValue (dbToGain (v.outTrimDb + (p.master - 5.0f) * 1.2f + (p.power == 0 ? 0.0f : 2.2f)));

    if (channelChanged)
    {
        for (int i = 0; i < maxStages; ++i) { stageHp[i].reset(); stageLp[i].reset(); }
        toneStack.reset();
        stage1Gain.setCurrentAndTargetValue (stage1Gain.getTargetValue());
    }
    if (powerChanged)
    {
        powerVoiceA.reset();
        powerVoiceB.reset();
    }
}

void AmpModel::process (float* data, int n, const AmpParams& p)
{
    updateFilters (p);
    const auto& v = getVoicing (current.channel);

    float* channels[] = { data };
    juce::dsp::AudioBlock<float> block (channels, 1, (size_t) n);
    auto up = oversampling->processSamplesUp (block);
    float* x = up.getChannelPointer (0);
    const int on = (int) up.getNumSamples();

    float biasOffset[maxStages];
    for (int s = 0; s < maxStages; ++s)
        biasOffset[s] = triode (v.bias[s]);
    const float powerBias = current.power == 0 ? 0.08f : 0.0f;
    const float powerBiasOffset = std::tanh (powerBias);
    const float sag = v.sag * (current.power == 0 ? 1.0f : 0.8f);

    for (int i = 0; i < on; ++i)
    {
        float s = x[i];

        // ---- preamp
        s = inputHp.process (s);
        s = brightShelf.process (s);
        s = preMid.process (s);
        s *= stage1Gain.getNextValue();

        for (int st = 0; st < v.stages; ++st)
        {
            s = stageHp[st].process (s * v.stageGain[st]);
            s = triode (s + v.bias[st]) - biasOffset[st];
            s = stageLp[st].process (s);
        }

        s = fizzLp2.process (fizzLp.process (s));

        // ---- tone stack
        s = toneStack.process (s) * stackMakeup;

        // ---- power amp with supply sag
        s = powerVoiceB.process (powerVoiceA.process (presence.process (depth.process (s))));
        const float drive = powerGain.getNextValue();
        const float level = std::abs (s * drive);
        sagEnv = level + (sagEnv - level) * (level > sagEnv ? sagAttack : sagRelease);
        const float sagGain = 1.0f / (1.0f + sag * sagEnv);
        // EL34s clip a little asymmetrically (more even harmonics); 6L6s stay symmetric.
        const float driven = s * drive * sagGain + powerBias;
        s = (std::tanh (driven) - powerBiasOffset) * (1.0f + 0.5f * sag);

        x[i] = dcBlock.process (s) * outGain.getNextValue();
    }

    oversampling->processSamplesDown (block);
}

} // namespace abyssal
