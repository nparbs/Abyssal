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
    b.toggle (ids::gateKey, "Gate Key From Input", false);

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
    b.choice (ids::ampChannel, "Amp Channel", { "Clean", "Crunch", "Lead", "Abyss" }, 2);
    b.choice (ids::ampPower, "Amp Power Tubes", { "EL34", "6L6" }, 0);
    {
        // Same defaults on every channel (the v0.2 shared faceplate), so factory presets that only
        // list some knobs sound exactly as they did: gain, bass, mid, treble, presence, depth, master, bright, tight
        const float defaults[ids::numAmpKnobs] = { 6.5f, 5.5f, 5.0f, 6.0f, 5.5f, 4.0f, 5.0f, 0.0f, 1.0f };
        const char* knobNames[] = { "Gain", "Bass", "Mid", "Treble", "Presence", "Depth", "Master", "Bright", "Tight" };
        const char* channelNames[] = { "Clean", "Crunch", "Lead", "Abyss" };
        for (int c = 0; c < ids::numAmpChannels; ++c)
            for (int k = 0; k < ids::numAmpKnobs; ++k)
            {
                const auto id = ids::ampKnob (c, ids::ampKnobKeys[k]);
                const auto name = juce::String ("Amp ") + channelNames[c] + " " + knobNames[k];
                if (k < 7) b.knob (id, name, defaults[k]);
                else       b.toggle (id, name, defaults[k] > 0.5f);
            }
    }
    b.flt (ids::namIn,  "NAM Input",  -24.0f, 24.0f, 0.0f, "dB", 0.0f, 0.1f);
    b.flt (ids::namOut, "NAM Output", -24.0f, 24.0f, 0.0f, "dB", 0.0f, 0.1f);
    b.toggle (ids::namCalibrate, "NAM Calibrate Input", false);
    b.flt (ids::namCalLevel, "NAM Interface Level", -10.0f, 30.0f, 12.0f, "dBu", 0.0f, 0.1f);
    b.toggle (ids::namTone, "NAM Tone Controls", false);
    b.knob (ids::namBass, "NAM Bass", 5.0f);
    b.knob (ids::namMid, "NAM Mid", 5.0f);
    b.knob (ids::namTreble, "NAM Treble", 5.0f);
    b.knob (ids::namPresence, "NAM Presence", 5.0f);

    b.toggle (ids::cabOn, "Cab On", true);
    b.toggle (ids::cabAlign, "Cab Align Slots", true);
    for (int s = 0; s < 2; ++s)
    {
        const juce::String n = s == 0 ? "Cab A " : "Cab B ";
        b.toggle (ids::cab (s, "on"), n + "On", s == 0);
        b.choice (ids::cab (s, "src"), n + "Source", Cabinet::getSourceNames(), s == 0 ? 0 : 1);
        b.flt (ids::cab (s, "level"), n + "Level", -24.0f, 12.0f, 0.0f, "dB", 0.0f, 0.1f);
        b.flt (ids::cab (s, "pan"), n + "Pan", -1.0f, 1.0f, 0.0f);
        b.flt (ids::cab (s, "mic"), n + "Mic Position", 0.0f, 1.0f, 0.3f);
        b.choice (ids::cab (s, "mtype"), n + "Mic Type", Cabinet::getMicNames(), 0);
        b.flt (ids::cab (s, "dist"), n + "Mic Distance", 0.0f, 1.0f, 0.0f);
        b.toggle (ids::cab (s, "inv"), n + "Polarity Invert", false);
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
    b.choice (ids::dlyMode, "Delay Mode", DelayParams::modeNames(), 0);

    b.toggle (ids::revOn, "Reverb On", false);
    b.flt (ids::revSize, "Reverb Size", 0.0f, 1.0f, 0.55f);
    b.flt (ids::revDamp, "Reverb Damping", 0.0f, 1.0f, 0.5f);
    b.flt (ids::revPredelay, "Reverb Pre-Delay", 0.0f, 200.0f, 20.0f, "ms", 40.0f);
    b.flt (ids::revWidth, "Reverb Width", 0.0f, 1.0f, 1.0f);
    b.flt (ids::revMix, "Reverb Mix", 0.0f, 1.0f, 0.2f);
    b.choice (ids::revMode, "Reverb Mode", ReverbParams::modeNames(), 0);
    b.flt (ids::revShimmer, "Reverb Shimmer", 0.0f, 1.0f, 0.5f);

    b.toggle (ids::wahOn, "Wah On", false);
    b.choice (ids::wahMode, "Wah Mode", { "Pedal", "Auto" }, 0);
    b.flt (ids::wahPos, "Wah Position", 0.0f, 1.0f, 0.5f);
    b.flt (ids::wahSens, "Wah Sensitivity", 0.0f, 1.0f, 0.5f);
    b.flt (ids::wahQ, "Wah Resonance", 0.0f, 1.0f, 0.5f);

    b.toggle (ids::whamOn, "Whammy On", false);
    b.choice (ids::whamInterval, "Whammy Interval", WhammyParams::intervalNames(), 1);
    b.flt (ids::whamPos, "Whammy Position", 0.0f, 1.0f, 1.0f);
    b.flt (ids::whamMix, "Whammy Mix", 0.0f, 1.0f, 1.0f);

    b.toggle (ids::phaOn, "Phaser On", false);
    b.flt (ids::phaRate, "Phaser Rate", 0.05f, 8.0f, 0.5f, "Hz", 0.8f);
    b.flt (ids::phaDepth, "Phaser Depth", 0.0f, 1.0f, 0.7f);
    b.flt (ids::phaFeedback, "Phaser Feedback", 0.0f, 0.9f, 0.4f);
    b.flt (ids::phaMix, "Phaser Mix", 0.0f, 1.0f, 0.6f);
    b.choice (ids::phaStages, "Phaser Stages", { "4 Stage", "8 Stage" }, 0);

    b.toggle (ids::tremOn, "Tremolo On", false);
    b.flt (ids::tremRate, "Tremolo Rate", 0.5f, 15.0f, 5.0f, "Hz", 4.0f);
    b.flt (ids::tremDepth, "Tremolo Depth", 0.0f, 1.0f, 0.6f);
    b.flt (ids::tremShape, "Tremolo Shape", 0.0f, 1.0f, 0.3f);
    b.toggle (ids::tremSync, "Tremolo Sync", false);
    b.choice (ids::tremDiv, "Tremolo Division", DelayDivisions::names(), 5);
    b.toggle (ids::tremPan, "Tremolo Auto-Pan", false);

    b.toggle (ids::dblOn, "Doubler On", false);
    b.flt (ids::dblWidth, "Doubler Width", 0.0f, 1.0f, 0.8f);
    b.flt (ids::dblTime, "Doubler Time", 5.0f, 40.0f, 14.0f, "ms");
    b.flt (ids::dblDetune, "Doubler Detune", 0.0f, 1.0f, 0.4f);

    return layout;
}

