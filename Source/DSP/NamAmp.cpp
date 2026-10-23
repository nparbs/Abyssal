#include "NamAmp.h"

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

namespace abyssal
{

/** Simple linear FIFO with preallocated storage (no allocation on the audio thread). */
struct LinearFifo
{
    std::vector<float> data;
    int count = 0;

    void allocate (int capacity) { data.assign ((size_t) capacity, 0.0f); count = 0; }
    int  capacity() const noexcept { return (int) data.size(); }

    void push (const float* src, int n) noexcept
    {
        if (count + n > capacity())          // overflow guard: drop the oldest samples
            consume (count + n - capacity());
        std::copy (src, src + n, data.begin() + count);
        count += n;
    }

    void consume (int n) noexcept
    {
        n = std::min (n, count);
        std::copy (data.begin() + n, data.begin() + count, data.begin());
        count -= n;
    }
};

struct NamAmp::Model
{
    std::unique_ptr<nam::DSP> dsp;
    double hostRate = 48000.0;   // host settings this model was configured for
    int hostBlock = 512;
    double rate = 48000.0;
    bool resample = false;
    float normGain = 1.0f;
    int maxModelBlock = 0;

    std::vector<NAM_SAMPLE> namIn, namOut, namOut2;
    std::vector<float> modelInF, modelOutF;
    LinearFifo inFifo, midFifo;
    juce::LagrangeInterpolator up, down;
};

NamAmp::NamAmp() = default;
NamAmp::~NamAmp() = default;

void NamAmp::configure (Model& m, double sampleRate, int blockSize) const
{
    m.hostRate = sampleRate;
    m.hostBlock = blockSize;
    m.rate = m.dsp->GetExpectedSampleRate() > 0 ? m.dsp->GetExpectedSampleRate() : 48000.0;
    m.resample = std::abs (m.rate - sampleRate) > 0.5;
    m.maxModelBlock = (int) std::ceil (blockSize * (m.resample ? m.rate / sampleRate : 1.0)) + 8;

    m.dsp->Reset (m.resample ? m.rate : sampleRate, m.maxModelBlock);

    m.namIn.assign ((size_t) m.maxModelBlock, 0.0);
    m.namOut.assign ((size_t) m.maxModelBlock, 0.0);
    m.namOut2.assign ((size_t) m.maxModelBlock, 0.0);
    m.modelInF.assign ((size_t) m.maxModelBlock, 0.0f);
    m.modelOutF.assign ((size_t) m.maxModelBlock, 0.0f);

    m.inFifo.allocate (blockSize * 2 + 64);
    m.midFifo.allocate (m.maxModelBlock * 3 + 128);
    static const float silence[32] = {};
    m.midFifo.push (silence, 32); // small cushion so the output side never underruns
    m.up.reset();
    m.down.reset();

    m.normGain = m.dsp->HasLoudness() ? dbToGain ((float) (-18.0 - m.dsp->GetLoudness())) : 1.0f;
}

void NamAmp::prepare (double sampleRate, int maxBlock)
{
    hostRate = sampleRate;
    hostBlock = maxBlock;
    const juce::SpinLock::ScopedLockType sl (lock);
    if (model)
        configure (*model, sampleRate, maxBlock);
}

void NamAmp::reset()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    if (model)
        configure (*model, hostRate, hostBlock);
}

NamAmp::PreparedModel NamAmp::prepareModel (const juce::File& file, juce::String& error) const
{
    auto fresh = std::make_shared<Model>();
    try
    {
        fresh->dsp = nam::get_dsp (std::filesystem::path (file.getFullPathName().toWideCharPointer()));
        if (fresh->dsp == nullptr)
        {
            error = "Could not load model";
            return nullptr;
        }
        configure (*fresh, hostRate, hostBlock);
    }
    catch (const std::exception& e)
    {
        error = juce::String ("Could not load model: ") + e.what();
        return nullptr;
    }
    return fresh;
}

void NamAmp::installModel (PreparedModel fresh, const juce::File& file)
{
    jassert (fresh != nullptr);
    modelRate = fresh->rate;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        // The host may have changed rate or block size while the model was being prepared.
        if (fresh->hostRate != hostRate || fresh->hostBlock != hostBlock)
            configure (*fresh, hostRate, hostBlock);
        std::swap (model, fresh);
    }
    modelFile = file;
    modelLoaded = true;
    // the previous model is destroyed here, outside the lock
}

juce::String NamAmp::loadModel (const juce::File& file)
{
    juce::String error;
    auto prepared = prepareModel (file, error);
    if (prepared == nullptr)
        return error;
    installModel (std::move (prepared), file);
    return {};
}

void NamAmp::unloadModel()
{
    std::shared_ptr<Model> old;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        std::swap (model, old);
    }
    modelFile = juce::File();
    modelLoaded = false;
}

bool NamAmp::process (float* data, int n, float inGainDb, float outGainDb)
{
    const juce::SpinLock::ScopedTryLockType tl (lock);
    if (! tl.isLocked() || model == nullptr)
        return false;

    const float inGain = dbToGain (inGainDb), outGain = dbToGain (outGainDb);
    const int block = model->hostBlock;
    for (int start = 0; start < n; start += block)
        processModel (*model, data + start, std::min (block, n - start), inGain, outGain);
    return true;
}

void NamAmp::processModel (Model& m, float* data, int n, float inGain, float outGain)
{
    const float post = outGain * m.normGain;
    NAM_SAMPLE* inPtr[] = { m.namIn.data() };
    NAM_SAMPLE* outPtr[] = { m.namOut.data(), m.namOut2.data() };

    auto runNam = [&] (const float* in, float* out, int count)
    {
        for (int i = 0; i < count; ++i) m.namIn[(size_t) i] = (NAM_SAMPLE) (in[i] * inGain);
        m.dsp->process (inPtr, outPtr, count);
        for (int i = 0; i < count; ++i) out[i] = (float) m.namOut[(size_t) i];
    };

    if (! m.resample)
    {
        runNam (data, data, n);
        for (int i = 0; i < n; ++i) data[i] *= post;
        return;
    }

    // host rate -> model rate
    m.inFifo.push (data, n);
    const double upSpeed = m.hostRate / m.rate; // input samples consumed per output sample
    int numModel = (int) std::floor ((m.inFifo.count - 2) / upSpeed);
    numModel = juce::jlimit (0, m.maxModelBlock, numModel);
    if (numModel > 0)
    {
        const int used = m.up.process (upSpeed, m.inFifo.data.data(), m.modelInF.data(), numModel);
        m.inFifo.consume (used);
        runNam (m.modelInF.data(), m.modelOutF.data(), numModel);
        m.midFifo.push (m.modelOutF.data(), numModel);
    }

    // model rate -> host rate
    const double downSpeed = m.rate / m.hostRate;
    if (m.midFifo.count >= (int) std::ceil (n * downSpeed) + 2)
    {
        const int used = m.down.process (downSpeed, m.midFifo.data.data(), data, n);
        m.midFifo.consume (used);
        for (int i = 0; i < n; ++i) data[i] *= post;
    }
    else
    {
        std::fill (data, data + n, 0.0f); // momentary underrun (only right after a reset)
    }
}

} // namespace abyssal
