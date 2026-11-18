#pragma once

#include <JuceHeader.h>
#include "ImportLimits.h"
#include <memory>
#include <vector>

namespace sliceforge
{
    struct SourceInfo
    {
        juce::String id;
        juce::String name;
        juce::String blurb;
        juce::String character;
    };

    const std::vector<SourceInfo>& factorySources();
    const SourceInfo* findFactorySource (const juce::String& id);

    struct SampleData
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 48000.0;
        juce::String name = "SOURCE";
        juce::String id = "custom";
        bool isFactory = false;

        int getNumSamples() const { return buffer.getNumSamples(); }
        int getNumChannels() const { return buffer.getNumChannels(); }
        double getDurationSeconds() const { return sampleRate > 0.0 ? getNumSamples() / sampleRate : 0.0; }

        std::vector<float> peakMin;
        std::vector<float> peakMax;

        void buildPeaks (int buckets = 3000);
    };

    using SamplePtr = std::shared_ptr<const SampleData>;

    SamplePtr loadFactorySource (const juce::String& id);
    SamplePtr loadAudioFile (const juce::File& file);
    SamplePtr decodeSampleFile (const void* data, size_t size, const juce::String& name);
    SamplePtr decodePcm16 (const juce::MemoryBlock& pcm, int numChannels, double sampleRate, const juce::String& name);

    juce::MemoryBlock encodePcm16 (const SampleData& sample);

    // Writes a stereo float buffer to a 16-bit WAV file. Used by one-shot export.
    bool writeStereoWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate);
} // namespace sliceforge
