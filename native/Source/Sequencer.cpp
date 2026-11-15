#include "Sequencer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace sliceforge
{
    namespace
    {
        constexpr int kSequenceFormatVersion = 2;
        constexpr int32_t kSequenceMagic = 0x56524143; // 'VRAC'
        constexpr int kMidiTicksPerQuarter = 960;
        constexpr double kFlamStep = 0.12;
        constexpr double kFlamVelocity = 0.62;

        int sanitiseDivision (int division)
        {
            return division == 2 || division == 3 || division == 4
                       || division == 6 || division == 8
                   ? division : 4;
        }

        int wrapStep (int64_t step, int length)
        {
            length = juce::jlimit (1, kSeqSteps, length);
            const auto wrapped = step % length;
            return (int) (wrapped < 0 ? wrapped + length : wrapped);
        }

        int patternIndexForStep (const SequencerData& data, int64_t absoluteStep)
        {
            if (! data.songMode)
                return juce::jlimit (0, kSeqPatterns - 1, data.activePattern);

            const int length = juce::jlimit (1, kSeqSteps, data.length);
            const int chainLength = juce::jlimit (1, kSeqChainLength, data.chainLength);
            auto slot = (absoluteStep / length) % chainLength;
            if (slot < 0)
                slot += chainLength;
            return juce::jlimit (0, kSeqPatterns - 1,
                                 data.chainPatterns[(size_t) ((int) slot % chainLength)]);
        }

        int laneLengthForPad (const SeqPattern& pattern, int pad, int globalLength)
        {
            const auto lane = pattern.laneLength[(size_t) pad];
            return lane > 0 ? juce::jlimit (1, kSeqSteps, lane)
                            : juce::jlimit (1, kSeqSteps, globalLength);
        }

        bool laneIsAudible (const SeqPattern& pattern, int pad)
        {
            if (pattern.laneMute[(size_t) pad])
                return false;
            const bool anySolo = std::any_of (pattern.laneSolo.begin(), pattern.laneSolo.end(),
                                              [] (bool solo) { return solo; });
            return ! anySolo || pattern.laneSolo[(size_t) pad];
        }

        bool wasRecentlyTriggered (const PadStepHistory& history, int64_t step)
        {
            return std::find (history.steps.begin(), history.steps.end(), step)
                   != history.steps.end();
        }

        void rememberTriggered (PadStepHistory& history, int64_t step)
        {
            history.steps[(size_t) (history.cursor % (int) history.steps.size())] = step;
            ++history.cursor;
        }

        void clearHistory (std::array<PadStepHistory, kNumPads>& history)
        {
            for (auto& lane : history)
            {
                lane.steps.fill (std::numeric_limits<int64_t>::min());
                lane.cursor = 0;
            }
        }

        void setStep (SeqPattern& pattern, int pad, int step, float velocity,
                      float probability = 1.0f, int ratchet = 1)
        {
            if (pad < 0 || pad >= kNumPads || step < 0 || step >= kSeqSteps)
                return;
            auto& s = pattern.rows[(size_t) pad][(size_t) step];
            s.on = true;
            s.velocity = juce::jlimit (0.0f, 1.0f, velocity);
            s.probability = juce::jlimit (0.0f, 1.0f, probability);
            s.ratchet = juce::jlimit (1, 4, ratchet);
        }

        void addTrigger (SeqBlockOutput& output, int sampleOffset, int pad,
                         float velocity, const SeqStep& step)
        {
            if (output.triggerCount >= (int) output.triggers.size())
                return;
            auto& trigger = output.triggers[(size_t) output.triggerCount++];
            trigger.sampleOffset = sampleOffset;
            trigger.pad = pad;
            trigger.velocity = velocity;
            trigger.hasLocks = step.locks.enabled;
            trigger.locks = step.locks;
        }

        /** The probability draw for one step, decided by *position* rather than by the
            shared RNG.

            A step's events are not contiguous: the grace hits sit before the main hit and
            the ratchet hits after it, so with a 512-sample block a single step is evaluated
            by two or three different blocks. A coin flip made at each of those evaluations
            would let a rolled-out hit still leave its flam behind, and would let the main
            hit play while its ratchets did not. The draw therefore has to be a pure function
            of where in the timeline the step is.

            The absolute step keeps counting while the pattern loops, so every pass through
            the bar still gets an independent draw; splitmix64 keeps that distribution flat.
        */
        double probabilityRollFor (int patternIndex, int pad, int64_t absoluteStep) noexcept
        {
            std::uint64_t h = 0x9E3779B97F4A7C15ull
                                  + (std::uint64_t) absoluteStep * 0xFF51AFD7ED558CCDull
                                  + (std::uint64_t) (pad + 1) * 0xC2B2AE3D27D4EB4Full
                                  + (std::uint64_t) (patternIndex + 1) * 0x165667B19E3779F9ull;
            h ^= h >> 30;
            h *= 0xBF58476D1CE4E5B9ull;
            h ^= h >> 27;
            h *= 0x94D049BB133111EBull;
            h ^= h >> 31;
            return (double) (h >> 11) * (1.0 / 9007199254740992.0); // [0, 1)
        }

        void scheduleAbsoluteStep (const SequencerData& data,
                                   int64_t absoluteStep,
                                   double blockStartPpq,
                                   double blockEndPpq,
                                   double bpm,
                                   double sampleRate,
                                   int numSamples,
                                   juce::Random& random,
                                   bool hostMode,
                                   SeqPlaybackState& state,
                                   SeqBlockOutput& output)
        {
            juce::ignoreUnused (random); // see probabilityRollFor: the draw is positional now
            const int globalLength = juce::jlimit (1, kSeqSteps, data.length);
            const int division = sanitiseDivision (data.division);
            const auto patternIndex = patternIndexForStep (data, absoluteStep);
            const auto& pattern = data.patterns[(size_t) patternIndex];
            auto& histories = hostMode ? state.lastHostPadStep : state.lastInternalPadStep;
            const double basePpq = (double) absoluteStep / (double) division;
            const double swing = (absoluteStep & 1) != 0
                                     ? juce::jlimit (0.0, 0.66, (double) data.swing) / division
                                     : 0.0;
            const double samplesPerPpq = 60.0 * sampleRate / juce::jmax (20.0, bpm);
            const double stepPpq = 1.0 / (double) division; // one step, in quarter notes

            for (int pad = 0; pad < kNumPads; ++pad)
            {
                auto& history = histories[(size_t) pad];
                const int laneLength = laneLengthForPad (pattern, pad, globalLength);
                const auto& step = pattern.rows[(size_t) pad][(size_t) wrapStep (absoluteStep, laneLength)];
                const double eventPpq = basePpq + swing + (double) step.nudge / division;

                const auto baseOffset = (int) std::llround ((eventPpq - blockStartPpq)
                                                            * samplesPerPpq);
                const bool baseDue = eventPpq >= blockStartPpq - 1.0e-9
                                         && eventPpq < blockEndPpq - 1.0e-9
                                         && baseOffset >= 0 && baseOffset < numSamples;

                // The main hit is the one event that the neighbouring blocks also consider,
                // because their look-ahead and look-back windows overlap here, so it keeps
                // the per-step history guard. The sub-hits do not need one: each has its own
                // place on the timeline, and a place falls inside exactly one block.
                // A step is consumed as soon as its main hit is due, even if it turns out to
                // be muted or rolled out below, exactly as before — otherwise un-muting a lane
                // would let the step fire late instead of not at all.
                const bool freshBase = baseDue && ! wasRecentlyTriggered (history, absoluteStep);
                if (baseDue)
                    rememberTriggered (history, absoluteStep);

                // Whether this step sounds at all is a property of the step, not of the
                // block that happens to be rendering it, so every block that looks at this
                // position has to come to the same answer.
                if (! laneIsAudible (pattern, pad) || ! step.on || step.probability <= 0.0f)
                    continue;
                if (step.probability < 0.999f
                    && probabilityRollFor (patternIndex, pad, absoluteStep) > step.probability)
                    continue;

                // Grace hits come *before* the main hit, which usually means they belong to
                // the block before the one carrying it. Their own position decides.
                const int flams = juce::jlimit (0, 2, step.flam);
                for (int f = 1; f <= flams; ++f)
                {
                    const auto offset = (int) std::llround ((eventPpq
                                                             - (double) f * kFlamStep * stepPpq
                                                             - blockStartPpq) * samplesPerPpq);
                    if (offset >= 0 && offset < numSamples)
                        addTrigger (output, offset, pad,
                                    juce::jlimit (0.02f, 1.0f, step.velocity * (float) kFlamVelocity),
                                    step);
                }

                if (freshBase)
                    addTrigger (output, baseOffset, pad,
                                juce::jlimit (0.02f, 1.0f, step.velocity), step);

                const int ratchets = juce::jlimit (1, 4, step.ratchet);
                for (int r = 1; r < ratchets; ++r)
                {
                    const auto offset = (int) std::llround ((eventPpq
                                                             + (double) r * stepPpq / (double) ratchets
                                                             - blockStartPpq) * samplesPerPpq);
                    if (offset >= 0 && offset < numSamples)
                        addTrigger (output, offset, pad,
                                    juce::jlimit (0.02f, 1.0f, step.velocity * 0.72f), step);
                }
            }
        }

        void scheduleHostBlock (const SequencerData& data,
                                const SeqHostInfo& host,
                                double sampleRate,
                                int numSamples,
                                juce::Random& random,
                                SeqPlaybackState& state,
                                SeqBlockOutput& output)
        {
            const int division = sanitiseDivision (data.division);
            const double bpm = host.bpm > 0.0 ? host.bpm : juce::jmax (20.0, data.bpm);
            const double ppqStart = host.ppq;
            const double ppqEnd = ppqStart + (double) numSamples * (bpm / 60.0)
                                              / juce::jmax (1.0, sampleRate);

            bool reset = ! state.hostWasPlaying;
            if (! reset && std::abs (ppqStart - state.expectedHostPpq) > 0.02)
                reset = true;
            if (reset)
                clearHistory (state.lastHostPadStep);

            if (ppqEnd < ppqStart)
            {
                state.hostWasPlaying = true;
                state.expectedHostPpq = ppqEnd;
                output.hostPlaying = true;
                output.transportPlaying = true;
                return;
            }

            const auto first = (int64_t) std::floor (ppqStart * division) - 2;
            const auto last = (int64_t) std::ceil (ppqEnd * division) + 2;
            for (auto absolute = first; absolute <= last; ++absolute)
                scheduleAbsoluteStep (data, absolute, ppqStart, ppqEnd, bpm, sampleRate,
                                      numSamples, random, true, state, output);

            output.hostBpm = bpm;
            output.hostPlaying = true;
            output.transportPlaying = true;
            output.playheadStep = wrapStep ((int64_t) std::floor (ppqStart * division + 1.0e-6),
                                            data.length);
            state.expectedHostPpq = ppqEnd;
            state.hostWasPlaying = true;
        }

        void scheduleInternalBlock (const SequencerData& data,
                                    double sampleRate,
                                    int numSamples,
                                    juce::Random& random,
                                    SeqPlaybackState& state,
                                    SeqBlockOutput& output)
        {
            if (! state.internalWasPlaying)
            {
                state.internalPpq = 0.0;
                clearHistory (state.lastInternalPadStep);
            }

            const int division = sanitiseDivision (data.division);
            const double bpm = juce::jmax (20.0, data.bpm);
            const double ppqStart = state.internalPpq;
            const double ppqEnd = ppqStart + (double) numSamples * (bpm / 60.0)
                                              / juce::jmax (1.0, sampleRate);

            const auto first = (int64_t) std::floor (ppqStart * division) - 2;
            const auto last = (int64_t) std::ceil (ppqEnd * division) + 2;
            for (auto absolute = first; absolute <= last; ++absolute)
                scheduleAbsoluteStep (data, absolute, ppqStart, ppqEnd, bpm, sampleRate,
                                      numSamples, random, false, state, output);

            output.hostPlaying = false;
            output.transportPlaying = true;
            output.hostBpm = bpm;
            output.playheadStep = wrapStep ((int64_t) std::floor (ppqEnd * division + 1.0e-6),
                                            data.length);
            state.internalPpq = ppqEnd;
            state.internalWasPlaying = true;
        }

        void writePattern (juce::MemoryOutputStream& stream, const SeqPattern& pattern)
        {
            for (int pad = 0; pad < kNumPads; ++pad)
            {
                stream.writeBool (pattern.laneMute[(size_t) pad]);
                stream.writeBool (pattern.laneSolo[(size_t) pad]);
                stream.writeInt (juce::jlimit (0, kSeqSteps, pattern.laneLength[(size_t) pad]));
                for (int step = 0; step < kSeqSteps; ++step)
                {
                    const auto& s = pattern.rows[(size_t) pad][(size_t) step];
                    stream.writeBool (s.on);
                    stream.writeFloat (juce::jlimit (0.0f, 1.0f, s.velocity));
                    stream.writeFloat (juce::jlimit (0.0f, 1.0f, s.probability));
                    stream.writeByte ((juce::uint8) juce::jlimit (1, 4, s.ratchet));
                    stream.writeFloat (juce::jlimit (-0.5f, 0.5f, s.nudge));
                    stream.writeByte ((juce::uint8) juce::jlimit (0, 2, s.flam));
                    stream.writeBool (s.locks.enabled);
                    stream.writeFloat (juce::jlimit (-24.0f, 24.0f, s.locks.pitch));
                    stream.writeFloat (juce::jlimit (-60.0f, 6.0f, s.locks.volume));
                    stream.writeFloat (juce::jlimit (-1.0f, 1.0f, s.locks.pan));
                    stream.writeFloat (juce::jlimit (-12.0f, 12.0f, s.locks.tone));
                    stream.writeFloat (juce::jlimit (0.02f, 2.0f, s.locks.length));
                }
            }
        }

        bool readPattern (juce::MemoryInputStream& stream, int version, SeqPattern& pattern)
        {
            for (int pad = 0; pad < kNumPads; ++pad)
            {
                pattern.laneMute[(size_t) pad] = stream.readBool();
                pattern.laneSolo[(size_t) pad] = stream.readBool();
                if (version >= 2)
                    pattern.laneLength[(size_t) pad] = juce::jlimit (0, kSeqSteps, stream.readInt());
                for (int step = 0; step < kSeqSteps; ++step)
                {
                    auto& s = pattern.rows[(size_t) pad][(size_t) step];
                    s.on = stream.readBool();
                    s.velocity = juce::jlimit (0.0f, 1.0f, stream.readFloat());
                    s.probability = juce::jlimit (0.0f, 1.0f, stream.readFloat());
                    s.ratchet = juce::jlimit (1, 4, (int) stream.readByte());
                    if (version >= 2)
                    {
                        s.nudge = juce::jlimit (-0.5f, 0.5f, stream.readFloat());
                        s.flam = juce::jlimit (0, 2, (int) stream.readByte());
                        s.locks.enabled = stream.readBool();
                        s.locks.pitch = juce::jlimit (-24.0f, 24.0f, stream.readFloat());
                        s.locks.volume = juce::jlimit (-60.0f, 6.0f, stream.readFloat());
                        s.locks.pan = juce::jlimit (-1.0f, 1.0f, stream.readFloat());
                        s.locks.tone = juce::jlimit (-12.0f, 12.0f, stream.readFloat());
                        s.locks.length = juce::jlimit (0.02f, 2.0f, stream.readFloat());
                    }
                }
            }
            return stream.getNumBytesRemaining() >= 0;
        }
    } // namespace

    std::shared_ptr<const SequencerData> makeDefaultSequencerData()
    {
        auto data = std::make_shared<SequencerData>();
        data->activePattern = 0;
        data->length = 16;
        data->swing = 0.0f;
        data->division = 4;
        data->bpm = 128.0;
        data->hostSync = true;
        data->songMode = false;
        data->chainLength = kSeqChainLength;
        data->chainPatterns = { 0, 1, 0, 2, 0, 1, 3, 2 };
        makeDefaultPattern (data->patterns[0], 0);
        data->patterns[1] = data->patterns[0];
        setStep (data->patterns[1], 7, 3, 0.35f);
        setStep (data->patterns[1], 7, 11, 0.4f);
        setStep (data->patterns[1], 10, 2, 0.5f);
        setStep (data->patterns[1], 10, 10, 0.55f);
        setStep (data->patterns[1], 13, 6, 0.35f);
        return data;
    }

    std::shared_ptr<SequencerData> cloneSequencerData (const SequencerData& source)
    {
        return std::make_shared<SequencerData> (source);
    }

    juce::String divisionName (int stepsPerQuarter)
    {
        switch (stepsPerQuarter)
        {
            case 2:  return "1/8";
            case 3:  return "1/8T";
            case 4:  return "1/16";
            case 6:  return "1/16T";
            case 8:  return "1/32";
            default: return "1/16";
        }
    }

    juce::String patternPresetName (PatternPreset preset)
    {
        switch (preset)
        {
            case PatternPreset::fourFloor: return "FOUR FLOOR";
            case PatternPreset::boomBap:   return "BOOM BAP";
            case PatternPreset::halfTime:  return "HALF TIME";
            case PatternPreset::trap:      return "TRAP";
            case PatternPreset::afro:      return "AFRO 12";
            case PatternPreset::broken:    return "BROKEN";
        }
        return "PATTERN";
    }

    SeqPattern makePatternPreset (PatternPreset preset)
    {
        SeqPattern pattern;
        switch (preset)
        {
            case PatternPreset::fourFloor:
                setStep (pattern, 0, 0, 0.98f);
                setStep (pattern, 0, 4, 0.82f);
                setStep (pattern, 0, 8, 0.98f);
                setStep (pattern, 0, 12, 0.86f);
                setStep (pattern, 2, 4, 0.86f);
                setStep (pattern, 2, 12, 0.9f);
                setStep (pattern, 3, 4, 0.55f);
                setStep (pattern, 3, 12, 0.62f);
                for (int step = 0; step < 16; step += 2)
                    setStep (pattern, 6, step, step % 4 == 0 ? 0.7f : 0.45f);
                for (int step = 1; step < 16; step += 2)
                    setStep (pattern, 7, step, 0.32f);
                break;

            case PatternPreset::boomBap:
                setStep (pattern, 0, 0, 0.98f);
                setStep (pattern, 0, 7, 0.7f);
                setStep (pattern, 0, 10, 0.86f);
                setStep (pattern, 1, 3, 0.5f);
                setStep (pattern, 2, 4, 0.88f);
                setStep (pattern, 2, 12, 0.95f);
                setStep (pattern, 3, 4, 0.55f);
                setStep (pattern, 3, 12, 0.62f);
                for (int step = 0; step < 16; step += 2)
                    setStep (pattern, 6, step, step % 4 == 0 ? 0.72f : 0.5f);
                for (int step = 1; step < 16; step += 2)
                    setStep (pattern, 7, step, 0.28f);
                pattern.rows[2][4].nudge = -0.08f;
                pattern.rows[2][12].nudge = 0.06f;
                break;

            case PatternPreset::halfTime:
                setStep (pattern, 0, 0, 1.0f);
                setStep (pattern, 0, 10, 0.8f);
                setStep (pattern, 2, 8, 0.92f);
                setStep (pattern, 3, 8, 0.66f);
                for (int step = 0; step < 16; step += 4)
                    setStep (pattern, 6, step, 0.7f);
                for (int step = 2; step < 16; step += 4)
                    setStep (pattern, 7, step, 0.35f);
                break;

            case PatternPreset::trap:
                setStep (pattern, 0, 0, 1.0f);
                setStep (pattern, 0, 3, 0.62f);
                setStep (pattern, 0, 6, 0.82f);
                setStep (pattern, 0, 10, 0.76f);
                setStep (pattern, 2, 8, 0.9f);
                setStep (pattern, 3, 4, 0.5f);
                setStep (pattern, 3, 12, 0.58f);
                for (int step = 0; step < 16; ++step)
                    setStep (pattern, 6, step, step % 4 == 0 ? 0.62f : 0.3f, 0.72f);
                pattern.rows[6][7].ratchet = 3;
                pattern.rows[6][15].ratchet = 2;
                pattern.rows[0][3].flam = 1;
                break;

            case PatternPreset::afro:
                setStep (pattern, 0, 0, 0.95f);
                setStep (pattern, 0, 3, 0.65f);
                setStep (pattern, 0, 6, 0.8f);
                setStep (pattern, 0, 10, 0.72f);
                setStep (pattern, 2, 4, 0.72f);
                setStep (pattern, 2, 12, 0.82f);
                for (int step = 0; step < 12; step += 3)
                {
                    setStep (pattern, 6, step, 0.55f);
                    setStep (pattern, 7, step + 1, 0.36f);
                }
                pattern.laneLength[6] = 12;
                pattern.laneLength[7] = 12;
                pattern.laneLength[10] = 6;
                setStep (pattern, 10, 2, 0.62f);
                setStep (pattern, 10, 5, 0.45f);
                break;

            case PatternPreset::broken:
                setStep (pattern, 0, 0, 0.9f);
                setStep (pattern, 0, 5, 0.6f);
                setStep (pattern, 0, 9, 0.75f);
                setStep (pattern, 2, 3, 0.62f);
                setStep (pattern, 2, 11, 0.95f);
                setStep (pattern, 3, 7, 0.55f);
                setStep (pattern, 6, 2, 0.45f);
                setStep (pattern, 6, 6, 0.55f);
                setStep (pattern, 6, 10, 0.4f);
                setStep (pattern, 7, 13, 0.4f);
                pattern.rows[0][5].nudge = 0.22f;
                pattern.rows[2][11].nudge = -0.18f;
                pattern.rows[6][10].flam = 1;
                break;
        }
        return pattern;
    }

    void clearPattern (SeqPattern& pattern)
    {
        pattern = SeqPattern {};
    }

    bool copyPatternTo (SequencerData& data, int source, int target)
    {
        if (source < 0 || source >= kSeqPatterns
            || target < 0 || target >= kSeqPatterns
            || source == target)
            return false;

        data.patterns[(size_t) target] = data.patterns[(size_t) source];
        return true;
    }

    void makeDefaultPattern (SeqPattern& pattern, int index)
    {
        clearPattern (pattern);
        if (index != 0)
            return;

        setStep (pattern, 0, 0, 0.95f);
        setStep (pattern, 0, 4, 0.82f);
        setStep (pattern, 0, 8, 0.96f);
        setStep (pattern, 0, 12, 0.86f);
        setStep (pattern, 0, 10, 0.42f);
        setStep (pattern, 1, 14, 0.55f);
        setStep (pattern, 2, 4, 0.82f);
        setStep (pattern, 2, 12, 0.9f);
        setStep (pattern, 2, 15, 0.28f);
        setStep (pattern, 3, 4, 0.6f);
        setStep (pattern, 3, 12, 0.7f);
        setStep (pattern, 14, 7, 0.25f);
        for (int step = 0; step < 16; step += 2)
            setStep (pattern, 6, step, step % 4 == 0 ? 0.72f : 0.5f);
        for (int step = 1; step < 16; step += 2)
            setStep (pattern, 7, step, 0.28f);
        setStep (pattern, 10, 6, 0.5f);
    }

    void randomizePattern (SeqPattern& pattern, int length,
                           const std::array<int, kNumPads>& families,
                           float density, juce::Random& random)
    {
        clearPattern (pattern);
        length = juce::jlimit (1, kSeqSteps, length);
        const auto densityScale = juce::jlimit (0.35f, 1.45f, density / 0.5f);

        for (int pad = 0; pad < kNumPads; ++pad)
        {
            const auto family = juce::jlimit (0, 3, families[(size_t) pad]);
            float probability = family == 0 ? 0.42f : family == 1 ? 0.24f : family == 2 ? 0.5f : 0.2f;

            for (int step = 0; step < length; ++step)
            {
                bool force = family == 0 && (step == 0 || step == 8);
                if (family == 0 && length >= 16 && step == 4)
                    force = random.nextFloat() < 0.55f;
                if (family == 1 && length >= 16 && (step == 4 || step == 12))
                    force = random.nextFloat() < 0.72f;
                if (family == 2 && step % 2 == 0)
                    force = random.nextFloat() < 0.62f;
                if (family == 3 && length >= 16 && (step == 4 || step == 12))
                    force = random.nextFloat() < 0.45f;
                if (! force && random.nextFloat() > probability * densityScale)
                    continue;

                const bool downbeat = step % 4 == 0;
                setStep (pattern, pad, step,
                         juce::jlimit (0.18f, 1.0f, (downbeat ? 0.8f : 0.52f) + random.nextFloat() * 0.22f),
                         random.nextFloat() < 0.9f ? 1.0f : 0.65f,
                         random.nextFloat() < 0.06f ? 2 : 1);
                auto& target = pattern.rows[(size_t) pad][(size_t) step];
                target.nudge = random.nextFloat() < 0.12f ? random.nextFloat() * 0.32f - 0.16f : 0.0f;
                target.flam = random.nextFloat() < 0.07f ? 1 : 0;
            }
        }
    }

    void mutatePattern (SeqPattern& pattern, int length, float amount, juce::Random& random)
    {
        length = juce::jlimit (1, kSeqSteps, length);
        amount = juce::jlimit (0.0f, 1.0f, amount);
        for (int pad = 0; pad < kNumPads; ++pad)
            for (int step = 0; step < length; ++step)
            {
                auto& s = pattern.rows[(size_t) pad][(size_t) step];
                if (random.nextFloat() < amount)
                    s.on = ! s.on;
                if (s.on && random.nextFloat() < amount * 1.8f)
                {
                    s.velocity = juce::jlimit (0.05f, 1.0f, s.velocity + random.nextFloat() * 0.28f - 0.14f);
                    s.probability = juce::jlimit (0.2f, 1.0f, s.probability + random.nextFloat() * 0.3f - 0.15f);
                    s.nudge = juce::jlimit (-0.5f, 0.5f, s.nudge + random.nextFloat() * 0.12f - 0.06f);
                    if (random.nextFloat() < 0.08f)
                        s.flam = random.nextBool() ? 1 : 0;
                }
            }
    }

    void humanizePattern (SeqPattern& pattern, int length, juce::Random& random)
    {
        length = juce::jlimit (1, kSeqSteps, length);
        for (int pad = 0; pad < kNumPads; ++pad)
            for (int step = 0; step < length; ++step)
            {
                auto& s = pattern.rows[(size_t) pad][(size_t) step];
                if (! s.on)
                    continue;
                s.velocity = juce::jlimit (0.05f, 1.0f, s.velocity + random.nextFloat() * 0.16f - 0.08f);
                s.probability = juce::jlimit (0.25f, 1.0f, s.probability + random.nextFloat() * 0.1f - 0.05f);
                s.nudge = juce::jlimit (-0.5f, 0.5f, s.nudge + random.nextFloat() * 0.08f - 0.04f);
            }
    }

    void euclideanLane (SeqPattern& pattern, int pad, int length, int pulses,
                        int rotation, float velocity)
    {
        if (pad < 0 || pad >= kNumPads)
            return;
        length = juce::jlimit (1, kSeqSteps, length);
        pulses = juce::jlimit (1, length, pulses);
        rotation = ((rotation % length) + length) % length;
        for (int step = 0; step < kSeqSteps; ++step)
            pattern.rows[(size_t) pad][(size_t) step] = SeqStep {};

        for (int i = 0; i < length; ++i)
        {
            const auto source = (i + rotation) % length;
            if (((source * pulses) % length) < pulses)
                setStep (pattern, pad, i, velocity, 1.0f, 1);
        }
    }

    void rotatePattern (SeqPattern& pattern, int length, int shift)
    {
        length = juce::jlimit (1, kSeqSteps, length);
        if (length <= 1)
            return;
        shift = ((shift % length) + length) % length;
        if (shift == 0)
            return;
        for (int pad = 0; pad < kNumPads; ++pad)
        {
            auto& row = pattern.rows[(size_t) pad];
            std::array<SeqStep, kSeqSteps> rotated {};
            for (int i = 0; i < length; ++i)
                rotated[(size_t) ((i + shift) % length)] = row[(size_t) i];
            for (int i = 0; i < length; ++i)
                row[(size_t) i] = rotated[(size_t) i];
        }
    }

    void accentDownbeats (SeqPattern& pattern, int length)
    {
        length = juce::jlimit (1, kSeqSteps, length);
        for (int pad = 0; pad < kNumPads; ++pad)
            for (int step = 0; step < length; step += 4)
            {
                auto& s = pattern.rows[(size_t) pad][(size_t) step];
                if (s.on)
                    s.velocity = juce::jlimit (0.1f, 1.0f, s.velocity + 0.14f);
            }
    }

    juce::MemoryBlock serialiseSequencer (const SequencerData& data)
    {
        juce::MemoryBlock block;
        juce::MemoryOutputStream stream (block, false);
        stream.writeInt (kSequenceMagic);
        stream.writeInt (kSequenceFormatVersion);
        stream.writeInt (juce::jlimit (0, kSeqPatterns - 1, data.activePattern));
        stream.writeInt (juce::jlimit (1, kSeqSteps, data.length));
        stream.writeInt (sanitiseDivision (data.division));
        stream.writeFloat (juce::jlimit (0.0f, 0.66f, data.swing));
        stream.writeDouble (juce::jlimit (20.0, 300.0, data.bpm));
        stream.writeBool (data.hostSync);
        stream.writeBool (data.songMode);
        stream.writeInt (juce::jlimit (1, kSeqChainLength, data.chainLength));
        for (int i = 0; i < kSeqChainLength; ++i)
            stream.writeInt (juce::jlimit (0, kSeqPatterns - 1, data.chainPatterns[(size_t) i]));

        for (const auto& pattern : data.patterns)
            writePattern (stream, pattern);

        stream.flush();
        return block;
    }

    std::shared_ptr<SequencerData> deserialiseSequencer (const juce::MemoryBlock& data)
    {
        if (data.getSize() < 32)
            return {};
        juce::MemoryInputStream stream (data.getData(), data.getSize(), false);
        if (stream.readInt() != kSequenceMagic)
            return {};
        const auto version = stream.readInt();
        if (version < 1 || version > kSequenceFormatVersion)
            return {};

        auto result = std::make_shared<SequencerData>();
        result->activePattern = juce::jlimit (0, kSeqPatterns - 1, stream.readInt());
        result->length = juce::jlimit (1, kSeqSteps, stream.readInt());
        result->division = sanitiseDivision (stream.readInt());
        result->swing = juce::jlimit (0.0f, 0.66f, stream.readFloat());
        result->bpm = juce::jlimit (20.0, 300.0, stream.readDouble());
        result->hostSync = stream.readBool();
        if (version >= 2)
        {
            result->songMode = stream.readBool();
            result->chainLength = juce::jlimit (1, kSeqChainLength, stream.readInt());
            for (int i = 0; i < kSeqChainLength; ++i)
                result->chainPatterns[(size_t) i] = juce::jlimit (0, kSeqPatterns - 1, stream.readInt());
        }

        for (auto& pattern : result->patterns)
            if (! readPattern (stream, version, pattern))
                return {};

        return result;
    }

    juce::MemoryBlock serialisePattern (const SeqPattern& pattern)
    {
        SequencerData wrapper;
        wrapper.patterns[0] = pattern;
        wrapper.activePattern = 0;
        return serialiseSequencer (wrapper);
    }

    std::shared_ptr<SeqPattern> deserialisePattern (const juce::MemoryBlock& data)
    {
        const auto wrapper = deserialiseSequencer (data);
        if (wrapper == nullptr)
            return {};
        return std::make_shared<SeqPattern> (wrapper->patterns[0]);
    }

    juce::MemoryBlock makePatternMidiFile (const SequencerData& data)
    {
        const int division = sanitiseDivision (data.division);
        const double ticksPerStep = (double) kMidiTicksPerQuarter / division;
        const int barLength = juce::jlimit (1, kSeqSteps, data.length);
        const int chainLength = juce::jlimit (1, kSeqChainLength, data.chainLength);
        const int totalSteps = data.songMode ? barLength * chainLength : barLength;

        juce::MidiMessageSequence sequence;
        sequence.addEvent (juce::MidiMessage::tempoMetaEvent (
            (int) std::round (60000000.0 / juce::jmax (20.0, data.bpm))), 0.0);
        sequence.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        sequence.addEvent (juce::MidiMessage::textMetaEvent (3, "SKEER SliceForge pattern"), 0.0);

        for (int absolute = 0; absolute < totalSteps; ++absolute)
        {
            const int patternIndex = data.songMode
                ? juce::jlimit (0, kSeqPatterns - 1,
                                data.chainPatterns[(size_t) ((absolute / barLength) % chainLength)])
                : juce::jlimit (0, kSeqPatterns - 1, data.activePattern);
            const auto& pattern = data.patterns[(size_t) patternIndex];

            for (int pad = 0; pad < kNumPads; ++pad)
            {
                const int laneLength = laneLengthForPad (pattern, pad, barLength);
                const auto& s = pattern.rows[(size_t) pad][(size_t) wrapStep (absolute, laneLength)];
                if (! s.on)
                    continue;

                const double baseTick = (double) absolute * ticksPerStep
                                      + (double) s.nudge * ticksPerStep;
                const int flams = juce::jlimit (0, 2, s.flam);
                const int ratchets = juce::jlimit (1, 4, s.ratchet);

                auto addNote = [&] (double tick)
                {
                    if (tick < 0.0)
                        return;
                    const int onTick = (int) std::round (tick);
                    const int offTick = onTick + juce::jmax (12, (int) std::round (ticksPerStep * 0.45));
                    const auto velocity = juce::jlimit (1, 127, (int) std::round (s.velocity * 127.0f));
                    sequence.addEvent (juce::MidiMessage::noteOn (10, 36 + pad, (juce::uint8) velocity),
                                       (double) onTick);
                    sequence.addEvent (juce::MidiMessage::noteOff (10, 36 + pad), (double) offTick);
                };

                for (int f = flams; f >= 1; --f)
                    addNote (baseTick - f * ticksPerStep * kFlamStep);
                for (int r = 0; r < ratchets; ++r)
                    addNote (baseTick + r * ticksPerStep / ratchets);
            }
        }

        sequence.updateMatchedPairs();
        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote (kMidiTicksPerQuarter);
        midiFile.addTrack (sequence);

        juce::MemoryBlock block;
        juce::MemoryOutputStream stream (block, false);
        midiFile.writeTo (stream);
        stream.flush();
        return block;
    }

    void processSequencerBlock (const SequencerData& data,
                                const SeqHostInfo& host,
                                bool internalPlaying,
                                double sampleRate,
                                int numSamples,
                                juce::Random& random,
                                SeqPlaybackState& state,
                                SeqBlockOutput& output)
    {
        output.triggerCount = 0;
        output.playheadStep = -1;
        output.transportPlaying = false;
        output.hostPlaying = false;
        output.hostBpm = host.valid ? juce::jmax (20.0, host.bpm) : juce::jmax (20.0, data.bpm);

        const bool hostMode = data.hostSync && host.valid;
        const bool hostPlaying = hostMode && host.playing;
        const bool internalMode = ! hostMode ? internalPlaying : (! host.playing && internalPlaying);

        if (hostPlaying)
        {
            state.internalWasPlaying = false;
            state.internalPpq = 0.0;
            clearHistory (state.lastInternalPadStep);
            scheduleHostBlock (data, host, sampleRate, numSamples, random, state, output);
            return;
        }

        state.hostWasPlaying = false;
        clearHistory (state.lastHostPadStep);

        if (internalMode)
        {
            scheduleInternalBlock (data, sampleRate, numSamples, random, state, output);
            return;
        }

        state.internalWasPlaying = false;
        state.internalPpq = 0.0;
        clearHistory (state.lastInternalPadStep);
    }
} // namespace sliceforge
