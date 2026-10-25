#include "Cabinet.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace abyssal
{

juce::StringArray Cabinet::getSourceNames()
{
    return { "Modern 4x12 V", "British 4x12 G", "Open Back 2x12", "Oversized 4x12 C", "Vintage 4x12 25", "Closed 2x12 V",
             "User IR" };
}

juce::StringArray Cabinet::getMicNames()
{
    return { "Dynamic", "Ribbon", "Condenser" };
}

int Cabinet::findOnset (const juce::AudioBuffer<float>& ir)
{
    if (ir.getNumSamples() == 0)
        return 0;
    const float* d = ir.getReadPointer (0);
    float peak = 0.0f;
    for (int i = 0; i < ir.getNumSamples(); ++i)
        peak = std::max (peak, std::abs (d[i]));
    for (int i = 0; i < ir.getNumSamples(); ++i)
        if (std::abs (d[i]) >= 0.1f * peak)
            return i;
    return 0;
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
        case 2: // Open-back 2x12: looser lows, airier top
            chain = { { T::HighPass, 60, 0.5, 0 }, { T::HighPass, 90, 0.7, 0 },
                      { T::Peak, 140, 1.0, 2 }, { T::Peak, 500, 1.0, -1.5 }, { T::Peak, 2200, 1.2, 3 },
                      { T::Peak, 3800, 2.0, 4 }, { T::Peak, 7800, 4.0, -6 },
                      { T::LowPass, 6200, 0.8, 0 }, { T::LowPass, 7000, 0.6, 0 }, { T::LowPass, 9000, 0.55, 0 } };
            break;
        case 3: // Oversized closed 4x12, C-type speakers: deep low-end thump, scooped low mids
            chain = { { T::HighPass, 45, 0.5, 0 }, { T::HighPass, 65, 0.9, 0 },
                      { T::Peak, 90, 1.3, 5 }, { T::Peak, 250, 1.0, 1 }, { T::Peak, 600, 0.9, -4 }, { T::Peak, 1600, 1.2, 1 },
                      { T::Peak, 3000, 1.4, 4.5 }, { T::Peak, 5000, 3.0, 2 }, { T::Peak, 7200, 4.0, -9 },
                      { T::LowPass, 5000, 0.8, 0 }, { T::LowPass, 5800, 0.6, 0 }, { T::LowPass, 7200, 0.55, 0 } };
            break;
        case 4: // Vintage 4x12 with low-power 25 W speakers: mid hump, early top-end roll-off
            chain = { { T::HighPass, 55, 0.5, 0 }, { T::HighPass, 80, 0.8, 0 },
                      { T::Peak, 115, 1.4, 2.5 }, { T::Peak, 900, 0.9, 1 }, { T::Peak, 1700, 1.1, 4 },
                      { T::Peak, 2800, 2.0, 1.5 }, { T::Peak, 4800, 3.0, -7 },
                      { T::LowPass, 4000, 0.8, 0 }, { T::LowPass, 4600, 0.6, 0 }, { T::LowPass, 6000, 0.55, 0 } };
            break;
        default: // Closed-back 2x12, V-type speakers: tighter and more focused than the 4x12
            chain = { { T::HighPass, 65, 0.5, 0 }, { T::HighPass, 90, 0.8, 0 },
                      { T::Peak, 130, 1.3, 3 }, { T::Peak, 450, 1.0, -2 }, { T::Peak, 2400, 1.5, 4.5 },
                      { T::Peak, 4400, 2.5, 3.5 }, { T::Peak, 7000, 4.0, -7 },
                      { T::LowPass, 5600, 0.8, 0 }, { T::LowPass, 6400, 0.6, 0 }, { T::LowPass, 8000, 0.55, 0 } };
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
        cachedMic[s] = cachedDist[s] = -1;
        cachedType[s] = -1;
        gainL[s].reset (sr, 0.03);
        gainR[s].reset (sr, 0.03);
        distDelay[s].prepare ((int) std::ceil (sr * 0.006) + 8);
    }
    reloadBoth(); // regenerate built-ins at the new rate
}

void Cabinet::reset()
{
    for (int s = 0; s < 2; ++s)
    {
        conv[s].reset();
        for (auto* b : { &micPresence[s], &micShelf[s], &micTypeA[s], &micTypeB[s], &micTypeC[s], &distLow[s], &distHigh[s] })
            b->reset();
        distDelay[s].reset();
    }
}

void Cabinet::setSource (int slot, int source)
{
    if (wantedSource[slot] == source && loadedSource[slot] >= 0)
        return;
    wantedSource[slot] = source;
    loadSource (slot);
}

void Cabinet::setAlign (bool shouldAlign)
{
    if (align == shouldAlign)
        return;
    align = shouldAlign;
    reloadBoth();
}

void Cabinet::reloadBoth()
{
    for (int s = 0; s < 2; ++s)
        loadSource (s);
}

