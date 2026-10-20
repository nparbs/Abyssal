#include "Parameters.h"

namespace abyssal
{

using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;

namespace
{
    struct Builder
    {
        juce::AudioProcessorValueTreeState::ParameterLayout& layout;

        void flt (const juce::String& id, const juce::String& name, float lo, float hi, float def,
                  const juce::String& label = {}, float skewCentre = 0.0f, float step = 0.0f)
        {
            juce::NormalisableRange<float> range (lo, hi, step);
            if (skewCentre > 0.0f)
                range.setSkewForCentre (skewCentre);
            layout.add (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, range, def,
                                               juce::AudioParameterFloatAttributes().withLabel (label)));
        }

        /** 0..10 amp-style knob. */
        void knob (const juce::String& id, const juce::String& name, float def)
        {
            flt (id, name, 0.0f, 10.0f, def, {}, 0.0f, 0.01f);
        }

        void toggle (const juce::String& id, const juce::String& name, bool def)
        {
            layout.add (std::make_unique<APB> (juce::ParameterID { id, 1 }, name, def));
        }

        void choice (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
        {
            layout.add (std::make_unique<APC> (juce::ParameterID { id, 1 }, name, items, def));
        }

        void integer (const juce::String& id, const juce::String& name, int lo, int hi, int def, const juce::String& label = {})
        {
            layout.add (std::make_unique<API> (juce::ParameterID { id, 1 }, name, lo, hi, def,
                                               juce::AudioParameterIntAttributes().withLabel (label)));
        }
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    Builder b { layout };

    b.flt (ids::inGain,  "Input",  -24.0f, 24.0f, 0.0f, "dB", 0.0f, 0.1f);
    b.flt (ids::outGain, "Output", -24.0f, 24.0f, 0.0f, "dB", 0.0f, 0.1f);
    b.choice (ids::inputMode, "Input Channel", { "Left", "Right", "Mono Sum" }, 0);
    b.choice (ids::outputMode, "Output Mode", { "Stereo", "Mono" }, 0);
    b.toggle (ids::bypass, "Bypass", false);

    b.toggle (ids::gateOn, "Gate On", true);
    b.flt (ids::gateThresh,  "Gate Threshold", -96.0f, -10.0f, -62.0f, "dB", 0.0f, 0.1f);
    b.flt (ids::gateRelease, "Gate Release", 5.0f, 500.0f, 60.0f, "ms", 80.0f);

    b.toggle (ids::octOn, "Octaver On", false);
    b.flt (ids::octDry,   "Octaver Dry", 0.0f, 1.0f, 1.0f);
    b.flt (ids::octDown1, "Octaver -1 Oct", 0.0f, 1.0f, 0.6f);
    b.flt (ids::octDown2, "Octaver -2 Oct", 0.0f, 1.0f, 0.0f);

    b.toggle (ids::dropOn, "Drop Tune On", false);
    b.integer (ids::dropSemis, "Drop Tune Semitones", -12, 12, -2, "st");
    b.flt (ids::dropMix, "Drop Tune Mix", 0.0f, 1.0f, 1.0f);

    b.toggle (ids::compOn, "Comp On", false);
    b.flt (ids::compThresh,  "Comp Threshold", -60.0f, 0.0f, -24.0f, "dB", 0.0f, 0.1f);
    b.flt (ids::compRatio,   "Comp Ratio", 1.0f, 20.0f, 4.0f, ":1", 4.0f);
    b.flt (ids::compAttack,  "Comp Attack", 0.1f, 100.0f, 4.0f, "ms", 10.0f);
    b.flt (ids::compRelease, "Comp Release", 10.0f, 1000.0f, 120.0f, "ms", 150.0f);
    b.flt (ids::compLevel,   "Comp Level", -12.0f, 24.0f, 6.0f, "dB", 0.0f, 0.1f);

    b.toggle (ids::odOn, "Overdrive On", true);
    b.knob (ids::odDrive, "Overdrive Drive", 1.0f);
    b.knob (ids::odTone,  "Overdrive Tone", 6.0f);
    b.knob (ids::odLevel, "Overdrive Level", 8.5f);

    b.toggle (ids::geqOn, "Graphic EQ On", false);
    for (int i = 0; i < GraphicEQParams::numBands; ++i)
    {
        const float f = GraphicEQParams::freqs[i];
        const auto name = f >= 1000.0f ? juce::String (f / 1000.0f) + "k" : juce::String ((int) f);
        b.flt (ids::geqBand (i), "Graphic EQ " + name, -12.0f, 12.0f, 0.0f, "dB", 0.0f, 0.1f);
    }
    b.flt (ids::geqLevel, "Graphic EQ Level", -12.0f, 12.0f, 0.0f, "dB", 0.0f, 0.1f);

    b.choice (ids::ampMode, "Amp Model", { "Abyssal DSP", "NAM Capture" }, 0);
    b.choice (ids::ampChannel, "Amp Channel", { "Clean", "Crunch", "Lead" }, 2);
    b.knob (ids::ampGain, "Amp Gain", 6.5f);
    b.knob (ids::ampBass, "Amp Bass", 5.5f);
    b.knob (ids::ampMid, "Amp Mid", 5.0f);
    b.knob (ids::ampTreble, "Amp Treble", 6.0f);
    b.knob (ids::ampPresence, "Amp Presence", 5.5f);
    b.knob (ids::ampDepth, "Amp Depth", 4.0f);
    b.knob (ids::ampMaster, "Amp Master", 5.0f);
    b.toggle (ids::ampBright, "Amp Bright", false);
    b.toggle (ids::ampTight, "Amp Tight", true);
    b.flt (ids::namIn,  "NAM Input",  -24.0f, 24.0f, 0.0f, "dB", 0.0f, 0.1f);
    b.flt (ids::namOut, "NAM Output", -24.0f, 24.0f, 0.0f, "dB", 0.0f, 0.1f);

    b.toggle (ids::cabOn, "Cab On", true);
    for (int s = 0; s < 2; ++s)
    {
        const juce::String n = s == 0 ? "Cab A " : "Cab B ";
        b.toggle (ids::cab (s, "on"), n + "On", s == 0);
        b.choice (ids::cab (s, "src"), n + "Source", Cabinet::getSourceNames(), s == 0 ? 0 : 1);
        b.flt (ids::cab (s, "level"), n + "Level", -24.0f, 12.0f, 0.0f, "dB", 0.0f, 0.1f);
        b.flt (ids::cab (s, "pan"), n + "Pan", -1.0f, 1.0f, 0.0f);
        b.flt (ids::cab (s, "mic"), n + "Mic Position", 0.0f, 1.0f, 0.3f);
    }

    b.toggle (ids::peqOn, "Post EQ On", true);
    b.flt (ids::peqHpf, "Post EQ Low Cut", 20.0f, 500.0f, 80.0f, "Hz", 90.0f);
    b.flt (ids::peqLpf, "Post EQ High Cut", 2000.0f, 20000.0f, 11000.0f, "Hz", 8000.0f);
    const float peqDefaults[ParamEQParams::numBands] = { 120.0f, 500.0f, 2500.0f, 6000.0f };
    for (int i = 0; i < ParamEQParams::numBands; ++i)
    {
        const juce::String n = "Post EQ Band " + juce::String (i + 1) + " ";
        b.flt (ids::peq (i, "freq"), n + "Freq", 30.0f, 16000.0f, peqDefaults[i], "Hz", 1000.0f);
        b.flt (ids::peq (i, "gain"), n + "Gain", -15.0f, 15.0f, 0.0f, "dB", 0.0f, 0.1f);
        b.flt (ids::peq (i, "q"), n + "Q", 0.2f, 8.0f, 1.0f, {}, 1.2f);
    }

    b.toggle (ids::choOn, "Chorus On", false);
    b.flt (ids::choRate, "Chorus Rate", 0.05f, 5.0f, 0.7f, "Hz", 1.0f);
    b.flt (ids::choDepth, "Chorus Depth", 0.0f, 1.0f, 0.4f);
    b.flt (ids::choMix, "Chorus Mix", 0.0f, 1.0f, 0.5f);

    b.toggle (ids::flOn, "Flanger On", false);
    b.flt (ids::flRate, "Flanger Rate", 0.02f, 5.0f, 0.2f, "Hz", 0.6f);
    b.flt (ids::flDepth, "Flanger Depth", 0.0f, 1.0f, 0.7f);
    b.flt (ids::flFeedback, "Flanger Feedback", -0.95f, 0.95f, 0.55f);
    b.flt (ids::flMix, "Flanger Mix", 0.0f, 1.0f, 0.5f);

    b.toggle (ids::dlyOn, "Delay On", false);
    b.flt (ids::dlyTime, "Delay Time", 10.0f, 2000.0f, 380.0f, "ms", 400.0f);
    b.toggle (ids::dlySync, "Delay Sync", false);
    b.choice (ids::dlyDiv, "Delay Division", DelayParams::divisionNames(), 3);
    b.flt (ids::dlyFeedback, "Delay Feedback", 0.0f, 0.95f, 0.35f);
    b.flt (ids::dlyTone, "Delay Tone", 800.0f, 14000.0f, 4500.0f, "Hz", 3500.0f);
    b.flt (ids::dlyMix, "Delay Mix", 0.0f, 1.0f, 0.25f);
    b.toggle (ids::dlyPing, "Delay Ping-Pong", true);

    b.toggle (ids::revOn, "Reverb On", false);
    b.flt (ids::revSize, "Reverb Size", 0.0f, 1.0f, 0.55f);
    b.flt (ids::revDamp, "Reverb Damping", 0.0f, 1.0f, 0.5f);
    b.flt (ids::revPredelay, "Reverb Pre-Delay", 0.0f, 200.0f, 20.0f, "ms", 40.0f);
    b.flt (ids::revWidth, "Reverb Width", 0.0f, 1.0f, 1.0f);
    b.flt (ids::revMix, "Reverb Mix", 0.0f, 1.0f, 0.2f);

    return layout;
}

bool isGlobalParameter (const juce::String& paramId)
{
    return paramId == ids::inGain || paramId == ids::inputMode || paramId == ids::outputMode || paramId == ids::bypass;
}

//==============================================================================
ParamCache::ParamCache (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* param : apvts.processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            map[withId->paramID] = apvts.getRawParameterValue (withId->paramID);

    for (int i = 0; i < GraphicEQParams::numBands; ++i)
        geq[i] = map.at (ids::geqBand (i));

    const char* cabFields[] = { "on", "src", "level", "pan", "mic" };
    for (int s = 0; s < 2; ++s)
        for (int f = 0; f < 5; ++f)
            cab[s][f] = map.at (ids::cab (s, cabFields[f]));

    const char* peqFields[] = { "freq", "gain", "q" };
    for (int i = 0; i < ParamEQParams::numBands; ++i)
        for (int f = 0; f < 3; ++f)
            peq[i][f] = map.at (ids::peq (i, peqFields[f]));
}

void ParamCache::fill (EngineParams& p) const
{
    p.inGainDb  = get (ids::inGain);
    p.outGainDb = get (ids::outGain);
    p.inputMode = (int) get (ids::inputMode);
    p.monoOut   = get (ids::outputMode) > 0.5f;
    p.bypass    = on (ids::bypass);

    p.gate = { on (ids::gateOn), get (ids::gateThresh), get (ids::gateRelease) };
    p.octaver = { on (ids::octOn), get (ids::octDry), get (ids::octDown1), get (ids::octDown2) };
    p.drop = { on (ids::dropOn), (int) std::round (get (ids::dropSemis)), get (ids::dropMix) };
    p.comp = { on (ids::compOn), get (ids::compThresh), get (ids::compRatio), get (ids::compAttack),
               get (ids::compRelease), get (ids::compLevel) };
    p.od = { on (ids::odOn), get (ids::odDrive), get (ids::odTone), get (ids::odLevel) };

    p.geq.on = on (ids::geqOn);
    for (int i = 0; i < GraphicEQParams::numBands; ++i)
        p.geq.gainsDb[i] = geq[i]->load();
    p.geq.levelDb = get (ids::geqLevel);

    p.amp.channel  = (int) get (ids::ampChannel);
    p.amp.gain     = get (ids::ampGain);
    p.amp.bass     = get (ids::ampBass);
    p.amp.mid      = get (ids::ampMid);
    p.amp.treble   = get (ids::ampTreble);
    p.amp.presence = get (ids::ampPresence);
    p.amp.depth    = get (ids::ampDepth);
    p.amp.master   = get (ids::ampMaster);
    p.amp.bright   = on (ids::ampBright);
    p.amp.tight    = on (ids::ampTight);
    p.useNam   = get (ids::ampMode) > 0.5f;
    p.namInDb  = get (ids::namIn);
    p.namOutDb = get (ids::namOut);

    p.cab.on = on (ids::cabOn);
    for (int s = 0; s < 2; ++s)
        p.cab.slot[s] = { cab[s][0]->load() > 0.5f, (int) cab[s][1]->load(), cab[s][2]->load(),
                          cab[s][3]->load(), cab[s][4]->load() };

    p.peq.on = on (ids::peqOn);
    p.peq.hpfHz = get (ids::peqHpf);
    p.peq.lpfHz = get (ids::peqLpf);
    for (int i = 0; i < ParamEQParams::numBands; ++i)
        p.peq.band[i] = { peq[i][0]->load(), peq[i][1]->load(), peq[i][2]->load() };

    p.chorus  = { on (ids::choOn), get (ids::choRate), get (ids::choDepth), 0.0f, get (ids::choMix) };
    p.flanger = { on (ids::flOn), get (ids::flRate), get (ids::flDepth), get (ids::flFeedback), get (ids::flMix) };

    p.delay.on       = on (ids::dlyOn);
    p.delay.timeMs   = get (ids::dlyTime);
    p.delay.sync     = on (ids::dlySync);
    p.delay.division = (int) get (ids::dlyDiv);
    p.delay.feedback = get (ids::dlyFeedback);
    p.delay.toneHz   = get (ids::dlyTone);
    p.delay.mix      = get (ids::dlyMix);
    p.delay.pingPong = on (ids::dlyPing);

    p.reverb = { on (ids::revOn), get (ids::revSize), get (ids::revDamp), get (ids::revPredelay),
                 get (ids::revWidth), get (ids::revMix) };
}

} // namespace abyssal
