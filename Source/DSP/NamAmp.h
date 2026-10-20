#pragma once

#include "Common.h"

namespace nam { class DSP; }

namespace abyssal
{

/** Hosts a Neural Amp Modeler (.nam) capture. Loading happens off the audio thread; the model is
    swapped in under a try-lock. If the host rate differs from the model's training rate the audio
    is streamed through a pair of Lagrange resamplers. */
class NamAmp
{
public:
    NamAmp();
    ~NamAmp();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    /** Loads a model (call from the message thread). Returns an error message, or empty on success. */
    juce::String loadModel (const juce::File& file);
    void unloadModel();

    bool hasModel() const noexcept { return modelLoaded.load(); }
    juce::File getModelFile() const { return modelFile; }
    double getModelSampleRate() const noexcept { return modelRate.load(); }

    /** Returns false if no model was available (caller should use the DSP amp instead). */
    bool process (float* data, int n, float inGainDb, float outGainDb);

private:
    struct Model;
    void configure (Model& m) const;
    void processModel (Model& m, float* data, int n, float inGain, float outGain);

    juce::SpinLock lock;
    std::unique_ptr<Model> model;
    juce::File modelFile;
    std::atomic<bool> modelLoaded { false };
    std::atomic<double> modelRate { 48000.0 };

    double hostRate = 48000.0;
    int hostBlock = 512;
};

} // namespace abyssal
