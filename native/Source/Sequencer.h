#pragma once

#include <JuceHeader.h>
#include "Params.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>

namespace sliceforge
{
    // kSeqPatterns and kSeqPatternFromDocument live in Params.h, beside the parameter layout and the
    // cache that clamps a host into the bank. This header gets them through its include at :4.
    constexpr int kSeqSteps = 32;
    constexpr int kSeqChainLength = 8;

    /*  The pass number of the first render anybody hears.

        A fresh SeqPlaybackState starts at one less than this, because both clocks advance the counter
        on the block that restarts them - and they restart *before* they schedule anything. The first
        audible pass of a session is therefore 2, not 1. It is a constant rather than a literal in two
        places because the offline exporter has to claim the same pass the plugin plays, which is what
        makePatternMidiFile's exportPass is for. */
    constexpr std::int64_t kFirstRenderPass = 2;

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
    /** Duplicates pattern `source` over pattern `target`, steps, lanes, mutes, solos and lane
        loop lengths included, which is what a hardware duplicate does. Returns false without
        touching anything when an index is out of range or both indices are the same. The global
        length, swing, division and song chain are not copied: the chain stores pattern numbers,
        so the duplicate is already audible in any slot that pointed at the target. */
    bool copyPatternTo (SequencerData& data, int source, int target);
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
    /** Renders the pattern as a standard MIDI file.

        `exportPass` is the pass whose probability draws the file is a snapshot of, and it defaults to
        kFirstRenderPass - the first pass anybody hears - because a printed clip cannot play the
        re-roll: one file is one set of draws, and the draw belongs to the pass counter as much as it
        belongs to the step. The pass is stated inside the file (text meta event 3) so the claim
        survives a rename. Everything else - swing, mute and solo, probability, flam, ratchet, nudge,
        tempo and the note map - is decided by the same code the render path decides with. */
    juce::MemoryBlock makePatternMidiFile (const SequencerData& data,
                                           std::int64_t exportPass = kFirstRenderPass);

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
        // How many times the host path threw its step history away. A clear is audible (a step
        // that straddles a block boundary loses its duplicate guard and can fire twice), so the
        // tolerance has to be measurable without changing what the scheduler does. One counter,
        // because the two causes - a transport engage and a tolerance trip - have the same effect,
        // and every check asks "how many times did the history go away", not "why".
        int historyClears = 0;

        bool internalWasPlaying = false;
        double internalPpq = 0.0;
        std::array<PadStepHistory, kNumPads> lastInternalPadStep {};

        // How many times either clock has restarted. "Pass" here means a restart, not a bar:
        // a real loop is one, and so is a tolerance trip while the 0.02 floor is narrower than
        // the block. Either way it clears its step history and gets its own probability draws, so
        // a host that loops a bar re-rolls instead of replaying one frozen dropout set all
        // session. Advanced on the same reset as historyClears, above. See probabilityRollFor.
        // Starts one below the first audible pass because the reset that engages the transport
        // increments it before it schedules anything: see kFirstRenderPass.
        int64_t passCount = kFirstRenderPass - 1;
    };

    struct SeqBlockOutput
    {
        std::array<SeqTrigger, 256> triggers {};
        int triggerCount = 0;
        int droppedTriggers = 0;   // hits the block could not fit; audible as a missing note
        int playheadStep = -1;
        bool transportPlaying = false;
        bool hostPlaying = false;
        double hostBpm = 128.0;
    };

    /** Schedules one block. `liveActivePattern` is the `seq.pattern` parameter value, already
        clamped, or kSeqPatternFromDocument to let the document's own copy decide - which is what
        every caller without a running processor (the tests here, the exporter) passes. */
    void processSequencerBlock (const SequencerData& data,
                                const SeqHostInfo& host,
                                bool internalPlaying,
                                double sampleRate,
                                int numSamples,
                                juce::Random& random,
                                SeqPlaybackState& state,
                                SeqBlockOutput& output,
                                int liveActivePattern = kSeqPatternFromDocument);
} // namespace sliceforge
