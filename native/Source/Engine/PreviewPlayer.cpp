#include "PreviewPlayer.h"

#include <cmath>

namespace sliceforge
{
    namespace
    {
        /** Safety limiter for the audition mix.

            Everything up to the knee passes through bit-exact, so a quiet preview over a
            quiet kit sounds like nothing was added at all; above it the sum is folded into
            the headroom that is left with a tanh knee, which asymptotes below full scale.
            Without this, a 0 dB loop clicked on top of a playing pattern leaves the plugin
            clipping in the host.
        */
        float softLimit (float x)
        {
            constexpr float knee = 0.9f;
            constexpr float ceiling = 0.999f;
            const auto magnitude = std::abs (x);
            if (magnitude <= knee)
                return x;
            const auto sign = x < 0.0f ? -1.0f : 1.0f;
            const auto folded = knee + (1.0f - knee) * std::tanh ((magnitude - knee) / (1.0f - knee));
            return sign * juce::jmin (folded, ceiling);
        }
    } // namespace

    void PreviewPlayer::prepare (double hostSampleRate)
    {
        hostRate.store (hostSampleRate > 0.0 ? hostSampleRate : 48000.0, std::memory_order_relaxed);
    }

    void PreviewPlayer::play (SamplePtr newSample, bool shouldLoop)
    {
        loopEnabled.store (shouldLoop, std::memory_order_relaxed);
        position.store (0.0, std::memory_order_relaxed);
        sample.store (newSample, std::memory_order_release);
        playing.store (newSample != nullptr && newSample->getNumSamples() > 1, std::memory_order_release);
    }

    void PreviewPlayer::stop()
    {
        playing.store (false, std::memory_order_release);
        position.store (0.0, std::memory_order_relaxed);
    }

    void PreviewPlayer::setLoopEnabled (bool shouldLoop)
    {
        loopEnabled.store (shouldLoop, std::memory_order_relaxed);
    }

    double PreviewPlayer::getProgress() const
    {
        const auto source = sample.load (std::memory_order_acquire);
        if (source == nullptr || source->getNumSamples() < 2)
            return 0.0;
        const auto length = (double) source->getNumSamples() - 1.0;
        return juce::jlimit (0.0, 1.0, position.load (std::memory_order_relaxed) / length);
    }

    double PreviewPlayer::getElapsedSeconds() const
    {
        const auto source = sample.load (std::memory_order_acquire);
        if (source == nullptr)
            return 0.0;
        return position.load (std::memory_order_relaxed) / (source->sampleRate > 0.0 ? source->sampleRate : 48000.0);
    }

    double PreviewPlayer::getDurationSeconds() const
    {
        const auto source = sample.load (std::memory_order_acquire);
        return source != nullptr ? source->getDurationSeconds() : 0.0;
    }

    void PreviewPlayer::render (juce::AudioBuffer<float>& destination, int numSamples)
    {
        if (! playing.load (std::memory_order_relaxed))
            return;

        const auto source = sample.load (std::memory_order_acquire);
        const auto length = source != nullptr ? source->getNumSamples() : 0;
        if (source == nullptr || length < 2 || destination.getNumChannels() == 0)
        {
            stop();
            return;
        }

        const auto last = (double) (length - 1);
        const auto sourceRate = source->sampleRate > 0.0 ? source->sampleRate : 48000.0;
        const auto step = (float) (sourceRate / hostRate.load (std::memory_order_relaxed));
        const auto gain = level.load (std::memory_order_relaxed);
        const auto channels = juce::jmin (2, destination.getNumChannels());
        const auto count = juce::jmin (numSamples, destination.getNumSamples());
        const auto* leftIn = source->buffer.getReadPointer (0);
        const auto* rightIn = source->getNumChannels() > 1 ? source->buffer.getReadPointer (1) : leftIn;
        auto* leftOut = destination.getWritePointer (0);
        auto* rightOut = channels > 1 ? destination.getWritePointer (1) : nullptr;

        auto pos = juce::jlimit (0.0, last, position.load (std::memory_order_relaxed));
        for (int i = 0; i < count; ++i)
        {
            const auto first = (int) pos;
            const auto next = juce::jmin (first + 1, length - 1);
            const auto frac = (float) (pos - (double) first);
            const auto inverse = 1.0f - frac;

            leftOut[i] = softLimit (leftOut[i] + (leftIn[first] * inverse + leftIn[next] * frac) * gain);
            if (rightOut != nullptr)
                rightOut[i] = softLimit (rightOut[i] + (rightIn[first] * inverse + rightIn[next] * frac) * gain);

            pos += (double) step;
            if (pos < last)
                continue;

            if (loopEnabled.load (std::memory_order_relaxed))
            {
                // Wrap on the loop length rather than the sample count so the last frame is
                // not read twice every cycle; a file shorter than one block still advances.
                while (pos >= last)
                    pos -= last;
                continue;
            }

            pos = last;
            position.store (pos, std::memory_order_relaxed);
            playing.store (false, std::memory_order_release);
            advanceStamp.fetch_add (1, std::memory_order_relaxed);
            return;
        }

        position.store (pos, std::memory_order_relaxed);
        advanceStamp.fetch_add (1, std::memory_order_relaxed);
    }
} // namespace sliceforge
