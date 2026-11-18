#include "SampleData.h"
#include "BinaryData.h"

namespace sliceforge
{
    const std::vector<SourceInfo>& factorySources()
    {
        static const std::vector<SourceInfo> sources {
            { "foley", "FOLEY TRASH", "Dustbin lids, broken wood and a squeaky hinge.", "Cinematic / dry" },
            { "kitchen", "KITCHEN PERC", "Pots, cutlery, glass and a stubborn drawer.", "Tactile / bright" },
            { "body", "BODY MUSIC", "Chest thumps, clicks, whispers and cheek pops.", "Warm / human" },
            { "scrap", "SCRAP METAL", "Sheet steel, springs, chains and a grinder.", "Abrasive / wide" },
            { "circuit", "CIRCUIT NOISE", "Relay clicks, coil whine and failing capacitors.", "Electronic / tight" },
            { "vinyl", "VINYL DUST", "Crackle, pops, run-out groove and a stuck note.", "Dusty / lo-fi" }
        };
        return sources;
    }

    const SourceInfo* findFactorySource (const juce::String& id)
    {
        for (const auto& source : factorySources())
            if (source.id == id)
                return &source;
        return nullptr;
    }

    void SampleData::buildPeaks (int buckets)
    {
        const int count = getNumSamples();
        if (count <= 0)
            return;
        buckets = juce::jlimit (1, count, buckets);
        peakMin.assign ((size_t) buckets, 0.0f);
        peakMax.assign ((size_t) buckets, 0.0f);
        const double per = (double) count / buckets;
        const auto* left = buffer.getReadPointer (0);
        const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;
        for (int b = 0; b < buckets; ++b)
        {
            const int start = (int) std::floor (b * per);
            const int end = juce::jmin (count, (int) std::ceil ((b + 1) * per));
            float lo = 0.0f, hi = 0.0f;
            for (int i = start; i < end; ++i)
            {
                const auto v = (left[i] + right[i]) * 0.5f;
                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
            }
            peakMin[(size_t) b] = lo;
            peakMax[(size_t) b] = hi;
        }
    }

    namespace
    {
        SamplePtr decodeReader (std::unique_ptr<juce::AudioFormatReader> reader, const juce::String& name, bool factory, const juce::String& id)
        {
            if (reader == nullptr || reader->lengthInSamples <= 0)
                return {};
            // The file door: kMaxFileSeconds of audio, measured at the file's own rate.
            const auto numSamples = (int) juce::jmin (reader->lengthInSamples,
                                                      (juce::int64) fileCapacitySamples (reader->sampleRate));
            const auto numChannels = juce::jlimit (1, 2, (int) reader->numChannels);
            auto data = std::make_shared<SampleData>();
            data->name = name;
            data->id = id;
            data->isFactory = factory;
            data->sampleRate = reader->sampleRate;
            data->buffer.setSize (2, numSamples);
            data->buffer.clear();
            if (numChannels == 1)
            {
                reader->read (&data->buffer, 0, numSamples, 0, true, false);
                data->buffer.copyFrom (1, 0, data->buffer, 0, 0, numSamples);
            }
            else
            {
                reader->read (&data->buffer, 0, numSamples, 0, true, true);
            }
            data->buildPeaks();
            return data;
        }

        SamplePtr decodeBytes (const void* data, size_t size, const juce::String& name, bool factory, const juce::String& id)
        {
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            auto stream = std::make_unique<juce::MemoryInputStream> (data, size, false);
            auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (std::move (stream)));
            return decodeReader (std::move (reader), name, factory, id);
        }
    } // namespace

    SamplePtr loadFactorySource (const juce::String& id)
    {
        static const std::map<juce::String, std::pair<const char*, int>> assets {
            { "foley", { SliceForgeAssets::foley_wav, SliceForgeAssets::foley_wavSize } },
            { "kitchen", { SliceForgeAssets::kitchen_wav, SliceForgeAssets::kitchen_wavSize } },
            { "body", { SliceForgeAssets::body_wav, SliceForgeAssets::body_wavSize } },
            { "scrap", { SliceForgeAssets::scrap_wav, SliceForgeAssets::scrap_wavSize } },
            { "circuit", { SliceForgeAssets::circuit_wav, SliceForgeAssets::circuit_wavSize } },
            { "vinyl", { SliceForgeAssets::vinyl_wav, SliceForgeAssets::vinyl_wavSize } }
        };
        const auto it = assets.find (id);
        if (it == assets.end())
            return {};
        const auto* info = findFactorySource (id);
        return decodeBytes (it->second.first, (size_t) it->second.second,
                            info != nullptr ? info->name : id.toUpperCase(), true, id);
    }

    SamplePtr loadAudioFile (const juce::File& file)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file));
        return decodeReader (std::move (reader), file.getFileNameWithoutExtension().toUpperCase(), false,
                             "custom-" + juce::String (juce::Time::currentTimeMillis()));
    }

    SamplePtr decodeSampleFile (const void* data, size_t size, const juce::String& name)
    {
        return decodeBytes (data, size, name, false, "custom-" + juce::String (juce::Time::currentTimeMillis()));
    }

    SamplePtr decodePcm16 (const juce::MemoryBlock& pcm, int numChannels, double sampleRate, const juce::String& name)
    {
        if (pcm.getSize() < 2 || numChannels < 1)
            return {};
        const auto numSamples = (int) (pcm.getSize() / (2 * numChannels));
        if (numSamples <= 0)
            return {};
        auto data = std::make_shared<SampleData>();
        data->name = name;
        data->id = "custom-" + juce::String (juce::Time::currentTimeMillis());
        data->sampleRate = sampleRate;
        data->buffer.setSize (2, numSamples);
        const auto* bytes = static_cast<const int16_t*> (pcm.getData());
        for (int i = 0; i < numSamples; ++i)
        {
            const auto l = (float) bytes[(size_t) i * numChannels] / 32768.0f;
            const auto r = (float) bytes[(size_t) i * numChannels + juce::jmin (1, numChannels - 1)] / 32768.0f;
            data->buffer.setSample (0, i, l);
            data->buffer.setSample (1, i, r);
        }
        data->buildPeaks();
        return data;
    }

    juce::MemoryBlock encodePcm16 (const SampleData& sample)
    {
        const auto numSamples = sample.getNumSamples();
        juce::MemoryBlock block ((size_t) numSamples * 4);
        auto* out = static_cast<int16_t*> (block.getData());
        const auto* left = sample.buffer.getReadPointer (0);
        const auto* right = sample.buffer.getNumChannels() > 1 ? sample.buffer.getReadPointer (1) : left;
        for (int i = 0; i < numSamples; ++i)
        {
            out[(size_t) i * 2] = (int16_t) juce::jlimit (-32768, 32767, (int) std::round (left[i] * 32767.0f));
            out[(size_t) i * 2 + 1] = (int16_t) juce::jlimit (-32768, 32767, (int) std::round (right[i] * 32767.0f));
        }
        return block;
    }

    bool writeStereoWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        file.deleteFile();
        auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
        if (stream == nullptr)
            return false;
        juce::WavAudioFormat format;
        auto writer = std::unique_ptr<juce::AudioFormatWriter> (
            format.createWriterFor (stream.get(), sampleRate, 2, 24, {}, 0));
        if (writer == nullptr)
            return false;
        stream.release(); // writer owns it now
        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    }
} // namespace sliceforge
