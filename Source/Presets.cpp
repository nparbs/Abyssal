#include "Presets.h"
#include "Parameters.h"

namespace abyssal
{

juce::StringArray getFactoryCategories()
{
    return { "Djent", "Metalcore", "Thrash", "Death", "Black", "Doom", "Nu Metal", "Lead", "Clean", "Utility" };
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    using namespace ids;
    static const std::vector<FactoryPreset> presets =
    {
        { "Init", "Utility", {}, {} },

        // ------------------------------------------------------------------ Djent
        { "Modern Djent", "Djent", {},
          { { gateThresh, -54 }, { gateRelease, 35 },
            { odOn, 1 }, { odDrive, 0 }, { odTone, 6.5f }, { odLevel, 10 },
            { ampChannel, 2 }, { ampGain, 5.5f }, { ampBass, 5 }, { ampMid, 6 }, { ampTreble, 6.5f },
            { ampPresence, 6 }, { ampDepth, 3 }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.25f },
            { peqHpf, 95 }, { peqLpf, 10000 },
            { peq (1, "freq"), 450 }, { peq (1, "gain"), -2.5f }, { peq (1, "q"), 1.4f },
            { peq (2, "freq"), 2600 }, { peq (2, "gain"), 1.5f } } },

        { "Prog Chug", "Djent", {},
          { { outGain, -1.5f }, { gateThresh, -50 }, { gateRelease, 25 },
            { compOn, 1 }, { compThresh, -20 }, { compRatio, 3 }, { compAttack, 15 }, { compLevel, 4 },
            { odOn, 1 }, { odDrive, 0.5f }, { odTone, 7 }, { odLevel, 9.5f },
            { ampChannel, 2 }, { ampGain, 5 }, { ampBass, 4.5f }, { ampMid, 6.5f }, { ampTreble, 6 },
            { ampPresence, 6.5f }, { ampDepth, 3.5f }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.2f },
            { cab (1, "on"), 1 }, { cab (1, "src"), 2 }, { cab (1, "mic"), 0.5f }, { cab (1, "level"), -8 },
            { peqHpf, 110 }, { peqLpf, 9500 },
            { peq (0, "freq"), 180 }, { peq (0, "gain"), -2 } } },

