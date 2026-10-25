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
#include <thread>

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

/** Background-load path: prepareModel() on a worker thread while the host changes rate and block
    size, then installModel() must reconfigure for the new settings and play cleanly. */
static int namLoadTest (const juce::File& model)
{
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl; if (! ok) ++fails; };

    abyssal::NamAmp nam;
    nam.prepare (48000.0, 512);

    juce::String error;
    abyssal::NamAmp::PreparedModel prepared;
    std::thread worker ([&] { prepared = nam.prepareModel (model, error); });
    nam.prepare (44100.0, 128); // the host changes settings mid-load
    worker.join();
    check (prepared != nullptr && error.isEmpty(), "prepareModel() on a worker thread: " + model.getFileName() + " " + error);
    if (prepared == nullptr) return 1;
    check (! nam.hasModel(), "nothing plays until the model is installed");

    nam.installModel (std::move (prepared), model);
    check (nam.hasModel() && nam.getModelFile() == model, "installModel() swaps the model in");

    std::vector<float> buf (128);
    float peak = 0.0f;
    bool finite = true, processed = true;
    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < 128; ++i)
            buf[(size_t) i] = 0.3f * std::sin (0.03f * (float) (b * 128 + i));
        processed = processed && nam.process (buf.data(), 128, 0.0f, 0.0f);
        for (float v : buf) { finite = finite && std::isfinite (v); peak = std::max (peak, std::abs (v)); }
    }
    check (processed && finite && peak > 1.0e-4f, "plays at the new 44.1 kHz / 128 settings (peak "
                                                  + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dB)");

    juce::String badError;
    check (nam.prepareModel (model.getSiblingFile ("does-not-exist.nam"), badError) == nullptr && badError.isNotEmpty(),
           "a missing file reports an error instead of a model");
    check (nam.hasModel() && nam.getModelFile() == model, "a failed load leaves the current model playing");

    std::cout << (fails == 0 ? "NAM LOAD OK" : "NAM LOAD FAILURES") << std::endl;
    return fails == 0 ? 0 : 1;
}

/** The v0.3 pitch shifter (fixed window, continuous sin^2 crossfade), kept to compare against. */
class LegacyPitchShifter
{
public:
    void prepare (double sampleRate, double windowMs)
    {
        window = (float) (windowMs * 0.001 * sampleRate);
        delay.prepare ((int) (window * 2.0f) + 16);
        delay.reset();
        phase = 0;
    }
    void setRatio (float r) noexcept { ratio = r; }
    float process (float x) noexcept
    {
        delay.push (x);
        phase += (1.0f - ratio) / window;
        if (phase >= 1.0f) phase -= 1.0f;
        if (phase < 0.0f)  phase += 1.0f;
        float phase2 = phase + 0.5f;
        if (phase2 >= 1.0f) phase2 -= 1.0f;
        const float s1 = std::sin (juce::MathConstants<float>::pi * phase);
        const float s2 = std::sin (juce::MathConstants<float>::pi * phase2);
        return s1 * s1 * delay.read (phase * window + 2.0f) + s2 * s2 * delay.read (phase2 * window + 2.0f);
    }

private:
    FracDelay delay;
    float window = 2048, phase = 0, ratio = 1;
};

