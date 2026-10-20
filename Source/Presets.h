#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace abyssal
{

struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values; // anything not listed uses its default
};

const std::vector<FactoryPreset>& getFactoryPresets();

/** Resets every parameter to its default, then applies the preset's values. */
void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset);

} // namespace abyssal
