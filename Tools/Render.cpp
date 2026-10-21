// AbyssalRender: offline test harness.
//   AbyssalRender <outputDir> [sampleRate] [model.nam]
// Synthesizes a metal DI riff, runs it through every factory preset, writes WAVs and prints
// level / stability / CPU stats. Exits non-zero if any preset produces NaNs or silence.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Parameters.h"
#include "Presets.h"
#include "FileLibrary.h"
#include "Online/T3KAuth.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

/** Loads the built VST3 like a DAW would and checks every .nam in a folder actually loads inside the
    plugin binary (state round-trip: namPath in -> namPath out only if the model loaded). */
static int vst3Test (const juce::File& pluginFile, const juce::File& modelsDir)
{
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl; if (! ok) ++fails; };

    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> types;
    format.findAllTypesForFile (types, pluginFile.getFullPathName());
    check (! types.isEmpty(), "VST3 scanned: " + pluginFile.getFileName());
    if (types.isEmpty()) return 1;

    juce::String error;
    auto plugin = format.createInstanceFromDescription (*types[0], 48000.0, 512, error);
    check (plugin != nullptr, "VST3 instantiated " + error);
    if (plugin == nullptr) return 1;
    plugin->enableAllBuses();
    plugin->prepareToPlay (48000.0, 512);

    // A hosted VST3's state is JUCE's <VST3PluginState> envelope with the plugin's own state
    // base64-encoded in <IComponent>. Read / edit / write Abyssal's state through it.
    auto readPluginState = [&]() -> std::unique_ptr<juce::XmlElement>
    {
        juce::MemoryBlock outer;
        plugin->getStateInformation (outer);
        auto env = juce::AudioProcessor::getXmlFromBinary (outer.getData(), (int) outer.getSize());
        if (env == nullptr) return {};
        auto* comp = env->getChildByName ("IComponent");
        juce::MemoryBlock inner;
        if (comp == nullptr || ! inner.fromBase64Encoding (comp->getAllSubText())) return {};
        return juce::AudioProcessor::getXmlFromBinary (inner.getData(), (int) inner.getSize());
    };
    auto writePluginState = [&] (const juce::XmlElement& state)
    {
        juce::MemoryBlock inner, outer;
        juce::AudioProcessor::copyXmlToBinary (state, inner);
        juce::XmlElement env ("VST3PluginState");
        env.createNewChildElement ("IComponent")->addTextElement (inner.toBase64Encoding());
        juce::AudioProcessor::copyXmlToBinary (env, outer);
        plugin->setStateInformation (outer.getData(), (int) outer.getSize());
    };

    auto initial = readPluginState();
    check (initial != nullptr && initial->hasTagName ("PARAMS"), "read Abyssal state through the VST3 wrapper");
    if (initial == nullptr) return 1;

    auto models = modelsDir.findChildFiles (juce::File::findFiles, false, "*.nam");
    models.sort();
    for (auto& model : models)
    {
        auto state = std::make_unique<juce::XmlElement> (*initial);
        state->setAttribute ("namPath", model.getFullPathName());
        writePluginState (*state);

        // The plugin only reports a namPath back if the model really loaded.
        auto after = readPluginState();
        const bool loaded = after != nullptr && juce::File (after->getStringAttribute ("namPath")) == model;

        // Run audio through it too, so a model that loads but crashes on process is caught.
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < 20; ++i)
        {
            for (int s = 0; s < 512; ++s)
                buffer.setSample (0, s, 0.2f * std::sin (0.05f * (float) (i * 512 + s)));
            buffer.copyFrom (1, 0, buffer, 0, 0, 512);
            plugin->processBlock (buffer, midi);
        }
        bool finite = true;
        for (int s = 0; s < 512; ++s) finite = finite && std::isfinite (buffer.getSample (0, s));
        check (loaded && finite, "plugin loads + plays " + model.getFileName());
    }

    plugin->releaseResources();
    std::cout << (fails == 0 ? "VST3 NAM OK" : "VST3 NAM FAILURES") << std::endl;
    return fails == 0 ? 0 : 1;
}

