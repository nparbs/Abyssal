#include "Cabinet.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace abyssal
{

juce::StringArray Cabinet::getSourceNames()
{
    return { "Modern 4x12 V", "British 4x12 G", "Open Back 2x12", "User IR" };
}

juce::AudioBuffer<float> Cabinet::makeBuiltInIR (int type, double sampleRate)
{
    const int len = (int) (0.085 * sampleRate);
    juce::AudioBuffer<float> ir (1, len);
    ir.clear();
    float* d = ir.getWritePointer (0);

    // Excitation: impulse plus (for the open back) early reflections and a short diffuse tail.
    d[0] = 1.0f;
    if (type == 2)
    {
        auto at = [&] (double ms) { return juce::jlimit (1, len - 1, (int) (ms * 0.001 * sampleRate)); };
        d[at (1.1)] -= 0.35f;
        d[at (2.7)] += 0.2f;
        d[at (4.3)] -= 0.1f;
        juce::Random rng (1234);
        const int tailLen = at (22.0);
        for (int i = at (0.6); i < tailLen; ++i)
            d[i] += 0.012f * (rng.nextFloat() * 2.0f - 1.0f) * std::exp (-(float) i / (float) (0.006 * sampleRate));
    }

    struct Stage { Biquad::Type t; double f, q, g; };
    using T = Biquad::Type;
    std::vector<Stage> chain;

    switch (type)
    {
        case 0: // Modern closed-back 4x12, V-type speakers: tight lows, aggressive upper mids
            chain = { { T::HighPass, 50, 0.5, 0 }, { T::HighPass, 75, 0.9, 0 },
                      { T::Peak, 110, 1.4, 4 }, { T::Peak, 400, 1.0, -3 }, { T::Peak, 1400, 1.0, 1.5 },
                      { T::Peak, 2600, 1.6, 5 }, { T::Peak, 4200, 2.5, 3 }, { T::Peak, 6800, 4.0, -8 },
                      { T::LowPass, 5200, 0.8, 0 }, { T::LowPass, 6000, 0.6, 0 }, { T::LowPass, 7500, 0.55, 0 } };
            break;
        case 1: // British 4x12, G-type speakers: warmer, smoother top
            chain = { { T::HighPass, 45, 0.5, 0 }, { T::HighPass, 70, 0.8, 0 },
                      { T::Peak, 105, 1.2, 3 }, { T::Peak, 650, 0.8, 1 }, { T::Peak, 1800, 1.2, 3 },
                      { T::Peak, 3200, 2.0, 2 }, { T::Peak, 5600, 3.5, -6 },
                      { T::LowPass, 4400, 0.8, 0 }, { T::LowPass, 5000, 0.6, 0 }, { T::LowPass, 6500, 0.55, 0 } };
            break;
        default: // Open-back 2x12: looser lows, airier top
            chain = { { T::HighPass, 60, 0.5, 0 }, { T::HighPass, 90, 0.7, 0 },
                      { T::Peak, 140, 1.0, 2 }, { T::Peak, 500, 1.0, -1.5 }, { T::Peak, 2200, 1.2, 3 },
                      { T::Peak, 3800, 2.0, 4 }, { T::Peak, 7800, 4.0, -6 },
                      { T::LowPass, 6200, 0.8, 0 }, { T::LowPass, 7000, 0.6, 0 }, { T::LowPass, 9000, 0.55, 0 } };
            break;
    }

    for (const auto& s : chain)
    {
        Biquad b;
        b.setup (s.t, sampleRate, s.f, s.q, s.g);
        b.process (d, len);
    }

    // Half-cosine fade over the last quarter so the IR ends cleanly.
    const int fadeStart = len * 3 / 4;
    for (int i = fadeStart; i < len; ++i)
        d[i] *= 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (float) (i - fadeStart) / (float) (len - fadeStart)));

    return ir;
}

