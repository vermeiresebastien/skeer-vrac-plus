#include "Slicer.h"
#include "Params.h"
#include <algorithm>

namespace sliceforge
{
    std::vector<Slice> detectSlices (const SampleData& sample, float sensitivity)
    {
        constexpr int hop = 128;
        constexpr int window = 512;
        const int length = sample.getNumSamples();
        const double sampleRate = sample.sampleRate;
        if (length <= 0 || sampleRate <= 0.0)
            return {};
        const int hopCount = juce::jmax (1, length / hop);
        std::vector<float> env ((size_t) hopCount, 0.0f);
        const auto* left = sample.buffer.getReadPointer (0);
        const auto* right = sample.buffer.getNumChannels() > 1 ? sample.buffer.getReadPointer (1) : left;

        for (int h = 0; h < hopCount; ++h)
        {
            const int centre = h * hop;
            const int start = juce::jmax (0, centre - window / 2);
            const int end = juce::jmin (length, start + window);
            double sum = 0.0;
            for (int i = start; i < end; ++i)
            {
                const auto v = (left[i] + right[i]) * 0.5;
                sum += (double) v * v;
            }
            env[(size_t) h] = (float) std::sqrt (sum / (double) juce::jmax (1, end - start));
        }

        std::vector<float> flux ((size_t) hopCount, 0.0f);
        float maxFlux = 0.0f;
        for (int h = 1; h < hopCount; ++h)
        {
            const auto rise = juce::jmax (0.0f, env[(size_t) h] - env[(size_t) h - 1]);
            const auto a = h > 1 ? juce::jmax (0.0f, env[(size_t) h - 1] - env[(size_t) h - 2]) : 0.0f;
            const auto b = h + 1 < hopCount ? juce::jmax (0.0f, env[(size_t) h + 1] - env[(size_t) h]) : 0.0f;
            const auto v = juce::jmax (rise, a * 0.5f, b * 0.85f);
            flux[(size_t) h] = v;
            maxFlux = juce::jmax (maxFlux, v);
        }

        const auto sens = juce::jlimit (0.0f, 1.0f, sensitivity);
        const int target = (int) std::round (8.0f + 46.0f * sens);
        const double minGapSeconds = 0.115;
        const float floor = maxFlux * 0.012f;

        struct Candidate { int index; float strength; };
        std::vector<Candidate> candidates;
        for (int h = 2; h < hopCount - 2; ++h)
        {
            const auto v = flux[(size_t) h];
            if (v < floor)
                continue;
            if (v < flux[(size_t) h - 1] || v < flux[(size_t) h + 1])
                continue;
            if (v < flux[(size_t) h - 2] && v < flux[(size_t) h + 2])
                continue;
            candidates.push_back ({ h, v });
        }
        std::sort (candidates.begin(), candidates.end(),
                   [] (const Candidate& a, const Candidate& b) { return a.strength > b.strength; });

        std::vector<int> chosen;
        const double minGapHops = minGapSeconds * sampleRate / hop;
        for (const auto& candidate : candidates)
        {
            if ((int) chosen.size() >= juce::jmin (target, kMaxSlices))
                break;
            bool conflict = false;
            for (int other : chosen)
                if (std::abs (other - candidate.index) < minGapHops)
                {
                    conflict = true;
                    break;
                }
            if (! conflict)
                chosen.push_back (candidate.index);
        }
        std::sort (chosen.begin(), chosen.end());

        std::vector<Slice> slices;
        double last = -1.0;
        for (int h : chosen)
        {
            const auto time = juce::jmax (0.0, (h * hop) / sampleRate - 0.006);
            if (time - last < 0.07)
                continue;
            last = time;
            slices.push_back ({ time, juce::jlimit (0.0f, 1.0f, flux[(size_t) h] / (maxFlux > 0.0f ? maxFlux : 1.0f)) });
        }
        return slices;
    }

    std::vector<double> padStartsFromSlices (const std::vector<Slice>& slices)
    {
        std::vector<double> starts ((size_t) kNumPads, 0.0);
        if (slices.empty())
            return starts;
        for (int pad = 0; pad < kNumPads; ++pad)
        {
            double position = 0.0;
            if ((int) slices.size() >= kNumPads)
                position = pad * ((double) slices.size() - 1.0) / (kNumPads - 1.0);
            else
                position = (double) (pad % (int) slices.size());
            const auto index = juce::jlimit (0, (int) slices.size() - 1, (int) std::round (position));
            starts[(size_t) pad] = slices[(size_t) index].time;
        }
        return starts;
    }
} // namespace sliceforge