/** Offline checks of the TONE3000 sign-in plumbing (no account or key needed). */
static int tone3000Test()
{
    using namespace abyssal::t3k;
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl; if (! ok) ++fails; };

    // PKCE: 43-char URL-safe verifier, challenge = base64url(SHA-256(verifier)), unique per call.
    const auto a = Pkce::generate(), b = Pkce::generate();
    check (a.codeVerifier.length() == 43 && a.codeVerifier.containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"),
           "PKCE verifier is 43 URL-safe chars");
    juce::SHA256 sha (a.codeVerifier.toRawUTF8(), a.codeVerifier.getNumBytesAsUTF8());
    const auto raw = sha.getRawData();
    const auto expected = juce::Base64::toBase64 (raw.getData(), raw.getSize()).replaceCharacter ('+', '-').replaceCharacter ('/', '_').removeCharacters ("=");
    check (a.codeChallenge == expected, "PKCE challenge = base64url(SHA-256(verifier))");
    check (a.codeVerifier != b.codeVerifier && a.state != b.state, "verifier and state are random per flow");

    // Callback parsing
    const auto cb = OAuthCallback::parse ("http://127.0.0.1:5000/callback?code=abc%20123&state=xyz&tone_id=42");
    check (cb.code == "abc 123" && cb.state == "xyz" && cb.toneId == "42" && ! cb.canceled, "callback query parsed");
    check (OAuthCallback::parse ("?canceled=true&state=s").canceled, "canceled callback recognised");

    // Loopback server: a simulated browser redirect gets the landing page and reaches onCallback.
    LoopbackServer server;
    juce::String received;
    server.onCallback = [&] (juce::String q) { received = q; };
    check (server.start(), "loopback server binds 127.0.0.1");
    const auto redirect = server.redirectUri();
    check (redirect.startsWith ("http://127.0.0.1:") && redirect.endsWith ("/callback"), "redirect URI is " + redirect);

    juce::Thread::launch ([redirect]
    {
        juce::URL (redirect + "?code=CODE&state=STATE&tone_id=7")
            .createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress).withConnectionTimeoutMs (3000));
    });
    HttpRequest page;
    page.url = juce::URL (redirect + "?code=PAGE&state=S");
    page.timeoutMs = 3000;
    juce::String html;
    juce::Thread::launch ([&] { html = sendSync (page).text(); });
    for (int i = 0; i < 60 && (received.isEmpty() || html.isEmpty()); ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    check (html.contains ("return to Abyssal"), "browser gets the landing page");
    check (OAuthCallback::parse (received).code.isNotEmpty(), "redirect query delivered on the message thread");
    server.stop();
    check (! server.isRunning(), "server stops cleanly");

    // Live endpoint check: an invalid code must come back as a readable OAuth error, not a crash.
    HttpRequest req;
    req.url = juce::URL ("https://www.tone3000.com/api/v1/oauth/token");
    req.method = "POST";
    req.headers.set ("Content-Type", "application/x-www-form-urlencoded");
    req.body = formEncode ({ { "grant_type", "authorization_code" }, { "code", "invalid" }, { "code_verifier", a.codeVerifier },
                             { "redirect_uri", "http://127.0.0.1:1/callback" }, { "client_id", "t3k_pub_invalid" } });
    const auto res = sendSync (req);
    std::cout << "  info  token endpoint answered HTTP " << res.status << ": " << res.text().substring (0, 160) << std::endl;
    check (res.status >= 400 && res.status < 500, "TONE3000 token endpoint reachable and rejects a bogus code");

    std::cout << (fails == 0 ? "TONE3000 OK" : "TONE3000 FAILURES") << std::endl;
    return fails == 0 ? 0 : 1;
}

using namespace abyssal;