bool isGlobalParameter (const juce::String& paramId)
{
    return paramId == ids::inGain || paramId == ids::inputMode || paramId == ids::outputMode || paramId == ids::bypass
        || paramId == ids::namCalibrate || paramId == ids::namCalLevel;
}

void migrateState (juce::ValueTree& state)
{
    const int version = (int) state.getProperty ("version", 1);
    if (version < 3)
    {
        auto findParam = [&state] (const juce::String& id)
        {
            for (auto child : state)
                if (child.hasType ("PARAM") && child.getProperty ("id").toString() == id)
                    return child;
            return juce::ValueTree();
        };

        const auto channelParam = findParam (ids::ampChannel);
        const int channel = channelParam.isValid() ? (int) channelParam.getProperty ("value") : 2;
        if (channel != 2)
            for (auto* key : ids::ampKnobKeys)
                if (auto legacy = findParam (ids::ampKnob (2, key)); legacy.isValid())
                {
                    auto target = findParam (ids::ampKnob (channel, key));
                    if (! target.isValid())
                    {
                        target = juce::ValueTree ("PARAM");
                        target.setProperty ("id", ids::ampKnob (channel, key), nullptr);
                        state.appendChild (target, nullptr);
                    }
                    target.setProperty ("value", legacy.getProperty ("value"), nullptr);
                }
    }
    if (version < 4)
    {
        // Built-in cabs 3..5 were added in front of "User IR".
        for (auto child : state)
            if (child.hasType ("PARAM"))
                for (int slot = 0; slot < 2; ++slot)
                    if (child.getProperty ("id").toString() == ids::cab (slot, "src") && (int) child.getProperty ("value") == 3)
                        child.setProperty ("value", Cabinet::userSource, nullptr);
    }
    state.setProperty ("version", 4, nullptr);
}