void Cabinet::loadSource (int slot)
{
    const int src = wantedSource[slot];

    // Leading silence before each slot's onset, in seconds (the built-ins start immediately).
    auto onsetSeconds = [this] (int s)
    {
        const bool user = wantedSource[s] == userSource && userIR[s].getNumSamples() > 0;
        return user ? findOnset (userIR[s]) / userRate[s] : 0.0;
    };

    if (src == userSource && userIR[slot].getNumSamples() > 0)
    {
        // Aligned: each IR is cut just before its own onset, so the two slots line up. Not aligned:
        // both lose the same amount (the earlier onset), keeping the offset between them.
        const double margin = 4.0 / 48000.0;
        const double cut = std::max (0.0, (align ? onsetSeconds (slot) : std::min (onsetSeconds (0), onsetSeconds (1))) - margin);
        const int cutSamples = std::min ((int) (cut * userRate[slot]), userIR[slot].getNumSamples() - 16);
        trimmed[slot] = std::max (0, cutSamples);

        juce::AudioBuffer<float> ir (1, userIR[slot].getNumSamples() - trimmed[slot]);
        ir.copyFrom (0, 0, userIR[slot], 0, trimmed[slot], ir.getNumSamples());
        conv[slot].loadImpulseResponse (std::move (ir), userRate[slot], juce::dsp::Convolution::Stereo::no,
                                        juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::yes);
    }
    else
    {
        // "User IR" with no file falls back to the first built-in so the slot is never silent.
        trimmed[slot] = 0;
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

    // Cabinet IRs are short; anything beyond a couple of seconds is room tail we don't need.
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 2.0));
    juce::AudioBuffer<float> buffer (1, len);
    reader->read (&buffer, 0, len, 0, true, false); // first channel, as a mono IR

    userIR[slot] = std::move (buffer);
    userRate[slot] = reader->sampleRate > 0 ? reader->sampleRate : sr;
    userFile[slot] = file;
    wantedSource[slot] = userSource;
    // The other slot's trim depends on this IR's onset when alignment is off.
    if (align) loadSource (slot);
    else       reloadBoth();
    return {};
}

void Cabinet::updateMicFilters (int s, const CabSlotParams& sp)
{
    using T = Biquad::Type;
    if (sp.mic != cachedMic[s])
    {
        cachedMic[s] = sp.mic;
        // Mic position: cap (0) is bright and present, edge (1) is darker and rounder.
        micPresence[s].setup (T::Peak, sr, 4500.0, 1.2, (1.0f - sp.mic) * 3.0f - 1.0f);
        micShelf[s].setup (T::HighShelf, sr, 2500.0, 0.6, -sp.mic * 9.0f);
    }

    if (sp.micType != cachedType[s])
    {
        cachedType[s] = sp.micType;
        cachedDist[s] = -1; // proximity effect depends on the mic type
        switch (sp.micType)
        {
            case 1: // ribbon: dark and smooth on top, generous low end
                micTypeA[s].setup (T::HighShelf, sr, 4000.0, 0.6, -7.0);
                micTypeB[s].setup (T::LowPass, sr, 10000.0, 0.6);
                micTypeC[s].setup (T::LowShelf, sr, 160.0, 0.7, 2.5);
                break;
            case 2: // condenser: extended, airy top, flatter presence region
                micTypeA[s].setup (T::HighShelf, sr, 7000.0, 0.7, 2.5);
                micTypeB[s].setup (T::Peak, sr, 4500.0, 1.2, -1.5);
                micTypeC[s].setup (T::LowShelf, sr, 120.0, 0.7, 1.0);
                break;
            default: // dynamic: the reference voicing
                micTypeA[s].setIdentity();
                micTypeB[s].setIdentity();
                micTypeC[s].setIdentity();
                break;
        }
    }

    if (sp.distance != cachedDist[s])
    {
        cachedDist[s] = sp.distance;
        if (sp.distance <= 0.0f)
        {
            distLow[s].setIdentity();
            distHigh[s].setIdentity();
        }
        else
        {
            // Backing off loses the close-mic proximity boost (most for the ribbon's figure-8) and some air.
            const float proximity = sp.micType == 1 ? 9.0f : sp.micType == 0 ? 6.0f : 2.0f;
            distLow[s].setup (T::LowShelf, sr, 200.0, 0.7, -proximity * sp.distance);
            distHigh[s].setup (T::HighShelf, sr, 6000.0, 0.7, -4.0f * sp.distance);
        }
    }
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

        updateMicFilters (s, sp);

        float* buf = scratch.getWritePointer (s);
        for (int i = 0; i < n; ++i)
            buf[i] = micShelf[s].process (micPresence[s].process (mono[i]));

        if (sp.micType != 0)
            for (int i = 0; i < n; ++i)
                buf[i] = micTypeC[s].process (micTypeB[s].process (micTypeA[s].process (buf[i])));

        // Distance: tone, time of flight (unless the slots are aligned) and a floor reflection that
        // gets stronger and arrives sooner after the direct sound as the mic moves back.
        if (sp.distance > 0.0f)
        {
            const float tof = p.align ? 0.0f : (float) (sp.distance * maxDistanceMetres / 343.0 * sr);
            const float reflection = (float) ((3.0 - 1.5 * sp.distance) * 0.001 * sr);
            const float reflectionGain = 0.3f * sp.distance;
            for (int i = 0; i < n; ++i)
            {
                const float x = distHigh[s].process (distLow[s].process (buf[i]));
                distDelay[s].push (x);
                const float direct = tof < 1.0f ? x : distDelay[s].read (tof);
                buf[i] = direct + reflectionGain * distDelay[s].read (tof + reflection);
            }
        }
        else
        {
            for (int i = 0; i < n; ++i)
                distDelay[s].push (buf[i]); // keep the history current so moving the mic doesn't click
        }

        float* chans[] = { buf };
        juce::dsp::AudioBlock<float> block (chans, 1, (size_t) n);
        conv[s].process (juce::dsp::ProcessContextReplacing<float> (block));

        // Constant-power pan; a single centred slot keeps unity on both sides.
        const float angle = (sp.pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        // Energy-normalised IRs gain ~3 dB per doubling of the sample rate for low-frequency
        // content; compensate so tone/level is the same at 44.1, 48, 96 kHz...
        const float g = dbToGain (sp.levelDb) * std::sqrt (48000.0f / (float) sr) * (sp.invert ? -1.0f : 1.0f);
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