        // ------------------------------------------------------------------ Metalcore
        { "Metalcore Breakdown", "Metalcore", {},
          { { outGain, 2.0f }, { gateThresh, -48 }, { gateRelease, 20 },
            { dropOn, 1 }, { dropSemis, -2 }, { dropMix, 1 },
            { odOn, 1 }, { odDrive, 0 }, { odTone, 6 }, { odLevel, 10 },
            { ampChannel, 2 }, { ampGain, 7 }, { ampBass, 6 }, { ampMid, 4.5f }, { ampTreble, 6.5f },
            { ampPresence, 6.5f }, { ampDepth, 5 }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.3f },
            { peqHpf, 85 }, { peqLpf, 10500 },
            { peq (1, "freq"), 400 }, { peq (1, "gain"), -3 } } },

        // ------------------------------------------------------------------ Thrash
        { "Thrash Rhythm", "Thrash", {},
          { { outGain, -2.0f }, { gateThresh, -58 }, { gateRelease, 50 },
            { odOn, 1 }, { odDrive, 1.5f }, { odTone, 6 }, { odLevel, 8 },
            { ampChannel, 2 }, { ampGain, 7 }, { ampBass, 6 }, { ampMid, 3.5f }, { ampTreble, 7 },
            { ampPresence, 6.5f }, { ampDepth, 4.5f },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.35f },
            { cab (1, "on"), 1 }, { cab (1, "src"), 0 }, { cab (1, "mic"), 0.6f }, { cab (1, "level"), -5 },
            { peqHpf, 85 }, { peqLpf, 10500 } } },

        { "Bay Area Crunch", "Thrash", {},
          { { gateThresh, -60 }, { gateRelease, 60 },
            { odOn, 1 }, { odDrive, 2 }, { odTone, 6.5f }, { odLevel, 7.5f },
            { ampChannel, 1 }, { ampGain, 8.5f }, { ampBass, 6.5f }, { ampMid, 3 }, { ampTreble, 7.5f },
            { ampPresence, 6 }, { ampDepth, 4 }, { ampTight, 1 },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.3f },
            { peqHpf, 80 }, { peqLpf, 11000 },
            { peq (2, "freq"), 3200 }, { peq (2, "gain"), 2 } } },

        // ------------------------------------------------------------------ Death
        { "Death Metal Wall", "Death", {},
          { { outGain, -3.0f }, { gateThresh, -52 }, { gateRelease, 30 },
            { odOn, 1 }, { odDrive, 0.5f }, { odTone, 5.5f }, { odLevel, 9.5f },
            { ampChannel, 2 }, { ampGain, 8.5f }, { ampBass, 6.5f }, { ampMid, 4 }, { ampTreble, 6 },
            { ampPresence, 5.5f }, { ampDepth, 6 }, { ampMaster, 6 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.3f }, { cab (0, "pan"), -0.5f }, { cab (0, "level"), -3 },
            { cab (1, "on"), 1 }, { cab (1, "src"), 1 }, { cab (1, "mic"), 0.4f }, { cab (1, "pan"), 0.5f }, { cab (1, "level"), -3 },
            { peqHpf, 80 }, { peqLpf, 9500 },
            { peq (0, "freq"), 110 }, { peq (0, "gain"), 1.5f } } },

        // The graphic EQ goes before a cranked overdrive: the classic Swedish "chainsaw" pedal trick.
        { "Swedish Chainsaw", "Death", "gate,geq,od,oct,drop,comp,amp,cho,fl,dly,rev",
          { { gateThresh, -52 }, { gateRelease, 40 },
            { geqOn, 1 }, { geqBand (0), 4 }, { geqBand (1), 8 }, { geqBand (2), 4 }, { geqBand (3), -2 },
            { geqBand (4), 8 }, { geqBand (5), 10 }, { geqBand (6), 8 }, { geqBand (7), 4 }, { geqLevel, -6 },
            { odOn, 1 }, { odDrive, 7 }, { odTone, 7 }, { odLevel, 5 },
            { ampChannel, 1 }, { ampGain, 5 }, { ampBass, 5.5f }, { ampMid, 5 }, { ampTreble, 5.5f },
            { ampPresence, 5 }, { ampDepth, 4 }, { ampTight, 0 },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.3f },
            { peqHpf, 75 }, { peqLpf, 10000 } } },

        // ------------------------------------------------------------------ Black
        { "Black Metal Buzz", "Black", {},
          { { outGain, 1.5f }, { gateThresh, -62 }, { gateRelease, 90 },
            { odOn, 1 }, { odDrive, 3 }, { odTone, 8 }, { odLevel, 7 },
            { ampChannel, 2 }, { ampGain, 8 }, { ampBass, 3.5f }, { ampMid, 4 }, { ampTreble, 8 },
            { ampPresence, 7 }, { ampDepth, 2 }, { ampTight, 1 },
            { cab (0, "src"), 2 }, { cab (0, "mic"), 0.2f },
            { peqHpf, 130 }, { peqLpf, 12500 },
            { revOn, 1 }, { revSize, 0.75f }, { revDamp, 0.35f }, { revMix, 0.22f }, { revPredelay, 15 } } },

        // ------------------------------------------------------------------ Doom
        // The octaver sits after the amp so the sub tracks a cleaner, already-saturated signal.
        { "Doom Sludge", "Doom", "gate,drop,comp,od,geq,amp,oct,cho,fl,dly,rev",
          { { outGain, -4.0f }, { gateThresh, -60 }, { gateRelease, 150 },
            { octOn, 1 }, { octDry, 1 }, { octDown1, 0.35f }, { octDown2, 0 },
            { odOn, 1 }, { odDrive, 4 }, { odTone, 4 }, { odLevel, 7 },
            { ampChannel, 1 }, { ampGain, 9 }, { ampBass, 8 }, { ampMid, 6 }, { ampTreble, 4 },
            { ampPresence, 4 }, { ampDepth, 7 }, { ampTight, 0 }, { ampMaster, 6.5f },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.6f },
            { peqHpf, 45 }, { peqLpf, 7000 },
            { revOn, 1 }, { revSize, 0.5f }, { revMix, 0.12f } } },

        // ------------------------------------------------------------------ Nu Metal
        { "Nu Metal Bounce", "Nu Metal", {},
          { { outGain, 5.0f }, { gateThresh, -50 }, { gateRelease, 25 },
            { odOn, 0 },
            { ampChannel, 2 }, { ampGain, 6.5f }, { ampBass, 7 }, { ampMid, 2.5f }, { ampTreble, 6.5f },
            { ampPresence, 6 }, { ampDepth, 6 }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.4f },
            { peqHpf, 70 }, { peqLpf, 10000 },
            { peq (1, "freq"), 700 }, { peq (1, "gain"), -3 }, { peq (1, "q"), 0.8f } } },

        // ------------------------------------------------------------------ Lead
        { "Searing Lead", "Lead", {},
          { { outGain, -1.5f }, { gateThresh, -66 }, { gateRelease, 120 },
            { odOn, 1 }, { odDrive, 2.5f }, { odTone, 5.5f }, { odLevel, 7.5f },
            { ampChannel, 2 }, { ampGain, 7.5f }, { ampBass, 4.5f }, { ampMid, 6.5f }, { ampTreble, 5.5f },
            { ampPresence, 5 }, { ampDepth, 3 },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.4f },
            { peqHpf, 100 }, { peqLpf, 9000 },
            { peq (1, "freq"), 800 }, { peq (1, "gain"), 2 },
            { dlyOn, 1 }, { dlyTime, 420 }, { dlyFeedback, 0.38f }, { dlyMix, 0.22f }, { dlyTone, 3500 },
            { revOn, 1 }, { revSize, 0.6f }, { revMix, 0.18f } } },

        { "Shred Lead", "Lead", {},
          { { outGain, 1.0f }, { gateThresh, -64 }, { gateRelease, 80 },
            { compOn, 1 }, { compThresh, -22 }, { compRatio, 3 }, { compLevel, 4 },
            { odOn, 1 }, { odDrive, 1.5f }, { odTone, 6 }, { odLevel, 9 },
            { ampChannel, 2 }, { ampGain, 7 }, { ampBass, 4.5f }, { ampMid, 7 }, { ampTreble, 6 },
            { ampPresence, 5.5f }, { ampDepth, 3 }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.45f },
            { peqHpf, 110 }, { peqLpf, 9000 },
            { dlyOn, 1 }, { dlySync, 1 }, { dlyDiv, 4 }, { dlyFeedback, 0.3f }, { dlyMix, 0.2f }, { dlyTone, 4000 },
            { revOn, 1 }, { revSize, 0.55f }, { revMix, 0.14f } } },

        // Flanger in front of the amp, like a pedal on the floor.
        { "Jet Flange Lead", "Lead", "gate,oct,drop,comp,od,fl,geq,amp,cho,dly,rev",
          { { outGain, 1.0f }, { gateThresh, -66 },
            { odOn, 1 }, { odDrive, 2 }, { odLevel, 8 },
            { ampChannel, 2 }, { ampGain, 7 }, { ampMid, 6 },
            { cab (0, "src"), 0 },
            { flOn, 1 }, { flRate, 0.15f }, { flDepth, 0.8f }, { flFeedback, 0.65f }, { flMix, 0.5f },
            { revOn, 1 }, { revMix, 0.15f } } },

        // ------------------------------------------------------------------ Clean
        { "Ambient Clean", "Clean", "comp,gate,oct,drop,od,geq,amp,cho,fl,dly,rev",
          { { outGain, 1.0f }, { gateThresh, -72 }, { gateRelease, 150 },
            { compOn, 1 }, { compThresh, -26 }, { compRatio, 4 }, { compLevel, 7 },
            { odOn, 0 },
            { ampChannel, 0 }, { ampGain, 4 }, { ampBass, 5 }, { ampMid, 5 }, { ampTreble, 6 },
            { ampBright, 1 }, { ampTight, 0 }, { ampPresence, 4 }, { ampDepth, 2 },
            { cab (0, "src"), 2 }, { cab (0, "mic"), 0.3f },
            { choOn, 1 }, { choRate, 0.6f }, { choDepth, 0.45f }, { choMix, 0.45f },
            { dlyOn, 1 }, { dlySync, 1 }, { dlyDiv, 4 }, { dlyFeedback, 0.45f }, { dlyMix, 0.3f },
            { revOn, 1 }, { revSize, 0.85f }, { revMix, 0.35f }, { revPredelay, 35 } } },
    };
    return presets;
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isGlobalParameter (rp->paramID))
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

} // namespace abyssal
