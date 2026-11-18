#pragma once

#include <JuceHeader.h>

namespace sliceforge
{
    /*  How much audio an import may hold. Two doors fill the kit source - the recorded take
        (PluginProcessor::toggleRecording) and the decoded file (SampleData.cpp's decodeReader) -
        and they used to be four unrelated numbers: the take measured `currentSampleRate * 120.0`
        samples under a 48 MB ceiling, while the file cut off at `48000.0 * 60.0 * 10.0` samples
        whatever its rate was. So "ten minutes" meant 10.9 at 44.1 kHz, and "two minutes" changed
        with the host's sample rate without telling anyone. A duration is what the product promises
        and what the browser edition already measures (src/lib/importLimits.ts), so both ceilings
        are stated in seconds here and the byte figure keeps the one job it always had: a clamp. */

    /** Seconds a recorded take may hold - five minutes of the sidechain input. */
    constexpr double kMaxTakeSeconds = 300.0;

    /** Seconds a decoded file may hold - ten minutes, the same promise the browser edition makes
        before it even starts decoding. */
    constexpr double kMaxFileSeconds = 600.0;

    /*  Safety clamp on the take, in bytes; two channels of float32 is 8 bytes per sample-frame.
        Sized so the duration binds first at every rate a drum instrument runs at - 300 s at 96 kHz
        is 230 MB, inside this - and so that a host at 192 kHz gets a 175 s take rather than 460 MB
        of float reserved behind the REC button. Raising the seconds without raising this would
        silently shorten the take at 44.1 kHz, which is the whole trap this header exists to stop. */
    constexpr std::int64_t kMaxRecordBytes = 256 * 1024 * 1024;

    /** Samples a take may hold at this host rate: the promised seconds, clamped by the byte ceiling. */
    inline int takeCapacitySamples (double sampleRate) noexcept
    {
        const auto byBytes = kMaxRecordBytes / (2 * (std::int64_t) sizeof (float));
        const auto bySeconds = (std::int64_t) std::llround (juce::jmax (0.0, sampleRate) * kMaxTakeSeconds);
        return juce::jmax (1, (int) juce::jmin (byBytes, bySeconds));
    }

    /** Seconds that capacity is worth, so the button can state what the host's rate actually allows
        instead of promising five minutes at a sample rate where the clamp bites. A rate the host has
        not given yet is the promise unchanged: nothing can clamp it. */
    inline double takeCapacitySeconds (double sampleRate) noexcept
    {
        return sampleRate > 0.0 ? (double) takeCapacitySamples (sampleRate) / sampleRate : kMaxTakeSeconds;
    }

    /** Samples a decoded file may hold, measured at the file's own rate: the promise is a duration. */
    inline int fileCapacitySamples (double sampleRate) noexcept
    {
        return juce::jmax (1, (int) std::llround (juce::jmax (0.0, sampleRate) * kMaxFileSeconds));
    }
} // namespace sliceforge
