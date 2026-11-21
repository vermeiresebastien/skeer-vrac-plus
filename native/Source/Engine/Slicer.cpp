#include "Slicer.h"
#include "SlicerTuning.h"
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
        const int target = (int) std::round (kTargetBase + kTargetSpan * sens);
        const float floor = maxFlux * kFloorRatio;

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
        const double minGapHops = kMinGapSeconds * sampleRate / hop;
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
            const auto time = juce::jmax (0.0, (h * hop) / sampleRate - kOnsetBiasSeconds);
            if (time - last < kDedupGateSeconds)
                continue;
            last = time;
            slices.push_back ({ time, juce::jlimit (0.0f, 1.0f, flux[(size_t) h] / (maxFlux > 0.0f ? maxFlux : 1.0f)) });
        }
        return slices;
    }

    double sliceGapSeconds (const std::vector<Slice>& slices, int index, double durationSeconds)
    {
        if (index < 0 || index >= (int) slices.size())
            return 0.0;
        const auto start = slices[(size_t) index].time;
        // A slice can sit past the end of the material (the pad editor lets you place one), so the
        // end is taken as never before the start rather than trusted to be after it.
        const auto end = juce::jmax (start, index + 1 < (int) slices.size()
                                            ? slices[(size_t) (index + 1)].time
                                            : durationSeconds);
        return end - start;
    }

    SliceMapping sliceMapFromSlices (const std::vector<Slice>& slices, double durationSeconds)
    {
        SliceMapping mapping;
        mapping.slices = slices;
        const int n = (int) slices.size();
        if (n == 0)
            return mapping; // No slices is no kit: the caller keeps whatever it already had.

        for (int pad = 0; pad < kNumPads; ++pad)
        {
            double position = 0.0;
            if (n >= kNumPads)
            {
                position = (double) pad * (double) (n - 1) / (double) (kNumPads - 1);
            }
            else if (pad < n)
            {
                // Fewer slices than pads. Every slice gets a pad of its own before any slice gets
                // a second one: wrapping at `pad % n` instead means a four-onset take puts all
                // sixteen pads on the first quarter of the file and none on the rest of it.
                position = (double) pad;
            }
            else
            {
                const auto stride = (double) (n - 1) / (double) (kNumPads - n);
                position = (double) (pad - n) * stride;
            }
            const auto index = juce::jlimit (0, n - 1, (int) std::round (position));
            mapping.padSliceIndices[(size_t) pad] = index;
            mapping.padStarts[(size_t) pad] = slices[(size_t) index].time;
            mapping.padLengths[(size_t) pad] = sliceGapSeconds (slices, index, durationSeconds);
        }
        return mapping;
    }

    std::vector<double> padStartsFromSlices (const std::vector<Slice>& slices)
    {
        // The starts-only view. The duration argument cannot make a start move, and no length is
        // read out of this result, so passing the last slice's own time is inert by construction.
        const auto duration = slices.empty () ? 0.0 : slices.back().time;
        const auto mapping = sliceMapFromSlices (slices, duration);
        return std::vector<double> (mapping.padStarts.begin(), mapping.padStarts.end());
    }
} // namespace sliceforge
