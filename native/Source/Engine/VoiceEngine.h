#pragma once

#include "Dsp.h"
#include "Params.h"
#include "SampleData.h"
#include <array>
#include <atomic>
#include <memory>

namespace sliceforge
{
    class Voice;

    struct EngineRuntime
    {
        std::array<PadSettings, kNumPads> pads {};
        std::array<float, kNumPads> padStarts {};
        std::array<bool, kNumPads> audible {};
        GlobalSettings global;
    };

    class DrumEngine
    {
    public:
        DrumEngine();
        ~DrumEngine();

        void prepare (double sampleRate, int maximumBlockSize);
        void reset();

        void setSample (SamplePtr newSample);
        SamplePtr getSample() const;
        void setRuntime (const EngineRuntime& runtime);
        const EngineRuntime& getRuntime() const noexcept { return runtime; }

        void trigger (int pad, float velocity, const PadSettings* overrideSettings = nullptr);
        void chokeAll();
        void updateGating();

        void clearBusBuffers (int numSamples);
        void processSegment (int startSample, int numSamples);
        void processMaster (int numSamples);

        juce::AudioBuffer<float>& getBusBuffer (int busIndex) { return busBuffers[(size_t) busIndex]; }

        int getActiveVoiceCount() const;
        float getBusPeak (int busIndex) const;

        double getSampleRate() const noexcept { return currentSampleRate; }

    private:
        void startVoice (int padIndex, float velocity);
        Voice* findFreeVoice();

        double currentSampleRate = 48000.0;
        int maximumBlock = 512;
        std::array<std::unique_ptr<Voice>, kMaxVoices> voices;
        std::array<juce::AudioBuffer<float>, kNumBuses> busBuffers;
        std::array<MasterChain, kNumBuses> masterChains;
        EngineRuntime runtime;
        std::atomic<std::shared_ptr<const SampleData>> samplePtr;
        Rng rng { 0x1957A4C3u };
        std::array<std::atomic<float>, kNumBuses> busPeaks {};
        int activeVoices = 0;
    };
} // namespace sliceforge
