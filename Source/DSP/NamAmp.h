#pragma once

#include "Common.h"

namespace nam { class DSP; }

namespace abyssal
{

/** Hosts a Neural Amp Modeler (.nam) capture. Loading happens off the audio thread; the model is
    swapped in under a try-lock. If the host rate differs from the model's training rate the audio
    is streamed through a pair of Lagrange resamplers.

    Loading is split in two so big models don't freeze the UI: prepareModel() does the slow part
    (parsing, building and warming up the network) on any thread, and installModel() swaps the
    result in on the message thread. */
class NamAmp
{
public:
    struct Model;
    using PreparedModel = std::shared_ptr<Model>;

    NamAmp();
    ~NamAmp();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** Builds a model ready to install. Safe on a worker thread. Returns nullptr and sets `error`
        on failure. */
    PreparedModel prepareModel (const juce::File& file, juce::String& error) const;
    /** Swaps a prepared model in (message thread). The previous model is freed here. */
    void installModel (PreparedModel prepared, const juce::File& file);

    /** Prepares and installs in one go (message thread). Returns an error message, or empty on success. */
    juce::String loadModel (const juce::File& file);
    void unloadModel();

    bool hasModel() const noexcept { return modelLoaded.load(); }
    juce::File getModelFile() const { return modelFile; }
    /** Records where the loaded model lives (e.g. its library copy). Message thread only. */
    void setModelFile (const juce::File& f) { modelFile = f; }
    double getModelSampleRate() const noexcept { return modelRate.load(); }

    /** Returns false if no model was available (caller should use the DSP amp instead). */
    bool process (float* data, int n, float inGainDb, float outGainDb);

private:
    void configure (Model& m, double sampleRate, int blockSize) const;
    void processModel (Model& m, float* data, int n, float inGain, float outGain);

    juce::SpinLock lock;
    std::shared_ptr<Model> model;
    juce::File modelFile;
    std::atomic<bool> modelLoaded { false };
    std::atomic<double> modelRate { 48000.0 };

    // Written in prepare(); read by prepareModel() on the loader thread.
    std::atomic<double> hostRate { 48000.0 };
    std::atomic<int> hostBlock { 512 };
};

} // namespace abyssal
