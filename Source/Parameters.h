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
    inline const juce::String gateOn = "gate_on", gateThresh = "gate_thresh", gateRelease = "gate_release";
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
                              namIn = "nam_in", namOut = "nam_out";
    // cabinet
    inline const juce::String cabOn = "cab_on";
    inline juce::String cab (int slot, const char* what) { return juce::String ("cab_") + (slot == 0 ? "a_" : "b_") + what; }
    // parametric EQ
    inline const juce::String peqOn = "peq_on", peqHpf = "peq_hpf", peqLpf = "peq_lpf";
    inline juce::String peq (int band, const char* what) { return "peq_" + juce::String (band + 1) + "_" + what; }
    // chorus / flanger
    inline const juce::String choOn = "cho_on", choRate = "cho_rate", choDepth = "cho_depth", choMix = "cho_mix";
    inline const juce::String flOn = "fl_on", flRate = "fl_rate", flDepth = "fl_depth", flFeedback = "fl_feedback", flMix = "fl_mix";
    // delay
    inline const juce::String dlyOn = "dly_on", dlyTime = "dly_time", dlySync = "dly_sync", dlyDiv = "dly_div",
                              dlyFeedback = "dly_feedback", dlyTone = "dly_tone", dlyMix = "dly_mix", dlyPing = "dly_ping";
    // reverb
    inline const juce::String revOn = "rev_on", revSize = "rev_size", revDamp = "rev_damp", revPredelay = "rev_predelay",
                              revWidth = "rev_width", revMix = "rev_mix";
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Setup parameters (routing, input level, bypass) that presets never change. */
bool isGlobalParameter (const juce::String& paramId);

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
    std::atomic<float>* cab[2][5] {};   // on, src, level, pan, mic
    std::atomic<float>* peq[ParamEQParams::numBands][3] {}; // freq, gain, q

    float get (const juce::String& id) const { return map.at (id)->load(); }
    bool  on  (const juce::String& id) const { return get (id) > 0.5f; }
};

} // namespace abyssal