//==============================================================================
ParamCache::ParamCache (juce::AudioProcessorValueTreeState& apvts)
{
    for (auto* param : apvts.processor.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            map[withId->paramID] = apvts.getRawParameterValue (withId->paramID);

    for (int i = 0; i < GraphicEQParams::numBands; ++i)
        geq[i] = map.at (ids::geqBand (i));

    const char* cabFields[] = { "on", "src", "level", "pan", "mic", "mtype", "dist", "inv" };
    for (int s = 0; s < 2; ++s)
        for (int f = 0; f < 8; ++f)
            cab[s][f] = map.at (ids::cab (s, cabFields[f]));

    const char* peqFields[] = { "freq", "gain", "q" };
    for (int i = 0; i < ParamEQParams::numBands; ++i)
        for (int f = 0; f < 3; ++f)
            peq[i][f] = map.at (ids::peq (i, peqFields[f]));

    for (int c = 0; c < ids::numAmpChannels; ++c)
        for (int k = 0; k < ids::numAmpKnobs; ++k)
            amp[c][k] = map.at (ids::ampKnob (c, ids::ampKnobKeys[k]));
}

void ParamCache::fill (EngineParams& p) const
{
    p.inGainDb  = get (ids::inGain);
    p.outGainDb = get (ids::outGain);
    p.inputMode = (int) get (ids::inputMode);
    p.monoOut   = get (ids::outputMode) > 0.5f;
    p.bypass    = on (ids::bypass);

    p.gate = { on (ids::gateOn), get (ids::gateThresh), get (ids::gateRelease), on (ids::gateKey) };
    p.octaver = { on (ids::octOn), get (ids::octDry), get (ids::octDown1), get (ids::octDown2) };
    p.drop = { on (ids::dropOn), (int) std::round (get (ids::dropSemis)), get (ids::dropMix) };
    p.comp = { on (ids::compOn), get (ids::compThresh), get (ids::compRatio), get (ids::compAttack),
               get (ids::compRelease), get (ids::compLevel) };
    p.od = { on (ids::odOn), get (ids::odDrive), get (ids::odTone), get (ids::odLevel) };

    p.geq.on = on (ids::geqOn);
    for (int i = 0; i < GraphicEQParams::numBands; ++i)
        p.geq.gainsDb[i] = geq[i]->load();
    p.geq.levelDb = get (ids::geqLevel);

    p.amp.channel  = juce::jlimit (0, ids::numAmpChannels - 1, (int) get (ids::ampChannel));
    {
        auto* const* k = amp[p.amp.channel];
        p.amp.gain     = k[0]->load();
        p.amp.bass     = k[1]->load();
        p.amp.mid      = k[2]->load();
        p.amp.treble   = k[3]->load();
        p.amp.presence = k[4]->load();
        p.amp.depth    = k[5]->load();
        p.amp.master   = k[6]->load();
        p.amp.bright   = k[7]->load() > 0.5f;
        p.amp.tight    = k[8]->load() > 0.5f;
    }
    p.amp.power = (int) get (ids::ampPower);
    p.useNam   = get (ids::ampMode) > 0.5f;
    p.namInDb  = get (ids::namIn);
    p.namOutDb = get (ids::namOut);
    p.namCalibrate = on (ids::namCalibrate);
    p.namInterfaceDbu = get (ids::namCalLevel);
    p.namTone = { on (ids::namTone), get (ids::namBass), get (ids::namMid), get (ids::namTreble), get (ids::namPresence) };

    p.cab.on = on (ids::cabOn);
    for (int s = 0; s < 2; ++s)
        p.cab.slot[s] = { cab[s][0]->load() > 0.5f, (int) cab[s][1]->load(), cab[s][2]->load(),
                          cab[s][3]->load(), cab[s][4]->load(), (int) cab[s][5]->load(), cab[s][6]->load(),
                          cab[s][7]->load() > 0.5f };
    p.cab.align = on (ids::cabAlign);

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
    p.delay.mode     = (int) get (ids::dlyMode);

    p.reverb = { on (ids::revOn), get (ids::revSize), get (ids::revDamp), get (ids::revPredelay),
                 get (ids::revWidth), get (ids::revMix), (int) get (ids::revMode), get (ids::revShimmer) };

    p.wah = { on (ids::wahOn), get (ids::wahMode) > 0.5f, get (ids::wahPos), get (ids::wahSens), get (ids::wahQ) };
    p.whammy = { on (ids::whamOn), (int) get (ids::whamInterval), get (ids::whamPos), get (ids::whamMix) };
    p.phaser = { on (ids::phaOn), get (ids::phaRate), get (ids::phaDepth), get (ids::phaFeedback), get (ids::phaMix),
                 (int) get (ids::phaStages) };
    p.tremolo = { on (ids::tremOn), get (ids::tremRate), get (ids::tremDepth), get (ids::tremShape), on (ids::tremSync),
                  on (ids::tremPan), (int) get (ids::tremDiv) };
    p.doubler = { on (ids::dblOn), get (ids::dblWidth), get (ids::dblTime), get (ids::dblDetune) };
}

} // namespace abyssal
