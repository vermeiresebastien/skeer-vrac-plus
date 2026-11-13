#pragma once

#include <JuceHeader.h>
#include "Params.h"

#include <array>
#include <limits>
#include <memory>

namespace vrac
{
    constexpr int kSeqPatterns = 4;
    constexpr int kSeqSteps = 32;
    constexpr int kSeqChainLength = 8;

    struct StepLocks
    {
        bool enabled = false;
        float pitch = 0.0f;       // semitones, absolute
        float volume = 0.0f;      // dB, absolute
        float pan = 0.0f;         // -1..1
        float tone = 0.0f;        // semitones
        float length = 0.25f;     // seconds, absolute
    };

    struct SeqStep
    {
        bool on = false;
        float velocity = 0.8f;
        float probability = 1.0f;
        int ratchet = 1;
        float nudge = 0.0f;       // fraction of a step, -0.5..0.5
        int flam = 0;             // extra grace hits before the main hit, 0..2
        StepLocks locks {};
    };

    struct SeqPattern
    {
        std::array<std::array<SeqStep, kSeqSteps>, kNumPads> rows {};
        std::array<bool, kNumPads> laneMute {};
        std::array<bool, kNumPads> laneSolo {};
        std::array<int, kNumPads> laneLength {}; // 0 = follow the global length, otherwise 1..32
    };

    struct SequencerData
    {
        std::array<SeqPattern, kSeqPatterns> patterns {};
        int activePattern = 0;
        int length = 16;         // 1..32 steps
        float swing = 0.0f;      // 0..0.66 of a step, applied to odd steps
        int division = 4;        // steps per quarter note
        double bpm = 128.0;      // used in internal mode and as fallback
        bool hostSync = true;
        bool songMode = false;
        std::array<int, kSeqChainLength> chainPatterns { 0, 1, 0, 2, 0, 1, 3, 2 };
        int chainLength = kSeqChainLength;
    };

    enum class PatternPreset
    {
        fourFloor,
        boomBap,
        halfTime,
        trap,
        afro,
        broken
    };

    juce::String patternPresetName (PatternPreset preset);
    SeqPattern makePatternPreset (PatternPreset preset);

    std::shared_ptr<const SequencerData> makeDefaultSequencerData();
    std::shared_ptr<SequencerData> cloneSequencerData (const SequencerData& source);
    juce::String divisionName (int stepsPerQuarter);

    void clearPattern (SeqPattern& pattern);
    void makeDefaultPattern (SeqPattern& pattern, int index);
    void randomizePattern (SeqPattern& pattern, int length,
                           const std::array<int, kNumPads>& families,
                           float density, juce::Random& random);
    void mutatePattern (SeqPattern& pattern, int length, float amount, juce::Random& random);
    void humanizePattern (SeqPattern& pattern, int length, juce::Random& random);
    void euclideanLane (SeqPattern& pattern, int pad, int length, int pulses,
                        int rotation, float velocity);
    void rotatePattern (SeqPattern& pattern, int length, int shift);
    void accentDownbeats (SeqPattern& pattern, int length);

    juce::MemoryBlock serialiseSequencer (const SequencerData& data);
    std::shared_ptr<SequencerData> deserialiseSequencer (const juce::MemoryBlock& data);
    juce::MemoryBlock serialisePattern (const SeqPattern& pattern);
    std::shared_ptr<SeqPattern> deserialisePattern (const juce::MemoryBlock& data);
    juce::MemoryBlock makePatternMidiFile (const SequencerData& data);

    struct SeqHostInfo
    {
        bool valid = false;
        bool playing = false;
        double ppq = 0.0;
        double bpm = 128.0;
    };

    struct SeqTrigger
    {
        int sampleOffset = 0;
        int pad = 0;
        float velocity = 0.8f;
        bool hasLocks = false;
        StepLocks locks {};
    };

    struct PadStepHistory
    {
        std::array<int64_t, 4> steps {};
        int cursor = 0;
    };

    struct SeqPlaybackState
    {
        bool hostWasPlaying = false;
        double expectedHostPpq = 0.0;
        std::array<PadStepHistory, kNumPads> lastHostPadStep {};

        bool internalWasPlaying = false;
        double internalPpq = 0.0;
        std::array<PadStepHistory, kNumPads> lastInternalPadStep {};
    };

    struct SeqBlockOutput
    {
        std::array<SeqTrigger, 256> triggers {};
        int triggerCount = 0;
        int playheadStep = -1;
        bool transportPlaying = false;
        bool hostPlaying = false;
        double hostBpm = 128.0;
    };

    void processSequencerBlock (const SequencerData& data,
                                const SeqHostInfo& host,
                                bool internalPlaying,
                                double sampleRate,
                                int numSamples,
                                juce::Random& random,
                                SeqPlaybackState& state,
                                SeqBlockOutput& output);
} // namespace vrac