/** Offline DSP checks that need no files: tuner accuracy, resampler latency, state migration, gate keying. */
static int dspTest()
{
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { std::cout << (ok ? "  pass  " : "  FAIL  ") << what << std::endl; if (! ok) ++fails; };

    // ---- tuner: decaying harmonic-rich notes from a drop-A 8-string up to the high E's 12th fret
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        float worst = 0.0f;
        bool allFound = true;
        for (float hz : { 30.87f, 41.2f, 46.25f, 55.0f, 82.41f, 110.0f, 146.83f, 196.0f, 246.94f, 329.63f, 659.26f })
        {
            TunerTap tap;
            tap.prepare (sr);
            std::vector<float> x ((size_t) (sr * 0.6));
            juce::Random rng (7);
            for (size_t i = 0; i < x.size(); ++i)
            {
                const double t = (double) i / sr;
                float v = 0.0f;
                for (int h = 1; h <= 8; ++h)
                    v += (float) (std::sin (juce::MathConstants<double>::twoPi * hz * h * t + h) / (h * (h == 2 ? 0.7 : 1.0)));
                x[i] = 0.3f * v * std::exp (-(float) t * 2.0f) + (rng.nextFloat() - 0.5f) * 0.002f;
            }
            tap.push (x.data(), (int) x.size());

            std::vector<float> buf (2048);
            tap.read (buf.data(), (int) buf.size());
            PitchDetector det;
            const auto r = det.detect (buf.data(), (int) buf.size(), tap.getRate());
            const auto note = NoteReading::fromFrequency (r.frequency);
            const auto expected = NoteReading::fromFrequency (hz);
            if (r.frequency <= 0.0f || note.midiNote != expected.midiNote) allFound = false;
            else worst = std::max (worst, std::abs (note.cents - expected.cents));
            if (std::abs (note.cents - expected.cents) > 0.5f || note.midiNote != expected.midiNote)
                std::cout << "        " << hz << " Hz read as " << r.frequency << " Hz (" << (note.cents - expected.cents) << " cents)" << std::endl;
        }
        check (allFound && worst < 1.0f, "tuner at " + juce::String (sr / 1000.0, 1) + " kHz finds B0..E5, worst error "
                                         + juce::String (worst, 2) + " cents");
    }
    {
        const auto n = NoteReading::fromFrequency (432.0f, 432.0f);
        check (n.name == "A" && n.octave == 4 && std::abs (n.cents) < 0.01f, "tuner reference A4 = 432 Hz");
        PitchDetector det;
        std::vector<float> silence (2048, 0.0f);
        check (det.detect (silence.data(), 2048, 12000.0).frequency == 0.0f, "tuner shows nothing for silence");
    }

    // ---- NAM resampler latency: an identity "capture" at another rate; a band-limited pulse goes in
    // (a bare impulse can fall between the samples a 2:1 down-sampler keeps) and is found at the output
    for (auto [host, model, block] : { std::tuple { 44100.0, 48000.0, 256 }, { 48000.0, 44100.0, 512 },
                                       { 96000.0, 48000.0, 128 }, { 88200.0, 48000.0, 64 }, { 48000.0, 96000.0, 480 } })
    {
        NamAmp nam;
        nam.prepare (host, block);
        nam.installIdentityModel (model);
        std::vector<float> buf ((size_t) block);
        const int pulseAt = block * 4 + 17, halfWidth = 16;
        std::vector<float> out;
        for (int b = 0; b < 16; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const int d = std::abs (b * block + i - pulseAt);
                buf[(size_t) i] = d < halfWidth ? 0.5f + 0.5f * std::cos (juce::MathConstants<float>::pi * (float) d / (float) halfWidth) : 0.0f;
            }
            nam.process (buf.data(), block, 0.0f, 0.0f);
            out.insert (out.end(), buf.begin(), buf.end());
        }
        int foundAt = 1;
        for (int i = 1; i + 1 < (int) out.size(); ++i)
            if (out[(size_t) i] > out[(size_t) foundAt]) foundAt = i;
        const float a = out[(size_t) foundAt - 1], c = out[(size_t) foundAt + 1], m = out[(size_t) foundAt];
        const float peak = (float) foundAt + 0.5f * (a - c) / std::min (-1.0e-9f, a - 2.0f * m + c);
        const int measured = juce::roundToInt (peak - (float) pulseAt);
        check (std::abs (measured - nam.getLatencySamples()) <= 1,
               "NAM resampler " + juce::String (model / 1000.0, 1) + " kHz capture at " + juce::String (host / 1000.0, 1)
               + " kHz / " + juce::String (block) + ": reports " + juce::String (nam.getLatencySamples())
               + ", measured " + juce::String (measured));
    }
    {
        NamAmp nam;
        nam.prepare (48000.0, 256);
        nam.installIdentityModel (48000.0);
        check (nam.getLatencySamples() == 0, "no resampler latency when the rates match");
    }

    // ---- pitch shifter vs. the v0.3 one: sustained notes and chords, shifted down
    {
        const double sr = 48000.0;
        struct Case { const char* name; float ratio; std::vector<float> notes; };
        const std::vector<Case> cases {
            { "drop -2: low E",          std::pow (2.0f, -2.0f / 12.0f), { 82.41f } },
            { "drop -2: power chord",    std::pow (2.0f, -2.0f / 12.0f), { 82.41f, 123.47f, 164.81f } },
            { "drop -2: open E chord",   std::pow (2.0f, -2.0f / 12.0f), { 82.41f, 123.47f, 164.81f, 207.65f, 246.94f, 329.63f } },
            { "drop -5: high lead note", std::pow (2.0f, -5.0f / 12.0f), { 659.26f } },
            { "octave down: A string",   0.5f,                            { 110.0f } },
            { "octave down: power chord", 0.5f,                           { 110.0f, 164.81f, 220.0f } },
            { "octave up: lead note",    2.0f,                            { 329.63f } },
            { "fifth up: power chord",   std::pow (2.0f, 7.0f / 12.0f),   { 82.41f, 123.47f, 164.81f } },
            { "two octaves up: G string", 4.0f,                           { 196.0f } },
        };

        // Fraction of energy within 1.5% of the shifted partials, envelope ripple, and average delay.
        struct Stats { double purity = 0, ripple = 0, delayMs = 0; };
        auto analyse = [&] (const std::vector<float>& out, const Case& c)
        {
            Stats st;
            constexpr int order = 16, size = 1 << order;
            const int start = (int) (0.6 * sr);
            juce::dsp::FFT fft (order);
            std::vector<float> data ((size_t) size * 2, 0.0f);
            for (int i = 0; i < size; ++i)
                data[(size_t) i] = out[(size_t) (start + i)] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) size));
            fft.performFrequencyOnlyForwardTransform (data.data());
            double total = 0, onTarget = 0;
            for (int b = 1; b < size / 2; ++b)
            {
                const double f = b * sr / size, e = (double) data[(size_t) b] * data[(size_t) b];
                total += e;
                bool hit = false;
                for (float n : c.notes)
                    for (int h = 1; h <= 8 && ! hit; ++h)
                        hit = std::abs (f - n * h * c.ratio) < std::max (3.0, 0.015 * n * h * c.ratio);
                if (hit) onTarget += e;
            }
            st.purity = onTarget / total;

            // Envelope in 20 ms blocks: standard deviation relative to the mean.
            const int blk = (int) (0.02 * sr);
            std::vector<double> env;
            for (int i = start; i + blk < start + size; i += blk)
            {
                double s = 0;
                for (int k = 0; k < blk; ++k) s += (double) out[(size_t) (i + k)] * out[(size_t) (i + k)];
                env.push_back (std::sqrt (s / blk));
            }
            double mean = 0, var = 0;
            for (auto e : env) mean += e;
            mean /= (double) env.size();
            for (auto e : env) var += (e - mean) * (e - mean);
            st.ripple = std::sqrt (var / (double) env.size()) / mean;
            return st;
        };

        for (const auto& c : cases)
        {
            const int len = (int) (2.2 * sr);
            std::vector<float> in ((size_t) len), ideal ((size_t) len), outNew ((size_t) len), outOld ((size_t) len);
            for (int i = 0; i < len; ++i)
            {
                double v = 0, w = 0;
                for (float n : c.notes)
                    for (int h = 1; h <= 8; ++h)
                    {
                        v += std::sin (juce::MathConstants<double>::twoPi * n * h * i / sr + n + h) / (h * (double) c.notes.size());
                        w += std::sin (juce::MathConstants<double>::twoPi * n * c.ratio * h * i / sr + n + h) / (h * (double) c.notes.size());
                    }
                in[(size_t) i] = (float) (0.4 * v);
                ideal[(size_t) i] = (float) (0.4 * w); // a perfect shift, for the envelope ripple the chord has anyway
            }
            PitchShifter fresh;
            fresh.prepare (sr, 30.0);
            fresh.setRatio (c.ratio);
            LegacyPitchShifter legacy;
            legacy.prepare (sr, c.ratio < 0.6f ? 55.0 : 35.0);
            legacy.setRatio (c.ratio);
            double delaySum = 0;
            for (int i = 0; i < len; ++i)
            {
                outNew[(size_t) i] = fresh.process (in[(size_t) i]);
                outOld[(size_t) i] = legacy.process (in[(size_t) i]);
                if (i > sr * 0.6) delaySum += fresh.getAverageDelay();
            }
            auto sNew = analyse (outNew, c), sOld = analyse (outOld, c);
            const double inherent = analyse (ideal, c).ripple;
            sNew.delayMs = delaySum / (len - sr * 0.6) / sr * 1000.0;
            sOld.delayMs = (c.ratio < 0.6f ? 55.0 : 35.0) * 0.5;
            check (sNew.purity >= sOld.purity - 0.01 && sNew.ripple <= std::max (sOld.ripple, inherent) + 0.03 && sNew.delayMs < sOld.delayMs,
                   juce::String (c.name) + ": on-pitch energy " + juce::String (100 * sOld.purity, 1) + "% -> "
                   + juce::String (100 * sNew.purity, 1) + "%, warble " + juce::String (100 * sOld.ripple, 1) + "% -> "
                   + juce::String (100 * sNew.ripple, 1) + "% (perfect shift " + juce::String (100 * inherent, 1) + "%), delay "
                   + juce::String (sOld.delayMs, 1) + " -> "
                   + juce::String (sNew.delayMs, 1) + " ms");
        }
    }

    // ---- cab slot alignment: an IR with 3 ms of leading silence lines up with a built-in when aligned
    {
        const double sr = 48000.0;
        auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("abyssal_late_ir.wav");
        {
            auto ir = Cabinet::makeBuiltInIR (1, sr);
            juce::AudioBuffer<float> late (1, ir.getNumSamples() + 144);
            late.clear();
            late.copyFrom (0, 144, ir, 0, 0, ir.getNumSamples());
            writeWav (wav, late, sr);
        }
        auto measure = [&] (bool align)
        {
            Cabinet cab;
            cab.prepare (sr, 256);
            cab.setAlign (align);
            cab.setSource (0, 0);
            cab.loadUserIR (1, wav);
            juce::Thread::sleep (300); // convolution IRs load in the background
            CabParams p;
            p.align = align;
            p.slot[1].on = true;
            p.slot[0].pan = -1.0f; // A hard left, B hard right
            p.slot[1].pan = 1.0f;
            std::vector<float> in (256, 0.0f), l (256), r (256);
            int peakL = -1, peakR = -1;
            float bestL = 0, bestR = 0;
            for (int b = 0; b < 40; ++b)
            {
                std::fill (in.begin(), in.end(), 0.0f);
                if (b == 20) in[0] = 1.0f;
                cab.process (in.data(), l.data(), r.data(), 256, p);
                if (b >= 20)
                    for (int i = 0; i < 256; ++i)
                    {
                        if (std::abs (l[(size_t) i]) > bestL) { bestL = std::abs (l[(size_t) i]); peakL = (b - 20) * 256 + i; }
                        if (std::abs (r[(size_t) i]) > bestR) { bestR = std::abs (r[(size_t) i]); peakR = (b - 20) * 256 + i; }
                    }
            }
            return std::pair { peakR - peakL, cab.getTrim (1) };
        };
        const auto aligned = measure (true), natural = measure (false);
        check (std::abs (aligned.first) <= 6 && std::abs (natural.first - 144) <= 6,
               "cab alignment: late IR " + juce::String (aligned.first) + " samples off when aligned, "
               + juce::String (natural.first) + " when not (trims " + juce::String (aligned.second) + " / " + juce::String (natural.second) + ")");
        wav.deleteFile();
    }

    // ---- pedal orders saved before v0.5 gain the new pedals in their default spots
    {
        const auto upgraded = ChainOrder::fromString ("gate,oct,drop,comp,od,geq,amp,cho,fl,dly,rev");
        check (upgraded == ChainOrder::defaultOrder(), "v0.4 default order upgrades to the v0.5 default: " + upgraded.toString());
        const auto custom = ChainOrder::fromString ("comp,gate,oct,drop,od,geq,amp,dly,cho,fl,rev");
        check (custom.toString() == "wah,wham,comp,gate,oct,drop,od,geq,amp,pha,dly,cho,fl,trem,dbl,rev",
               "a custom v0.4 order keeps its pedals in place: " + custom.toString());
        check (ChainOrder::unpack (ChainOrder::defaultOrder().pack()) == ChainOrder::defaultOrder(), "16-slot order packs into 64 bits");
    }

    // ---- reverb modes decay, even at the extremes (the shimmer loop must not sustain itself)
    for (int mode = 0; mode < 3; ++mode)
    {
        const double sr = 48000.0;
        Reverb rev;
        rev.prepare (sr, 256);
        ReverbParams p;
        p.on = true;
        p.mode = mode;
        p.size = 1.0f;
        p.mix = 1.0f;
        p.shimmer = 1.0f;
        p.damping = 0.2f;
        juce::Random rng (11);
        std::vector<float> l (256), r (256);
        double firstSecond = 0, lastSecond = 0;
        const int blocks = (int) (12.0 * sr / 256);
        for (int b = 0; b < blocks; ++b)
        {
            for (int i = 0; i < 256; ++i)
                l[(size_t) i] = r[(size_t) i] = b * 256 < (int) (0.3 * sr) ? 0.5f * (rng.nextFloat() - 0.5f) : 0.0f;
            rev.process (l.data(), r.data(), 256, p);
            double e = 0;
            for (int i = 0; i < 256; ++i) e += (double) l[(size_t) i] * l[(size_t) i];
            if (b * 256 < (int) sr) firstSecond += e;
            if (b * 256 >= (int) (11.0 * sr)) lastSecond += e;
        }
        const double dropDb = 10.0 * std::log10 ((firstSecond + 1e-20) / (lastSecond + 1e-20));
        check (dropDb > 40.0, juce::String (ReverbParams::modeNames()[mode]) + " reverb at max size decays "
                              + juce::String (dropDb, 1) + " dB over 11 s");
    }

    // ---- passive tone stack: solved circuit behaves like a TMB network
    for (int ch = 0; ch < AmpModel::numChannels; ++ch)
    {
        const auto& c = AmpModel::getVoicing (ch).stack;
        auto db = [&c] (float t, float m, float b, double f) { return 20.0 * std::log10 (ToneStack::magnitude (c, t, m, b, f)); };
        double maxGain = -100.0;
        for (float t : { 0.0f, 5.0f, 10.0f })
            for (float m : { 0.0f, 5.0f, 10.0f })
                for (float b : { 0.0f, 5.0f, 10.0f })
                    for (double f = 20.0; f < 20000.0; f *= 1.1)
                        maxGain = std::max (maxGain, db (t, m, b, f));
        const bool knobsWork = db (5, 5, 10, 80) > db (5, 5, 0, 80) + 6.0
                            && db (5, 10, 5, 500) > db (5, 0, 5, 500) + 4.0 // a passive mid pot has a modest range
                            && db (10, 5, 5, 4000) > db (0, 5, 5, 4000) + 6.0;
        std::cout << "        knob ranges: bass@80 " << juce::String (db (5, 5, 10, 80) - db (5, 5, 0, 80), 1)
                  << " mid@500 " << juce::String (db (5, 10, 5, 500) - db (5, 0, 5, 500), 1)
                  << " treble@4k " << juce::String (db (10, 5, 5, 4000) - db (0, 5, 5, 4000), 1) << std::endl;
        const double scoop = db (5, 5, 5, 600) - 0.5 * (db (5, 5, 5, 100) + db (5, 5, 5, 4000));
        std::cout << "        stack " << ch << " at noon: 100 Hz " << juce::String (db (5, 5, 5, 100), 1) << " dB, 600 Hz "
                  << juce::String (db (5, 5, 5, 600), 1) << " dB, 4 kHz " << juce::String (db (5, 5, 5, 4000), 1)
                  << " dB, makeup " << juce::String (ToneStack::noonMakeupDb (c), 1) << " dB" << std::endl;
        check (maxGain < 0.1 && knobsWork && scoop < -2.0,
               "tone stack " + juce::String (ch) + ": passive (max " + juce::String (maxGain, 2) + " dB), each knob works, mid scoop "
               + juce::String (scoop, 1) + " dB at noon");

        // The digital filter matches the analog circuit (oversampled rate, as in the amp).
        ToneStack ts;
        const double fs = 192000.0;
        ts.setup (c, fs, 7.0f, 3.0f, 6.0f);
        double worst = 0.0;
        for (double f : { 60.0, 250.0, 1000.0, 3000.0, 8000.0 })
        {
            ts.reset();
            double sumIn = 0, sumOut = 0;
            const int len = (int) fs;
            for (int i = 0; i < len; ++i)
            {
                const float x = (float) std::sin (juce::MathConstants<double>::twoPi * f * i / fs);
                const float yv = ts.process (x);
                if (i > len / 2) { sumIn += (double) x * x; sumOut += (double) yv * yv; }
            }
            const double measured = 10.0 * std::log10 (sumOut / sumIn);
            worst = std::max (worst, std::abs (measured - 20.0 * std::log10 (ToneStack::magnitude (c, 7.0f, 3.0f, 6.0f, f))));
        }
        check (worst < 0.2, "tone stack " + juce::String (ch) + " digital matches analog within " + juce::String (worst, 3) + " dB");
    }

    // ---- reported latency follows drop tune
    {
        DummyProcessor proc;
        ParamCache cache (proc.apvts);
        Engine engine;
        engine.prepare (48000.0, 256);
        EngineParams p;
        cache.fill (p);
        juce::AudioBuffer<float> buf (2, 256);
        buf.clear();
        engine.process (buf, p);
        const int base = engine.getLatencySamples();
        p.drop.on = true;
        p.drop.semitones = -2;
        engine.process (buf, p);
        const int withDrop = engine.getLatencySamples();
        check (withDrop > base + 400 && withDrop < base + 600, "latency " + juce::String (base) + " -> " + juce::String (withDrop)
                                                                 + " samples with drop tune on");
    }

    // ---- v0.2 state migration: the shared faceplate moves to the selected channel
    {
        DummyProcessor proc;
        auto state = proc.apvts.copyState();
        state.setProperty ("version", 2, nullptr);
        auto setParam = [&state] (const juce::String& id, float v)
        {
            for (auto child : state)
                if (child.getProperty ("id").toString() == id) child.setProperty ("value", v, nullptr);
        };
        setParam (ids::ampChannel, 1);
        setParam ("amp_gain", 3.25f);
        setParam ("amp_bright", 1.0f);
        migrateState (state);
        proc.apvts.replaceState (state);
        check (std::abs (proc.apvts.getRawParameterValue (ids::ampKnob (1, "gain"))->load() - 3.25f) < 0.01f
                   && proc.apvts.getRawParameterValue (ids::ampKnob (1, "bright"))->load() > 0.5f,
               "v0.2 Crunch settings land on the Crunch channel");

        auto v3 = proc.apvts.copyState();
        v3.setProperty ("version", 3, nullptr);
        for (auto child : v3)
            if (child.getProperty ("id").toString() == ids::cab (1, "src")) child.setProperty ("value", 3, nullptr);
        migrateState (v3);
        proc.apvts.replaceState (v3);
        check ((int) proc.apvts.getRawParameterValue (ids::cab (1, "src"))->load() == Cabinet::userSource,
               "v0.3 'User IR' cab source moves past the new built-in cabs");

        const auto& factory = getFactoryPresets();
        for (auto& preset : factory)
            if (preset.name == "Ambient Clean")
            {
                applyFactoryPreset (proc.apvts, preset);
                check (proc.apvts.getRawParameterValue (ids::ampKnob (0, "bright"))->load() > 0.5f
                           && std::abs (proc.apvts.getRawParameterValue (ids::ampKnob (0, "gain"))->load() - 4.0f) < 0.01f
                           && std::abs (proc.apvts.getRawParameterValue (ids::ampKnob (2, "gain"))->load() - 6.5f) < 0.01f,
                       "factory Clean preset sets the Clean channel, Lead keeps its defaults");
            }
    }

    // ---- gate keyed from the input: closes on the quiet DI even after a cranked amp
    {
        DummyProcessor proc;
        ParamCache cache (proc.apvts);
        applyFactoryPreset (proc.apvts, getFactoryPresets()[1]);
        auto measureTail = [&] (bool keyFromInput)
        {
            EngineParams p;
            cache.fill (p);
            p.gate.on = true;
            p.gate.threshDb = -40.0f;
            p.gate.keyFromInput = keyFromInput;
            Engine engine;
            engine.setChainOrder (ChainOrder::fromString ("oct,drop,comp,od,geq,amp,gate,cho,fl,dly,rev"));
            engine.prepare (48000.0, 256);
            juce::AudioBuffer<float> buf (2, 256);
            juce::Random rng (3);
            float tailPeak = 0.0f;
            for (int b = 0; b < 400; ++b)
            {
                const bool playing = b < 150;
                for (int i = 0; i < 256; ++i)
                {
                    const float noise = (rng.nextFloat() - 0.5f) * 0.004f; // about -54 dBFS of hum and hiss
                    const float note = playing ? 0.3f * std::sin (0.02f * (float) (b * 256 + i)) : 0.0f;
                    buf.setSample (0, i, note + noise);
                    buf.setSample (1, i, note + noise);
                }
                engine.process (buf, p);
                if (b > 250) tailPeak = std::max (tailPeak, buf.getMagnitude (0, 0, 256));
            }
            return juce::Decibels::gainToDecibels (tailPeak);
        };
        const float ownKey = measureTail (false), inputKey = measureTail (true);
        check (inputKey < -90.0f && inputKey < ownKey - 20.0f,
               "post-amp gate keyed from the input: noise tail " + juce::String (inputKey, 1) + " dB (own key: "
               + juce::String (ownKey, 1) + " dB)");
    }

    std::cout << (fails == 0 ? "DSP OK" : "DSP FAILURES") << std::endl;
    return fails == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc > 3 && juce::String (argv[1]) == "--vst3test")
        return vst3Test (juce::File (juce::String (argv[2])), juce::File (juce::String (argv[3])));
    if (argc > 1 && juce::String (argv[1]) == "--dsptest")
        return dspTest();
    if (argc > 1 && juce::String (argv[1]) == "--t3ktest")
        return tone3000Test();
    if (argc > 2 && juce::String (argv[1]) == "--libtest")
        return libraryTest (juce::File (juce::String (argv[2])));
    if (argc > 2 && juce::String (argv[1]) == "--namtest")
        return namLoadTest (juce::File (juce::String (argv[2])));

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
          { ids::choOn, 1 }, { ids::flOn, 1 }, { ids::dlyOn, 1 }, { ids::revOn, 1 },
          { ids::wahOn, 1 }, { ids::wahMode, 1 }, { ids::whamOn, 1 }, { ids::whamMix, 0.5f }, { ids::phaOn, 1 },
          { ids::tremOn, 1 }, { ids::dblOn, 1 } } };
    runPreset (everything, false, idx++, "", false, "All on - default order");
    runPreset (everything, false, idx++, "cho,fl,dly,rev,amp,gate,oct,drop,comp,od,geq", false, "All on - swapped sides");
    runPreset (everything, false, idx++, "amp,gate,oct,drop,comp,od,geq,cho,fl,dly,rev", false, "All on - all post-amp");
    runPreset (getFactoryPresets()[1], false, idx++, "", true, "Mono output");

    // Every amp channel and power tube type at default knobs (levels should be in the same ballpark).
    const char* channelNames[] = { "Clean", "Crunch", "Lead", "Abyss" };
    for (int ch = 0; ch < ids::numAmpChannels; ++ch)
        for (int tube = 0; tube < 2; ++tube)
        {
            FactoryPreset p { "Channel", "Test", {}, { { ids::ampChannel, (float) ch }, { ids::ampPower, (float) tube } } };
            runPreset (p, false, idx++, "", false, juce::String ("Channel ") + channelNames[ch] + (tube == 0 ? " EL34" : " 6L6"));
        }

    // Mic types and distance on the Modern 4x12.
    for (int type = 0; type < 3; ++type)
    {
        FactoryPreset p { "Mic", "Test", {}, { { ids::cab (0, "mtype"), (float) type }, { ids::cab (0, "dist"), type == 0 ? 0.0f : 0.6f } } };
        runPreset (p, false, idx++, "", false, juce::String ("Mic ") + Cabinet::getMicNames()[type] + (type == 0 ? "" : " far"));
    }
    // The v0.5 effects, one at a time on the default sound.
    const std::vector<std::pair<juce::String, std::vector<std::pair<juce::String, float>>>> fx {
        { "FX Auto wah",          { { ids::wahOn, 1 }, { ids::wahMode, 1 }, { ids::wahSens, 0.6f } } },
        { "FX Wah half-cocked",   { { ids::wahOn, 1 }, { ids::wahPos, 0.6f }, { ids::wahQ, 0.8f } } },
        { "FX Whammy +1 oct",     { { ids::whamOn, 1 } } },
        { "FX Whammy -1 oct harmony", { { ids::whamOn, 1 }, { ids::whamInterval, 6 }, { ids::whamMix, 0.5f } } },
        { "FX Whammy +2 oct",     { { ids::whamOn, 1 }, { ids::whamInterval, 0 } } },
        { "FX Phaser 8 stage",    { { ids::phaOn, 1 }, { ids::phaStages, 1 }, { ids::phaFeedback, 0.7f } } },
        { "FX Tremolo pan sync",  { { ids::tremOn, 1 }, { ids::tremPan, 1 }, { ids::tremSync, 1 }, { ids::tremShape, 0.8f } } },
        { "FX Doubler",           { { ids::dblOn, 1 } } },
        { "FX Tape delay",        { { ids::dlyOn, 1 }, { ids::dlyMode, 1 }, { ids::dlyFeedback, 0.6f }, { ids::dlyMix, 0.4f } } },
        { "FX Analog delay",      { { ids::dlyOn, 1 }, { ids::dlyMode, 2 }, { ids::dlyFeedback, 0.6f }, { ids::dlyMix, 0.4f } } },
        { "FX Plate reverb",      { { ids::revOn, 1 }, { ids::revMode, 1 }, { ids::revMix, 0.4f } } },
        { "FX Hall reverb",       { { ids::revOn, 1 }, { ids::revMix, 0.4f } } },
        { "FX Shimmer max",       { { ids::revOn, 1 }, { ids::revMode, 2 }, { ids::revShimmer, 1 }, { ids::revSize, 1 }, { ids::revMix, 1 } } },
    };
    for (auto& [name, values] : fx)
        runPreset ({ name, "Test", {}, values }, false, idx++, "", false, name);

    for (int cab = 3; cab < Cabinet::numBuiltIns; ++cab)
    {
        FactoryPreset p { "Cab", "Test", {}, { { ids::cab (0, "src"), (float) cab } } };
        runPreset (p, false, idx++, "", false, "Cab " + Cabinet::getSourceNames()[cab]);
    }

    std::cout << (failures == 0 ? "OK" : "FAILURES: " + juce::String (failures)) << "\n";
    return failures == 0 ? 0 : 1;
}
