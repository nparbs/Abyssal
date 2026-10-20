#include "Presets.h"
#include "Parameters.h"

namespace abyssal
{

const std::vector<FactoryPreset>& getFactoryPresets()
{
    using namespace ids;
    static const std::vector<FactoryPreset> presets =
    {
        { "Init", {} },

        { "Modern Djent",
          { { gateThresh, -54 }, { gateRelease, 35 },
            { odOn, 1 }, { odDrive, 0 }, { odTone, 6.5f }, { odLevel, 10 },
            { ampChannel, 2 }, { ampGain, 5.5f }, { ampBass, 5 }, { ampMid, 6 }, { ampTreble, 6.5f },
            { ampPresence, 6 }, { ampDepth, 3 }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.25f },
            { peqHpf, 95 }, { peqLpf, 10000 },
            { peq (1, "freq"), 450 }, { peq (1, "gain"), -2.5f }, { peq (1, "q"), 1.4f },
            { peq (2, "freq"), 2600 }, { peq (2, "gain"), 1.5f } } },

        { "Thrash Rhythm",
          { { gateThresh, -58 }, { gateRelease, 50 },
            { odOn, 1 }, { odDrive, 1.5f }, { odTone, 6 }, { odLevel, 8 },
            { ampChannel, 2 }, { ampGain, 7 }, { ampBass, 6 }, { ampMid, 3.5f }, { ampTreble, 7 },
            { ampPresence, 6.5f }, { ampDepth, 4.5f },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.35f },
            { cab (1, "on"), 1 }, { cab (1, "src"), 0 }, { cab (1, "mic"), 0.6f }, { cab (1, "level"), -5 },
            { peqHpf, 85 }, { peqLpf, 10500 } } },

        { "Death Metal Wall",
          { { gateThresh, -52 }, { gateRelease, 30 },
            { odOn, 1 }, { odDrive, 0.5f }, { odTone, 5.5f }, { odLevel, 9.5f },
            { ampChannel, 2 }, { ampGain, 8.5f }, { ampBass, 6.5f }, { ampMid, 4 }, { ampTreble, 6 },
            { ampPresence, 5.5f }, { ampDepth, 6 }, { ampMaster, 6 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.3f }, { cab (0, "pan"), -0.5f }, { cab (0, "level"), -3 },
            { cab (1, "on"), 1 }, { cab (1, "src"), 1 }, { cab (1, "mic"), 0.4f }, { cab (1, "pan"), 0.5f }, { cab (1, "level"), -3 },
            { peqHpf, 80 }, { peqLpf, 9500 },
            { peq (0, "freq"), 110 }, { peq (0, "gain"), 1.5f } } },

        { "Drop C Chug",
          { { gateThresh, -50 }, { gateRelease, 30 },
            { dropOn, 1 }, { dropSemis, -2 }, { dropMix, 1 },
            { odOn, 1 }, { odDrive, 0 }, { odTone, 6 }, { odLevel, 10 },
            { ampChannel, 2 }, { ampGain, 6.5f }, { ampBass, 5 }, { ampMid, 5.5f }, { ampTreble, 6 },
            { ampPresence, 6 }, { ampDepth, 4 }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { cab (0, "mic"), 0.3f }, { peqHpf, 90 } } },

        { "Searing Lead",
          { { gateThresh, -66 }, { gateRelease, 120 },
            { odOn, 1 }, { odDrive, 2.5f }, { odTone, 5.5f }, { odLevel, 7.5f },
            { ampChannel, 2 }, { ampGain, 7.5f }, { ampBass, 4.5f }, { ampMid, 6.5f }, { ampTreble, 5.5f },
            { ampPresence, 5 }, { ampDepth, 3 },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.4f },
            { peqHpf, 100 }, { peqLpf, 9000 },
            { peq (1, "freq"), 800 }, { peq (1, "gain"), 2 },
            { dlyOn, 1 }, { dlyTime, 420 }, { dlyFeedback, 0.38f }, { dlyMix, 0.22f }, { dlyTone, 3500 },
            { revOn, 1 }, { revSize, 0.6f }, { revMix, 0.18f } } },

        { "Crunch Rhythm",
          { { gateThresh, -64 },
            { odOn, 0 },
            { ampChannel, 1 }, { ampGain, 6.5f }, { ampBass, 5 }, { ampMid, 6.5f }, { ampTreble, 6 },
            { ampPresence, 5 }, { ampDepth, 3 }, { ampTight, 0 },
            { cab (0, "src"), 1 }, { cab (0, "mic"), 0.35f }, { peqHpf, 70 } } },

        { "Ambient Clean",
          { { gateThresh, -72 }, { gateRelease, 150 },
            { compOn, 1 }, { compThresh, -26 }, { compRatio, 4 }, { compLevel, 7 },
            { odOn, 0 },
            { ampChannel, 0 }, { ampGain, 4 }, { ampBass, 5 }, { ampMid, 5 }, { ampTreble, 6 },
            { ampBright, 1 }, { ampTight, 0 }, { ampPresence, 4 }, { ampDepth, 2 },
            { cab (0, "src"), 2 }, { cab (0, "mic"), 0.3f },
            { choOn, 1 }, { choRate, 0.6f }, { choDepth, 0.45f }, { choMix, 0.45f },
            { dlyOn, 1 }, { dlySync, 1 }, { dlyDiv, 4 }, { dlyFeedback, 0.45f }, { dlyMix, 0.3f },
            { revOn, 1 }, { revSize, 0.85f }, { revMix, 0.35f }, { revPredelay, 35 } } },

        { "Sub Octave Riff",
          { { gateThresh, -54 }, { gateRelease, 40 },
            { octOn, 1 }, { octDry, 1 }, { octDown1, 0.55f }, { octDown2, 0 },
            { odOn, 1 }, { odDrive, 0 }, { odLevel, 9.5f },
            { ampChannel, 2 }, { ampGain, 6 }, { ampMid, 5.5f }, { ampTight, 1 },
            { cab (0, "src"), 0 }, { peqHpf, 60 } } },

        { "Jet Flange Lead",
          { { gateThresh, -66 },
            { odOn, 1 }, { odDrive, 2 }, { odLevel, 8 },
            { ampChannel, 2 }, { ampGain, 7 }, { ampMid, 6 },
            { cab (0, "src"), 0 },
            { flOn, 1 }, { flRate, 0.15f }, { flDepth, 0.8f }, { flFeedback, 0.65f }, { flMix, 0.5f },
            { revOn, 1 }, { revMix, 0.15f } } },
    };
    return presets;
}

void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset)
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

} // namespace abyssal