void Cabinet::prepare (double sampleRate, int block)
{
    sr = sampleRate;
    maxBlock = block;
    scratch.setSize (2, block);

    for (int s = 0; s < 2; ++s)
    {
        conv[s].prepare ({ sr, (juce::uint32) block, 1 });
        cachedMic[s] = -1;
        gainL[s].reset (sr, 0.03);
        gainR[s].reset (sr, 0.03);
        loadSource (s); // regenerate built-ins at the new rate
    }
}

void Cabinet::reset()
{
    for (int s = 0; s < 2; ++s)
    {
        conv[s].reset();
        micPresence[s].reset();
        micShelf[s].reset();
    }
}

void Cabinet::setSource (int slot, int source)
{
    if (wantedSource[slot] == source && loadedSource[slot] >= 0)
        return;
    wantedSource[slot] = source;
    loadSource (slot);
}

void Cabinet::loadSource (int slot)
{
    const int src = wantedSource[slot];

    if (src == userSource && userFile[slot].existsAsFile())
    {
        conv[slot].loadImpulseResponse (userFile[slot], juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::yes, 0,
                                        juce::dsp::Convolution::Normalise::yes);
    }
    else
    {
        // "User IR" with no file falls back to the first built-in so the slot is never silent.
        auto ir = makeBuiltInIR (src == userSource ? 0 : src, sr);
        conv[slot].loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::no,
                                        juce::dsp::Convolution::Normalise::yes);
    }
    loadedSource[slot] = src;
}

juce::String Cabinet::loadUserIR (int slot, const juce::File& file)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
        return "Couldn't read " + file.getFileName();
    if (reader->lengthInSamples < 16)
        return "IR file is empty";

    userFile[slot] = file;
    wantedSource[slot] = userSource;
    loadSource (slot);
    return {};
}

void Cabinet::process (const float* mono, float* outL, float* outR, int n, const CabParams& p)
{
    if (! p.on || (! p.slot[0].on && ! p.slot[1].on))
    {
        std::copy (mono, mono + n, outL);
        std::copy (mono, mono + n, outR);
        return;
    }

    std::fill (outL, outL + n, 0.0f);
    std::fill (outR, outR + n, 0.0f);

    for (int s = 0; s < 2; ++s)
    {
        const auto& sp = p.slot[s];
        if (! sp.on)
            continue;

        if (sp.mic != cachedMic[s])
        {
            cachedMic[s] = sp.mic;
            // Mic position: cap (0) is bright and present, edge (1) is darker and rounder.
            micPresence[s].setup (Biquad::Type::Peak, sr, 4500.0, 1.2, (1.0f - sp.mic) * 3.0f - 1.0f);
            micShelf[s].setup (Biquad::Type::HighShelf, sr, 2500.0, 0.6, -sp.mic * 9.0f);
        }

        float* buf = scratch.getWritePointer (s);
        for (int i = 0; i < n; ++i)
            buf[i] = micShelf[s].process (micPresence[s].process (mono[i]));

        float* chans[] = { buf };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        conv[s].process (juce::dsp::ProcessContextReplacing<float> (block));

        // Constant-power pan; a single centred slot keeps unity on both sides.
        const float angle = (sp.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        // Energy-normalised IRs gain ~3 dB per doubling of the sample rate for low-frequency
        // content; compensate so tone/level is the same at 44.1, 48, 96 kHz...
        const float g = dbToGain (sp.levelDb) * std::sqrt (48000.0f / (float) sr);
        gainL[s].setTargetValue (g * std::cos (angle) * juce::MathConstants<float>::sqrt2);
        gainR[s].setTargetValue (g * std::sin (angle) * juce::MathConstants<float>::sqrt2);

        for (int i = 0; i < n; ++i)
        {
            outL[i] += buf[i] * gainL[s].getNextValue();
            outR[i] += buf[i] * gainR[s].getNextValue();
        }
    }
}

} // namespace abyssal
