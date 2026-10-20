#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace abyssal
{

/** Every movable pedal / effect. The amp block (amp -> cab -> post EQ) is a fixed unit that the
    pedals are arranged around. */
enum FxId : int
{
    fxGate = 0, fxOctaver, fxDrop, fxComp, fxOverdrive, fxGEQ,
    fxChorus, fxFlanger, fxDelay, fxReverb,
    numFx,
    fxAmpBlock = numFx
};

constexpr int chainLength = numFx + 1;

inline const char* fxKey (int id)
{
    static const char* keys[] = { "gate", "oct", "drop", "comp", "od", "geq", "cho", "fl", "dly", "rev", "amp" };
    return keys[juce::jlimit (0, chainLength - 1, id)];
}

inline const char* fxShortName (int id)
{
    static const char* names[] = { "GATE", "OCT", "DROP", "COMP", "OD", "GEQ", "CHORUS", "FLANGER", "DELAY", "REVERB", "AMP" };
    return names[juce::jlimit (0, chainLength - 1, id)];
}

/** The order of the whole chain. Packs into 64 bits so the audio thread can read it atomically. */
struct ChainOrder
{
    std::array<int, chainLength> slots {};

    static ChainOrder defaultOrder()
    {
        return { { fxGate, fxOctaver, fxDrop, fxComp, fxOverdrive, fxGEQ, fxAmpBlock,
                   fxChorus, fxFlanger, fxDelay, fxReverb } };
    }

    bool operator== (const ChainOrder& o) const { return slots == o.slots; }

    int indexOf (int id) const
    {
        for (int i = 0; i < chainLength; ++i)
            if (slots[(size_t) i] == id) return i;
        return -1;
    }

    int ampIndex() const { return indexOf (fxAmpBlock); }
    bool isPreAmp (int id) const { return indexOf (id) < ampIndex(); }

    /** The pedals on one side of the amp, in order. */
    juce::Array<int> section (bool preAmp) const
    {
        juce::Array<int> out;
        const int amp = ampIndex();
        for (int i = preAmp ? 0 : amp + 1; i < (preAmp ? amp : chainLength); ++i)
            out.add (slots[(size_t) i]);
        return out;
    }

    /** Moves a pedal to position `index` within the given section (clamped; -1 = end). */
    void moveTo (int id, bool preAmp, int index)
    {
        if (id < 0 || id >= numFx) return;
        juce::Array<int> list;
        for (auto s : slots) if (s != id) list.add (s);

        const int amp = list.indexOf ((int) fxAmpBlock);
        const int start = preAmp ? 0 : amp + 1;
        const int count = preAmp ? amp : list.size() - amp - 1;
        index = (index < 0 || index > count) ? count : index;
        list.insert (start + index, id);

        for (int i = 0; i < chainLength; ++i)
            slots[(size_t) i] = list[i];
    }

    juce::uint64 pack() const
    {
        juce::uint64 v = 0;
        for (int i = 0; i < chainLength; ++i)
            v |= (juce::uint64) (slots[(size_t) i] & 0xf) << (4 * i);
        return v;
    }

    static ChainOrder unpack (juce::uint64 v)
    {
        ChainOrder o;
        for (int i = 0; i < chainLength; ++i)
            o.slots[(size_t) i] = (int) ((v >> (4 * i)) & 0xf);
        return o;
    }

    juce::String toString() const
    {
        juce::StringArray s;
        for (auto id : slots) s.add (fxKey (id));
        return s.joinIntoString (",");
    }

    /** Parses "gate,oct,...,amp,..."; unknown keys are ignored and missing pedals are appended
        in their default positions, so presets from older versions still load. */
    static ChainOrder fromString (const juce::String& text)
    {
        auto tokens = juce::StringArray::fromTokens (text, ",", "");
        juce::Array<int> list;
        for (auto& t : tokens)
            for (int id = 0; id < chainLength; ++id)
                if (t.trim() == fxKey (id) && ! list.contains (id))
                    list.add (id);

        if (! list.contains ((int) fxAmpBlock))
            return defaultOrder();

        const auto def = defaultOrder();
        for (int id = 0; id < numFx; ++id)
            if (! list.contains (id))
            {
                if (def.isPreAmp (id)) list.insert (list.indexOf ((int) fxAmpBlock), id);
                else                   list.add (id);
            }

        ChainOrder o;
        for (int i = 0; i < chainLength; ++i)
            o.slots[(size_t) i] = list[i];
        return o;
    }
};

} // namespace abyssal
