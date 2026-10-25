#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/Engine.h"

namespace abyssal
{

namespace ids
{
    // global
    inline const juce::String inGain = "in_gain", outGain = "out_gain";
    inline const juce::String inputMode = "io_input", outputMode = "io_output", bypass = "bypass";
    // gate
    inline const juce::String gateOn = "gate_on", gateThresh = "gate_thresh", gateRelease = "gate_release",
                              gateKey = "gate_key";
    // octaver
    inline const juce::String octOn = "oct_on", octDry = "oct_dry", octDown1 = "oct_down1", octDown2 = "oct_down2";
    // drop tune
    inline const juce::String dropOn = "drop_on", dropSemis = "drop_semis", dropMix = "drop_mix";
    // compressor
    inline const juce::String compOn = "comp_on", compThresh = "comp_thresh", compRatio = "comp_ratio",
                              compAttack = "comp_attack", compRelease = "comp_release", compLevel = "comp_level";
    // overdrive
    inline const juce::String odOn = "od_on", odDrive = "od_drive", odTone = "od_tone", odLevel = "od_level";
    // graphic EQ
    inline const juce::String geqOn = "geq_on", geqLevel = "geq_level";
    inline juce::String geqBand (int b) { return "geq_b" + juce::String (b); }
    // amp
    inline const juce::String ampMode = "amp_mode", ampChannel = "amp_channel", ampGain = "amp_gain",
                              ampBass = "amp_bass", ampMid = "amp_mid", ampTreble = "amp_treble",
                              ampPresence = "amp_presence", ampDepth = "amp_depth", ampMaster = "amp_master",
                              ampBright = "amp_bright", ampTight = "amp_tight",
                              namIn = "nam_in", namOut = "nam_out",
                              namCalibrate = "nam_cal", namCalLevel = "nam_cal_level",
                              ampPower = "amp_power",
                              namTone = "nam_tone", namBass = "nam_bass", namMid = "nam_mid", namTreble = "nam_treble",
                              namPresence = "nam_presence";

    /** Every amp channel keeps its own faceplate. The Lead channel uses the original ids
        ("amp_gain", ...) so automation and presets from v0.2 still find it. */
    constexpr int numAmpChannels = 4;
    constexpr int numAmpKnobs = 9;
    inline const char* const ampKnobKeys[numAmpKnobs] = { "gain", "bass", "mid", "treble", "presence", "depth", "master", "bright", "tight" };
    inline juce::String ampChannelKey (int channel)
    {
        static const char* keys[] = { "clean", "crunch", "lead", "abyss" };
        return keys[juce::jlimit (0, numAmpChannels - 1, channel)];
    }
    inline juce::String ampKnob (int channel, const char* what)
    {
        return channel == 2 ? juce::String ("amp_") + what : "amp_" + ampChannelKey (channel) + "_" + what;
    }
    /** If `id` is one of the Lead channel's (legacy) faceplate ids, returns its key ("gain", ...). */
    inline const char* ampKnobKeyOf (const juce::String& id)
    {
        for (auto* k : ampKnobKeys)
            if (id == juce::String ("amp_") + k) return k;
        return nullptr;
    }
    // cabinet
    inline const juce::String cabOn = "cab_on", cabAlign = "cab_align";
    inline juce::String cab (int slot, const char* what) { return juce::String ("cab_") + (slot == 0 ? "a_" : "b_") + what; }
    // parametric EQ
    inline const juce::String peqOn = "peq_on", peqHpf = "peq_hpf", peqLpf = "peq_lpf";
    inline juce::String peq (int band, const char* what) { return "peq_" + juce::String (band + 1) + "_" + what; }
    // chorus / flanger
    inline const juce::String choOn = "cho_on", choRate = "cho_rate", choDepth = "cho_depth", choMix = "cho_mix";
    inline const juce::String flOn = "fl_on", flRate = "fl_rate", flDepth = "fl_depth", flFeedback = "fl_feedback", flMix = "fl_mix";
    // delay
    inline const juce::String dlyOn = "dly_on", dlyTime = "dly_time", dlySync = "dly_sync", dlyDiv = "dly_div",
                              dlyFeedback = "dly_feedback", dlyTone = "dly_tone", dlyMix = "dly_mix", dlyPing = "dly_ping",
                              dlyMode = "dly_mode";
    // reverb
    inline const juce::String revOn = "rev_on", revSize = "rev_size", revDamp = "rev_damp", revPredelay = "rev_predelay",
                              revWidth = "rev_width", revMix = "rev_mix", revMode = "rev_mode", revShimmer = "rev_shimmer";
    // wah / whammy / phaser / tremolo / doubler
    inline const juce::String wahOn = "wah_on", wahMode = "wah_mode", wahPos = "wah_pos", wahSens = "wah_sens", wahQ = "wah_q";
    inline const juce::String whamOn = "wham_on", whamInterval = "wham_interval", whamPos = "wham_pos", whamMix = "wham_mix";
    inline const juce::String phaOn = "pha_on", phaRate = "pha_rate", phaDepth = "pha_depth", phaFeedback = "pha_fb",
                              phaMix = "pha_mix", phaStages = "pha_stages";
    inline const juce::String tremOn = "trem_on", tremRate = "trem_rate", tremDepth = "trem_depth", tremShape = "trem_shape",
                              tremSync = "trem_sync", tremDiv = "trem_div", tremPan = "trem_pan";
    inline const juce::String dblOn = "dbl_on", dblWidth = "dbl_width", dblTime = "dbl_time", dblDetune = "dbl_detune";

    /** The on/off switch of each pedal, indexed by FxId. */
    inline const juce::String& fxOnParam (int fxId)
    {
        static const std::array<juce::String, numFx> onIds { gateOn, octOn, dropOn, compOn, odOn, geqOn, choOn, flOn, dlyOn, revOn,
                                                             wahOn, whamOn, phaOn, tremOn, dblOn };
        return onIds[(size_t) juce::jlimit (0, numFx - 1, fxId)];
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Setup parameters (routing, input level, bypass, NAM calibration) that presets never change. */
bool isGlobalParameter (const juce::String& paramId);

/** Upgrades a saved state / preset tree from an older version in place. v0.2 had one shared
    faceplate for all amp channels: its values move to whichever channel was selected. v0.3 had
    three built-in cabs, so "User IR" moves from source 3 to after the new built-ins. */
void migrateState (juce::ValueTree& state);

/** Caches raw parameter pointers so the audio thread can build an EngineParams snapshot cheaply. */
class ParamCache
{
public:
    explicit ParamCache (juce::AudioProcessorValueTreeState& apvts);
    void fill (EngineParams& p) const;

private:
    std::unordered_map<juce::String, std::atomic<float>*> map;
    // Indexed parameters are resolved up front so fill() never builds strings on the audio thread.
    std::atomic<float>* geq[GraphicEQParams::numBands] {};
    std::atomic<float>* cab[2][8] {};   // on, src, level, pan, mic, mic type, distance, invert
    std::atomic<float>* peq[ParamEQParams::numBands][3] {}; // freq, gain, q
    std::atomic<float>* amp[ids::numAmpChannels][ids::numAmpKnobs] {};

    float get (const juce::String& id) const { return map.at (id)->load(); }
    bool  on  (const juce::String& id) const { return get (id) > 0.5f; }
};

} // namespace abyssal
