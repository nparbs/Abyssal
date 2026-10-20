#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace abyssal
{

struct FactoryPreset
{
    juce::String name, category;
    juce::String chain;                                  // pedal order; empty = default
    std::vector<std::pair<juce::String, float>> values;  // anything not listed uses its default
};

const std::vector<FactoryPreset>& getFactoryPresets();

/** Categories in display order. */
juce::StringArray getFactoryCategories();

/** Resets every (non-global) parameter to its default, then applies the preset's values.
    The pedal order is applied separately by the processor. */
void applyFactoryPreset (juce::AudioProcessorValueTreeState& apvts, const FactoryPreset& preset);

} // namespace abyssal