struct DummyProcessor : juce::AudioProcessor
{
    DummyProcessor() : apvts (*this, nullptr, "PARAMS", createParameterLayout()) {}
    const juce::String getName() const override { return "dummy"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    juce::AudioProcessorValueTreeState apvts;
};

/** Karplus-Strong plucked string, with optional palm-mute damping. */
static void pluck (std::vector<float>& out, double sr, int start, double freq, double durSec, float amp, bool muted)
{
    const int period = std::max (2, (int) std::round (sr / freq));
    std::vector<float> ring ((size_t) period);
    juce::Random rng ((juce::int64) (freq * 1000 + start));
    for (auto& s : ring) s = rng.nextFloat() * 2.0f - 1.0f;

    const float damp = muted ? 0.35f : 0.4985f;     // averaging coefficient (0.5 = no loss)
    const int len = (int) (durSec * sr);
    float lp = 0;
    for (int i = 0; i < len && start + i < (int) out.size(); ++i)
    {
        const int idx = i % period;
        const int nxt = (idx + 1) % period;
        const float y = ring[(size_t) idx];
        ring[(size_t) idx] = damp * (y + ring[(size_t) nxt]);
        lp = lp + 0.5f * (y - lp);
        const float env = muted ? std::exp (-(float) i / (float) (0.09 * sr)) : 1.0f;
        const float release = i > len - 200 ? (float) (len - i) / 200.0f : 1.0f; // fret-hand mute
        out[(size_t) (start + i)] += amp * lp * env * release;
    }
}

static std::vector<float> makeRiff (double sr)
{
    std::vector<float> x ((size_t) (sr * 9.0), 0.0f);
    const double E2 = 82.41, B2 = 123.47, E3 = 164.81, F2 = 87.31, G2 = 98.0;
    const double eighth = 60.0 / 160.0 / 2.0; // 160 bpm

    // Gallop chugs on low E
    for (int i = 0; i < 16; ++i)
        pluck (x, sr, (int) (i * eighth * sr), E2, eighth * 0.95, 0.5f, true);

    // Power chords (open, ringing)
    const double t0 = 16 * eighth;
    const double chords[3] = { E2, G2, F2 };
    for (int c = 0; c < 3; ++c)
    {
        const int s = (int) ((t0 + c * 4 * eighth) * sr);
        pluck (x, sr, s, chords[c], 4 * eighth * 0.95, 0.35f, false);
        pluck (x, sr, s + 20, chords[c] * 1.4983, 4 * eighth * 0.95, 0.3f, false);
        pluck (x, sr, s + 40, chords[c] * 2.0, 4 * eighth * 0.95, 0.25f, false);
    }

    // Lead line
    const double t1 = t0 + 12 * eighth;
    const double lead[] = { 329.6, 392.0, 440.0, 493.9, 587.3, 659.3, 587.3, 493.9 };
    for (int i = 0; i < 8; ++i)
        pluck (x, sr, (int) ((t1 + i * eighth) * sr), lead[i], eighth * (i == 7 ? 4 : 1), 0.3f, false);

    // Normalise to -8 dBFS peak (typical hot DI), add a -75 dBFS noise floor for the gate.
    float pk = 0;
    for (auto s : x) pk = std::max (pk, std::abs (s));
    juce::Random rng (99);
    const float g = std::pow (10.0f, -8.0f / 20.0f) / pk;
    for (auto& s : x) s = s * g + (rng.nextFloat() * 2.0f - 1.0f) * 0.00018f;
    juce::ignoreUnused (E3, B2);
    return x;
}

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& buf, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    auto os = f.createOutputStream();
    if (os == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) buf.getNumChannels(), 24, {}, 0));
    if (w == nullptr) return false;
    os.release(); // the writer owns the stream now
    return w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}

