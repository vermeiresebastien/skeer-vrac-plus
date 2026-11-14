#pragma once

#include "SampleData.h"
#include <JuceHeader.h>
#include <atomic>
#include <memory>

namespace sliceforge
{
    /** Audition transport for the SKEER Sample Vault.

        Holds one decoded file and mixes it into a host-facing buffer, either once or on
        repeat. Everything the UI does — click a row, toggle LOOP, press STOP — is a plain
        atomic store, so the vault can be browsed while the audio thread never takes a lock.

        The class deliberately knows nothing about the vault or the UI: the panel owns the
        decode, the processor owns the player, and this is the hand-off in between.
    */
    class PreviewPlayer
    {
    public:
        PreviewPlayer() = default;

        /** Audio thread / message thread: the rate the render loop has to obey. */
        void prepare (double hostSampleRate);

        /** Start auditioning `newSample` from the top. A null sample is the same as stop(). */
        void play (SamplePtr newSample, bool shouldLoop);

        void stop();

        void setLoopEnabled (bool shouldLoop);
        bool isLoopEnabled() const noexcept { return loopEnabled.load (std::memory_order_relaxed); }

        bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }
        bool hasSample() const noexcept { return sample.load (std::memory_order_acquire) != nullptr; }

        /** 0..1 through the file, for the waveform playhead. */
        double getProgress() const;
        double getElapsedSeconds() const;
        double getDurationSeconds() const;

        /** Preview position in host samples, advanced by the audio thread. A watchdog for
            hosts that stop calling processBlock while the transport is idle. */
        uint64_t getAdvanceStamp() const noexcept { return advanceStamp.load (std::memory_order_relaxed); }

        void setLevel (float newLevel) { level.store (juce::jlimit (0.0f, 1.0f, newLevel), std::memory_order_relaxed); }

        /** Audio thread. Adds the audition to `destination` (stereo, in place) and applies a
            safety limiter so preview-plus-kit can never leave the plugin above full scale. */
        void render (juce::AudioBuffer<float>& destination, int numSamples);

    private:
        std::atomic<std::shared_ptr<const SampleData>> sample;
        std::atomic<double> position { 0.0 };                              // fractional source-sample index
        std::atomic<double> hostRate { 48000.0 };
        std::atomic<float> level { 0.9f };
        std::atomic<bool> playing { false };
        std::atomic<bool> loopEnabled { false };
        std::atomic<uint64_t> advanceStamp { 0 };
    };
} // namespace sliceforge