/** Exercises FileLibrary against a throwaway library folder (Documents/Abyssal/_selftest). */
static int libraryTest (const juce::File& namExamples)
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl; if (! ok) ++fails; };

    FileLibrary lib ("_selftest", "*.nam");
    lib.getDirectory().deleteRecursively();

    // A "pack" folder with a nested sub-folder, plus a non-matching file that must be ignored.
    auto pack = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Abyssal Test Pack");
    pack.deleteRecursively();
    pack.getChildFile ("Cleans").createDirectory();
    auto models = namExamples.findChildFiles (juce::File::findFiles, false, "*.nam");
    models.sort();
    check (models.size() >= 3, "example .nam files found");
    models[0].copyFileTo (pack.getChildFile ("01 " + models[0].getFileName()));
    models[1].copyFileTo (pack.getChildFile ("02 " + models[1].getFileName()));
    models[2].copyFileTo (pack.getChildFile ("Cleans").getChildFile (models[2].getFileName()));
    pack.getChildFile ("readme.txt").replaceWithText ("not a capture");

    const auto imported = lib.importFolder (pack);
    check (imported.size() == 3, "importFolder copies 3 captures, skips readme.txt");
    check (lib.list().size() == 3, "list() sees 3 files");
    check (lib.getDirectory().getChildFile ("Abyssal Test Pack").getChildFile ("Cleans").isDirectory(), "sub-folder structure kept");
    check (lib.folderOf (imported[0]) == "Abyssal Test Pack", "folderOf() names the pack");

    const auto again = lib.importFolder (pack);
    check (lib.list().size() == 3, "re-importing the same folder doesn't duplicate");

    const auto single = lib.add (models[0]);
    check (single.isAChildOf (lib.getDirectory()) && lib.list().size() == 4, "add() copies a loose file into the root");
    check (lib.add (models[0]) == single && lib.list().size() == 4, "add() of an identical file reuses the copy");
    check (lib.add (single) == single, "add() of a library file is a no-op");

    const auto first = lib.getDirectory().getChildFile ("Abyssal Test Pack").getChildFile ("01 " + models[0].getFileName());
    const auto second = lib.getDirectory().getChildFile ("Abyssal Test Pack").getChildFile ("02 " + models[1].getFileName());
    check (lib.step (first, 1) == second, "step() moves to the next file in the pack");
    check (lib.step (second, 1) == first, "step() wraps within the pack");
    check (lib.step (juce::File(), 1).existsAsFile(), "step() from nothing picks a file");

    const auto nested = lib.getDirectory().getChildFile ("Abyssal Test Pack").getChildFile ("Cleans").getChildFile (models[2].getFileName());
    check (lib.remove (nested) && ! nested.existsAsFile(), "remove() moves the file to the Recycle Bin");
    check (! nested.getParentDirectory().isDirectory(), "remove() tidies the empty sub-folder");
    check (! lib.remove (models[0]), "remove() refuses files outside the library");
    check (models[0].existsAsFile(), "original files are untouched");

    lib.getDirectory().deleteRecursively();
    pack.deleteRecursively();
    juce::ignoreUnused (again);
    std::cout << (fails == 0 ? "LIBRARY OK" : "LIBRARY FAILURES") << std::endl;
    return fails == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 3 && juce::String (argv[1]) == "--vst3test")
        return vst3Test (juce::File (juce::String (argv[2])), juce::File (juce::String (argv[3])));
    if (argc > 1 && juce::String (argv[1]) == "--t3ktest")
        return tone3000Test();
    if (argc > 2 && juce::String (argv[1]) == "--libtest")
        return libraryTest (juce::File (juce::String (argv[2])));

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    const double sr = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 48000.0;
    const juce::String namPath = argc > 3 ? juce::String (argv[3]) : juce::String();
    outDir.createDirectory();

    const int block = 256;
    const auto di = makeRiff (sr);
    const int len = (int) di.size();

    {
        juce::AudioBuffer<float> diBuf (1, len);
        std::copy (di.begin(), di.end(), diBuf.getWritePointer (0));
        writeWav (outDir.getChildFile ("00_DI.wav"), diBuf, sr);
    }

    DummyProcessor proc;
    ParamCache cache (proc.apvts);
    int failures = 0;

    auto runPreset = [&] (const FactoryPreset& preset, bool useNam, int index,
                          const juce::String& orderOverride = {}, bool monoOut = false, const juce::String& label = {})
    {
        applyFactoryPreset (proc.apvts, preset);
        EngineParams params;
        cache.fill (params);
        params.bpm = 160.0;
        params.monoOut = monoOut;

        auto engine = std::make_unique<Engine>();
        engine->setChainOrder (orderOverride.isNotEmpty() ? ChainOrder::fromString (orderOverride)
                               : preset.chain.isEmpty() ? ChainOrder::defaultOrder() : ChainOrder::fromString (preset.chain));
        engine->prepare (sr, block);
        for (int s = 0; s < 2; ++s)
            engine->getCabinet().setSource (s, params.cab.slot[s].source);

        if (useNam)
        {
            const auto err = engine->getNam().loadModel (juce::File (namPath));
            if (err.isNotEmpty()) { std::cout << "NAM load failed: " << err << "\n"; ++failures; return; }
            params.useNam = true;
            std::cout << "  NAM model rate " << engine->getNam().getModelSampleRate() << " Hz, host " << sr << " Hz\n";
        }

        // Convolution IRs load on a background thread; give them a moment, then prime with silence.
        juce::Thread::sleep (400);
        juce::AudioBuffer<float> warm (2, block);
        for (int i = 0; i < 20; ++i) { warm.clear(); engine->process (warm, params); }

        juce::AudioBuffer<float> out (2, len);
        out.clear();
        std::copy (di.begin(), di.end(), out.getWritePointer (0));

        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int start = 0; start < len; start += block)
        {
            const int n = std::min (block, len - start);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, start, n);
            engine->process (view, params);
        }
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

        bool nan = false;
        double sumSq = 0; float pk = 0;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < len; ++i)
            {
                const float v = out.getSample (c, i);
                if (! std::isfinite (v)) nan = true;
                pk = std::max (pk, std::abs (v));
                sumSq += (double) v * v;
            }
        const double rms = std::sqrt (sumSq / (2.0 * len));
        const double audioMs = 1000.0 * len / sr;

        juce::String name = label.isNotEmpty() ? label : (useNam ? "NAM - " : "") + preset.name;
        std::cout << juce::String (name).paddedRight (' ', 24)
                  << " peak " << juce::String (juce::Decibels::gainToDecibels (pk), 1).paddedLeft (' ', 6) << " dBFS"
                  << "  rms " << juce::String (juce::Decibels::gainToDecibels ((float) rms), 1).paddedLeft (' ', 6) << " dBFS"
                  << "  cpu " << juce::String (100.0 * ms / audioMs, 1).paddedLeft (' ', 5) << "% of realtime"
                  << (nan ? "  ** NaN **" : "") << (pk < 1.0e-4f ? "  ** SILENT **" : "") << "\n";

        if (nan || pk < 1.0e-4f) ++failures;
        auto fname = juce::String (index).paddedLeft ('0', 2) + "_" + name.replaceCharacters (" -", "__") + ".wav";
        writeWav (outDir.getChildFile (fname), out, sr);
    };

    std::cout << "Rendering at " << sr << " Hz, block " << block << "\n";
    int idx = 1;
    for (const auto& p : getFactoryPresets())
        runPreset (p, false, idx++);
    if (namPath.isNotEmpty())
        runPreset (getFactoryPresets()[1], true, idx++);

    // Chain-order stress tests: every pedal enabled, mono pedals post-amp and stereo FX pre-amp.
    FactoryPreset everything { "Everything On", "Test", {},
        { { ids::gateOn, 1 }, { ids::octOn, 1 }, { ids::dropOn, 1 }, { ids::compOn, 1 }, { ids::odOn, 1 }, { ids::geqOn, 1 },
          { ids::choOn, 1 }, { ids::flOn, 1 }, { ids::dlyOn, 1 }, { ids::revOn, 1 } } };
    runPreset (everything, false, idx++, "", false, "All on - default order");
    runPreset (everything, false, idx++, "cho,fl,dly,rev,amp,gate,oct,drop,comp,od,geq", false, "All on - swapped sides");
    runPreset (everything, false, idx++, "amp,gate,oct,drop,comp,od,geq,cho,fl,dly,rev", false, "All on - all post-amp");
    runPreset (getFactoryPresets()[1], false, idx++, "", true, "Mono output");

    std::cout << (failures == 0 ? "OK" : "FAILURES: " + juce::String (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
