#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VaultLibrary.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
    class MockHostPlayHead final : public juce::AudioPlayHead
    {
    public:
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo position;
            position.setIsPlaying (playing);
            position.setPpqPosition (ppq);
            position.setBpm (bpm);
            return position;
        }
    };

    int failures = 0;
    int checks = 0;

    void check (bool condition, const juce::String& name, const juce::String& detail = {})
    {
        ++checks;
        if (condition)
            std::cout << "  ok    " << name << '\n';
        else
        {
            std::cout << "  FAIL  " << name;
            if (detail.isNotEmpty())
                std::cout << "  [" << detail << "]";
            std::cout << '\n';
            ++failures;
        }
    }

    void setParam (SliceForgeAudioProcessor& processor, const juce::String& id, float value)
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (id))
        {
            const auto& range = parameter->getNormalisableRange();
            parameter->setValueNotifyingHost (range.convertTo0to1 (value));
        }
    }

    float getParam (SliceForgeAudioProcessor& processor, const juce::String& id)
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (id))
        {
            const auto& range = parameter->getNormalisableRange();
            return range.convertFrom0to1 (parameter->getValue());
        }
        return 0.0f;
    }

    struct Rendered
    {
        std::array<float, sliceforge::kNumBuses> peaks {};
        std::array<std::array<float, 2>, sliceforge::kNumBuses> channelPeaks {};
        float total = 0.0f;
        bool finite = true;
    };

    Rendered render (SliceForgeAudioProcessor& processor, int blocks, float velocity = 1.0f, int pad = 0,
                     int note = -1, int startBlock = 0)
    {
        Rendered result;
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        for (int block = 0; block < blocks; ++block)
        {
            if (block == startBlock && note < 0)
                processor.triggerPad (pad, velocity);
            midi.clear();
            if (block == startBlock && note >= 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, note, velocity), 0);
            buffer.clear();
            processor.processBlock (buffer, midi);
            for (int bus = 0; bus < sliceforge::kNumBuses; ++bus)
            {
                auto busBuffer = processor.getBusBuffer (buffer, false, bus);
                for (int channel = 0; channel < busBuffer.getNumChannels(); ++channel)
                {
                    const auto* data = busBuffer.getReadPointer (channel);
                    for (int i = 0; i < busBuffer.getNumSamples(); ++i)
                    {
                        if (! std::isfinite (data[i]))
                            result.finite = false;
                        const auto value = std::abs (data[i]);
                        result.peaks[(size_t) bus] = juce::jmax (result.peaks[(size_t) bus], value);
                        result.channelPeaks[(size_t) bus][(size_t) juce::jmin (1, channel)] =
                            juce::jmax (result.channelPeaks[(size_t) bus][(size_t) juce::jmin (1, channel)], value);
                        result.total = juce::jmax (result.total, value);
                    }
                }
            }
        }
        return result;
    }

    /** Flat int16 PCM source for the deterministic audition-driven checks. */
    sliceforge::SamplePtr makeDcSource (int numSamples, double rate, float value)
    {
        juce::MemoryBlock pcm ((size_t) numSamples * 4);
        auto* out = static_cast<int16_t*> (pcm.getData());
        for (int i = 0; i < numSamples; ++i)
        {
            out[(size_t) i * 2] = (int16_t) std::round (value * 32767.0f);
            out[(size_t) i * 2 + 1] = (int16_t) std::round (value * 32767.0f);
        }
        return sliceforge::decodePcm16 (pcm, 2, rate, "OUTPUT STAGE TONE");
    }

    /** Removes one parameter entry from a saved state, so a kit can be rewound to the schema
        that existed before that parameter was added. APVTS matches children by their "id"
        property, so a missing entry restores the layout default rather than shifting values. */
    void stripParameter (juce::ValueTree& tree, const juce::String& id)
    {
        for (int i = tree.getNumChildren(); --i >= 0;)
        {
            auto child = tree.getChild (i);
            if (child.getProperty ("id").toString() == id)
                tree.removeChild (child, nullptr);
            else
                stripParameter (child, id);
        }
    }
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "SliceForge native smoke test\n";

    SliceForgeAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    check (processor.getSlices().size() >= 8, "factory source is sliced",
           juce::String (processor.getSlices().size()) + " slices");
    check (processor.getSamplePtr() != nullptr, "factory sample is loaded");
    check (processor.getPadStart (3) > 0.0, "pad starts are assigned");
    check (processor.getNumPrograms() == 6, "six factory programs");

    auto silent = render (processor, 4, 1.0f, 0, -1, 1000);
    check (silent.total == 0.0f, "engine is silent before a trigger");

    // One pad, one bus.
    setParam (processor, sliceforge::padParamId (0, "bus"), 1.0f);
    auto hit = render (processor, 20, 1.0f, 0);
    check (hit.finite, "pad output is finite");
    check (hit.peaks[0] > 0.005f, "pad hit is audible on output 1", juce::String (hit.peaks[0], 4));
    check (hit.peaks[1] < 0.0001f, "pad does not leak to output 2");

    // Pan.
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    setParam (processor, sliceforge::padParamId (0, "pan"), -1.0f);
    auto panned = render (processor, 20, 1.0f, 0);
    check (panned.channelPeaks[0][0] > panned.channelPeaks[0][1] * 3.0f, "pad pan works",
           juce::String (panned.channelPeaks[0][0], 4) + " / " + juce::String (panned.channelPeaks[0][1], 4));
    setParam (processor, sliceforge::padParamId (0, "pan"), 0.0f);

    // All eight buses.
    bool allBuses = true;
    for (int bus = 1; bus <= sliceforge::kNumBuses; ++bus)
    {
        processor.chokeAll();
        render (processor, 6, 1.0f, 0, -1, 999);
        setParam (processor, sliceforge::padParamId (0, "bus"), (float) bus);
        auto busHit = render (processor, 14, 1.0f, 0);
        if (busHit.peaks[(size_t) (bus - 1)] <= 0.002f)
        {
            allBuses = false;
            std::cout << "    missing bus " << bus << '\n';
        }
    }
    check (allBuses, "all eight stereo outputs are addressable");

    // Velocity.
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    setParam (processor, sliceforge::padParamId (0, "bus"), 1.0f);
    auto soft = render (processor, 20, 0.2f, 0);
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    auto loud = render (processor, 20, 1.0f, 0);
    check (loud.total > soft.total * 1.5f, "velocity changes level",
           juce::String (soft.total, 4) + " -> " + juce::String (loud.total, 4));

    // Mute and solo.
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    setParam (processor, sliceforge::padParamId (0, "mute"), 1.0f);
    auto muted = render (processor, 10, 1.0f, 0);
    check (muted.total < 0.0001f, "muted pad is silent");
    setParam (processor, sliceforge::padParamId (0, "mute"), 0.0f);

    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    setParam (processor, sliceforge::padParamId (2, "solo"), 1.0f);
    auto soloBlocked = render (processor, 10, 1.0f, 0);
    check (soloBlocked.total < 0.0001f, "solo silences other pads");
    auto soloAudible = render (processor, 20, 1.0f, 2);
    check (soloAudible.total > 0.002f, "soloed pad is audible");
    setParam (processor, sliceforge::padParamId (2, "solo"), 0.0f);

    // MIDI note triggering.
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    auto midiNote = render (processor, 20, 0.9f, 0, sliceforge::kPadMidiBase + 1, 0);
    check (midiNote.total > 0.002f, "MIDI note triggers a pad", juce::String (midiNote.total, 4));

    // Every model.
    bool allModelsAudible = true;
    for (int model = 0; model < sliceforge::kNumModels; ++model)
    {
        processor.chokeAll();
        render (processor, 6, 1.0f, 0, -1, 999);
        setParam (processor, sliceforge::padParamId (0, "model"), (float) model);
        setParam (processor, sliceforge::padParamId (0, "drumify"), 1.0f);
        setParam (processor, sliceforge::padParamId (0, "bus"), 1.0f);
        setParam (processor, sliceforge::padParamId (0, "length"), 0.22f);
        auto modelHit = render (processor, 18, 1.0f, 0);
        if (modelHit.total <= 0.001f)
        {
            std::cout << "    missing model " << model << '\n';
            allModelsAudible = false;
        }
    }
    check (allModelsAudible, "all sixteen models produce audio");

    // Factory presets.
    processor.applyFactoryPresetFromUi (1);
    check (processor.getPresetName() == "NIGHT KITCHEN", "factory preset loads", processor.getPresetName());
    check (processor.getSlices().size() > 0, "preset source is sliced");
    auto presetHit = render (processor, 20, 1.0f, processor.getSelectedPad());
    check (presetHit.total > 0.001f, "factory preset is audible");
    check (getParam (processor, "global.clip") == 0.0f && getParam (processor, "global.out") == 0.0f,
           "a factory preset leaves the output stage bypassed");

    // State round trip.
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    const auto presetName = processor.getPresetName();
    const auto sliceCount = processor.getSlices().size();
    const auto selected = processor.getSelectedPad();
    processor.applyFactoryPresetFromUi (4);
    processor.setStateInformation (state.getData(), (int) state.getSize());
    check (processor.getPresetName() == presetName, "preset name survives state round trip");
    check ((int) processor.getSlices().size() == sliceCount, "slices survive state round trip");
    check (processor.getSelectedPad() == selected, "selected pad survives state round trip");
    auto restoredHit = render (processor, 20, 1.0f, selected);
    check (restoredHit.total > 0.001f, "restored state is audible");

    // Undo / redo.
    setParam (processor, "global.volume", -2.4f);
    const auto beforeUndo = getParam (processor, "global.volume");
    processor.pushUndoSnapshot();
    setParam (processor, "global.volume", -24.0f);
    processor.undo();
    check (std::abs (getParam (processor, "global.volume") - beforeUndo) < 0.05f, "undo restores a parameter");
    processor.redo();
    check (std::abs (getParam (processor, "global.volume") - -24.0f) < 0.05f, "redo restores the edit");
    processor.undo();

    // Randomize.
    processor.randomizePads();
    check (processor.getPresetName() == "RANDOM KIT", "randomize changes the kit");
    processor.undo();

    // MIDI learn.
    processor.armMidiLearn (sliceforge::padParamId (0, "pitch"));
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 127), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    }
    check (processor.getMidiMappingForCc (74) == sliceforge::padParamId (0, "pitch"), "MIDI CC learn captures a parameter");
    {
        setParam (processor, sliceforge::padParamId (0, "pitch"), 0.0f);
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 0), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    }
    check (getParam (processor, sliceforge::padParamId (0, "pitch")) < -10.0f, "learned MIDI CC moves the parameter");

    // Step sequencer.
    {
        auto sequence = sliceforge::cloneSequencerData (*processor.getSequencerData());
        sequence->hostSync = false;
        sequence->bpm = 120.0;
        processor.setSequencerData (sequence);
        processor.setSequencerInternalPlaying (true);
        const auto sequenceHit = render (processor, 40, 1.0f, 0, -1, 999);
        check (sequenceHit.total > 0.001f, "step sequencer produces audio",
               juce::String (sequenceHit.total, 4));
        check (processor.getSequencerPlayheadStep() >= 0, "step sequencer advances the playhead");
        processor.setSequencerInternalPlaying (false);
        check (! processor.isSequencerTransportPlaying(), "step sequencer stops cleanly");

        juce::MemoryBlock sequenceState;
        processor.getStateInformation (sequenceState);
        auto changed = sliceforge::cloneSequencerData (*processor.getSequencerData());
        const int kickStep = 0;
        changed->patterns[(size_t) changed->activePattern].rows[0][(size_t) kickStep].on = false;
        processor.setSequencerData (changed);
        processor.setStateInformation (sequenceState.getData(), (int) sequenceState.getSize());
        const auto restored = processor.getSequencerData();
        check (restored != nullptr
                   && restored->patterns[(size_t) restored->activePattern].rows[0][(size_t) kickStep].on,
               "sequencer pattern survives state round trip");

        const bool beforeSeqUndo = restored->patterns[(size_t) restored->activePattern].rows[2][3].on;
        processor.pushUndoSnapshot();
        auto undoEdit = sliceforge::cloneSequencerData (*processor.getSequencerData());
        undoEdit->patterns[(size_t) undoEdit->activePattern].rows[2][3].on = ! beforeSeqUndo;
        processor.setSequencerData (undoEdit);
        processor.undo();
        const auto undone = processor.getSequencerData();
        check (undone != nullptr
                   && undone->patterns[(size_t) undone->activePattern].rows[2][3].on == beforeSeqUndo,
               "sequencer edits participate in undo");

        // "Copy pattern to": the whole pattern moves, the destination is replaced, undo agrees.
        auto dup = sliceforge::cloneSequencerData (*processor.getSequencerData());
        auto& source = dup->patterns[(size_t) 0];
        source.rows[3][7].on = true;
        source.rows[3][7].velocity = 0.42f;
        source.rows[3][7].ratchet = 3;
        source.rows[3][7].flam = 2;
        source.rows[3][7].nudge = -0.25f;
        source.rows[3][7].locks.enabled = true;
        source.rows[3][7].locks.pitch = 5.0f;
        source.laneMute[3] = true;
        source.laneSolo[5] = true;
        source.laneLength[9] = 7;
        auto& destination = dup->patterns[(size_t) 2];
        destination.rows[11][30].on = true;

        const auto selectedBefore = dup->activePattern;
        check (sliceforge::copyPatternTo (*dup, 0, 2), "copy pattern to succeeds");
        const auto& copied = dup->patterns[(size_t) 2].rows[3][7];
        check (copied.on && copied.velocity == 0.42f && copied.ratchet == 3 && copied.flam == 2
                   && copied.nudge == -0.25f && copied.locks.enabled && copied.locks.pitch == 5.0f,
               "copy pattern to duplicates steps, ratchets, flams, nudges and parameter locks");
        check (! dup->patterns[(size_t) 2].rows[11][30].on, "copy pattern to replaces the destination");
        check (dup->patterns[(size_t) 2].laneMute[3] && dup->patterns[(size_t) 2].laneSolo[5]
                   && dup->patterns[(size_t) 2].laneLength[9] == 7,
               "copy pattern to duplicates the lane mute, solo and loop settings");
        check (dup->patterns[(size_t) 0].rows[3][7].on && dup->patterns[(size_t) 0].laneLength[9] == 7,
               "copy pattern to leaves the source pattern intact");
        check (dup->activePattern == selectedBefore,
               "copy pattern to does not change which pattern is selected");

        const auto unchanged = sliceforge::serialiseSequencer (*dup);
        check (! sliceforge::copyPatternTo (*dup, 1, 1), "a pattern cannot be copied onto itself");
        check (! sliceforge::copyPatternTo (*dup, -1, 2), "copy pattern to rejects a negative index");
        check (! sliceforge::copyPatternTo (*dup, 0, sliceforge::kSeqPatterns), "copy pattern to rejects an out-of-range index");
        check (sliceforge::serialiseSequencer (*dup) == unchanged, "a rejected copy leaves the data untouched");

        auto withMarker = sliceforge::cloneSequencerData (*dup);
        withMarker->patterns[(size_t) 3].rows[5][12].on = true;
        processor.setSequencerData (withMarker);
        processor.pushUndoSnapshot();
        auto copiedAgain = sliceforge::cloneSequencerData (*processor.getSequencerData());
        sliceforge::copyPatternTo (*copiedAgain, 2, 3);
        processor.setSequencerData (copiedAgain);
        check (processor.getSequencerData()->patterns[(size_t) 3].rows[3][7].on
                   && ! processor.getSequencerData()->patterns[(size_t) 3].rows[5][12].on,
               "copy pattern to reaches the engine and replaces its target");
        processor.undo();
        const auto afterUndo = processor.getSequencerData();
        check (afterUndo != nullptr
                   && ! afterUndo->patterns[(size_t) 3].rows[3][7].on
                   && afterUndo->patterns[(size_t) 3].rows[5][12].on,
               "undo brings back the pattern that copy pattern to replaced");

        sliceforge::SeqPattern randomized;
        std::array<int, sliceforge::kNumPads> families {};
        families.fill (2);
        juce::Random random (1234);
        sliceforge::randomizePattern (randomized, 16, families, 0.5f, random);
        int randomisedSteps = 0;
        for (const auto& row : randomized.rows)
            for (int step = 0; step < 16; ++step)
                randomisedSteps += row[(size_t) step].on ? 1 : 0;
        check (randomisedSteps > 8, "step-randomizer creates a pattern");

        sliceforge::euclideanLane (randomized, 0, 16, 5, 0, 0.8f);
        int euclidSteps = 0;
        for (int step = 0; step < 16; ++step)
            euclidSteps += randomized.rows[0][(size_t) step].on ? 1 : 0;
        check (euclidSteps == 5, "Euclidean lane generator produces the requested pulses");
    }

    // Host-synchronised transport, using a mock playhead.
    {
        MockHostPlayHead playHead;
        SliceForgeAudioProcessor hostProcessor;
        hostProcessor.setPlayHead (&playHead);
        hostProcessor.prepareToPlay (48000.0, 512);
        auto sequence = sliceforge::cloneSequencerData (*hostProcessor.getSequencerData());
        sequence->hostSync = true;
        sequence->bpm = 120.0;
        sequence->length = 16;
        hostProcessor.setSequencerData (sequence);

        playHead.playing = true;
        playHead.ppq = 0.0;
        const auto hostHit = render (hostProcessor, 8, 1.0f, 0, -1, 999);
        check (hostHit.total > 0.001f, "host-synced sequencer produces audio",
               juce::String (hostHit.total, 4));
        check (hostProcessor.isSequencerHostPlaying(), "host playhead is detected");
        check (hostProcessor.getSequencerPlayheadStep() >= 0, "host-synced playhead advances");

        playHead.ppq += 8.0 * 512.0 * 120.0 / 60.0 / 48000.0;
        playHead.playing = false;
        render (hostProcessor, 1, 1.0f, 0, -1, 999);
        check (! hostProcessor.isSequencerTransportPlaying(), "host stop stops the sequencer");
        hostProcessor.setPlayHead (nullptr);
    }

    // A parameter-locked hit must still reach the engine as audio.
    {
        MockHostPlayHead playHead;
        SliceForgeAudioProcessor lockedProcessor;
        lockedProcessor.setPlayHead (&playHead);
        lockedProcessor.prepareToPlay (48000.0, 512);
        auto sequence = sliceforge::cloneSequencerData (*lockedProcessor.getSequencerData());
        sequence->hostSync = true;
        sliceforge::clearPattern (sequence->patterns[0]);
        auto& lockedStep = sequence->patterns[0].rows[0][0];
        lockedStep.on = true;
        lockedStep.velocity = 1.0f;
        lockedStep.locks.enabled = true;
        lockedStep.locks.pitch = 12.0f;
        lockedStep.locks.length = 0.08f;
        lockedProcessor.setSequencerData (sequence);
        playHead.playing = true;
        playHead.ppq = 0.0;
        const auto lockedHit = render (lockedProcessor, 4, 1.0f, 0, -1, 999);
        check (lockedHit.total > 0.001f, "parameter-locked sequencer step is audible",
               juce::String (lockedHit.total, 4));
        lockedProcessor.setPlayHead (nullptr);
    }

    // Extended sequencer features: micro-timing, flams, polymeter, song mode,
    // parameter locks, pattern serialization and MIDI export.
    {
        const auto makeFeatureSequence = []
        {
            auto data = std::make_shared<sliceforge::SequencerData>();
            data->length = 16;
            data->division = 4;
            data->bpm = 120.0;
            data->hostSync = true;
            data->songMode = false;
            data->chainLength = sliceforge::kSeqChainLength;
            data->chainPatterns = { 0, 1, 0, 2, 0, 1, 3, 2 };
            return data;
        };

        sliceforge::SeqHostInfo host;
        host.valid = true;
        host.playing = true;
        host.bpm = 120.0;

        // Everything below renders at the block size a real host uses. That is the point:
        // a step's events are not contiguous (the flam graces precede the main hit, the
        // ratchets follow it), so with a small block they straddle two or three blocks.
        // The oversized 7000-sample block this suite used to pass hid a whole step inside
        // one block and never exercised that handover. Offsets come back as absolute
        // sample positions on the timeline, so a step split across blocks stays measurable.
        const auto renderOffsets = [] (const sliceforge::SequencerData& data,
                                       sliceforge::SeqHostInfo host,
                                       int numBlocks, int blockSize)
        {
            sliceforge::SeqPlaybackState state;
            juce::Random random (42);
            std::vector<int> offsets;
            const double blockPpq = (double) blockSize * (host.bpm / 60.0) / 48000.0;
            for (int block = 0; block < numBlocks; ++block)
            {
                host.ppq = blockPpq * (double) block;
                sliceforge::SeqBlockOutput out;
                sliceforge::processSequencerBlock (data, host, false, 48000.0, blockSize,
                                                   random, state, out);
                for (int i = 0; i < out.triggerCount; ++i)
                    offsets.push_back (block * blockSize + out.triggers[(size_t) i].sampleOffset);
            }
            return offsets;
        };

        const auto countAt = [] (const std::vector<int>& values, int position)
        {
            return (int) std::count (values.begin(), values.end(), position);
        };

        const auto describe = [] (const std::vector<int>& values)
        {
            juce::String detail (juce::String ((int) values.size()) + ":");
            for (size_t i = 0; i < values.size() && i < 8; ++i)
                detail << " " + juce::String (values[i]);
            return detail;
        };

        {
            // 120 BPM / 48 kHz / 4 steps per quarter note -> one step is 6000 samples,
            // so nudge 0.5 puts this hit at 3000.
            auto data = makeFeatureSequence();
            auto& step = data->patterns[0].rows[0][0];
            step.on = true;
            step.velocity = 1.0f;
            step.nudge = 0.5f;
            const auto offsets = renderOffsets (*data, host, 24, 512);
            check (offsets.size() == 1 && std::abs (offsets[0] - 3000) <= 1,
                   "step nudge shifts trigger timing", describe (offsets));
        }

        {
            // Grace gap is 0.12 of a step = 720 samples, so it lands in the block *before*
            // the one carrying the main hit. The old code subtracted the gap from a
            // block-local offset, got a negative number and silently dropped the grace.
            auto data = makeFeatureSequence();
            auto& step = data->patterns[0].rows[0][1];
            step.on = true;
            step.velocity = 1.0f;
            step.flam = 1;
            const auto offsets = renderOffsets (*data, host, 24, 512);
            check (offsets.size() == 2 && countAt (offsets, 5280) == 1 && countAt (offsets, 6000) == 1,
                   "flam generates a grace hit at host block size", describe (offsets));
        }

        {
            // x4 ratchet is one hit per 1500 samples; three of the four used to fall outside
            // the 512-sample window and were dropped with the main hit.
            auto data = makeFeatureSequence();
            auto& step = data->patterns[0].rows[0][1];
            step.on = true;
            step.velocity = 1.0f;
            step.ratchet = 4;
            const auto offsets = renderOffsets (*data, host, 24, 512);
            check (offsets.size() == 4 && countAt (offsets, 6000) == 1 && countAt (offsets, 7500) == 1
                       && countAt (offsets, 9000) == 1 && countAt (offsets, 10500) == 1,
                   "ratchet repeats survive a host block boundary", describe (offsets));
        }

        {
            // Flam and ratchet on one step: the handover must neither lose an event nor emit
            // one twice while neighbouring blocks look at the same position.
            auto data = makeFeatureSequence();
            auto& step = data->patterns[0].rows[0][1];
            step.on = true;
            step.velocity = 1.0f;
            step.flam = 2;
            step.ratchet = 4;
            const auto offsets = renderOffsets (*data, host, 24, 512);
            int duplicates = 0;
            for (const int position : { 4560, 5280, 6000, 7500, 9000, 10500 })
                duplicates += countAt (offsets, position) != 1 ? 1 : 0;
            check (offsets.size() == 6 && duplicates == 0,
                   "a step with flam and ratchet fires each event exactly once", describe (offsets));
        }

        {
            // A probability draw belongs to the step, not to the block rendering it. With an
            // independent coin at each of the three blocks that see a step, a rolled-out hit
            // could still leave its flam behind, or play the hit without its ratchets.
            auto data = makeFeatureSequence();
            for (int s = 0; s < 16; ++s)
            {
                auto& step = data->patterns[0].rows[0][(size_t) s];
                step.on = true;
                step.velocity = 1.0f;
                step.probability = 0.5f;
                step.flam = 1;
                step.ratchet = 2;
            }
            const auto offsets = renderOffsets (*data, host, 400, 512);
            int mixed = 0, played = 0, skipped = 0;
            for (int s = 1; s <= 33; ++s) // s = 0 is skipped: its grace precedes the timeline start
            {
                const int events = countAt (offsets, s * 6000 - 720) + countAt (offsets, s * 6000)
                                     + countAt (offsets, s * 6000 + 3000);
                if (events == 3)
                    ++played;
                else if (events == 0)
                    ++skipped;
                else
                    ++mixed;
            }
            juce::String spread = "mixed=";
            spread << mixed << " played=" << played << " skipped=" << skipped;
            check (mixed == 0 && played > 0 && skipped > 0,
                   "probability is decided per step, not per block", spread);
        }

        {
            // Swing on an odd step can put it after the next step's negative nudge.
            // Both events must survive the block boundary rather than one overwriting history.
            auto data = makeFeatureSequence();
            data->swing = 0.66f;
            data->patterns[0].rows[0][1].on = true;
            data->patterns[0].rows[0][1].velocity = 1.0f;
            data->patterns[0].rows[0][2].on = true;
            data->patterns[0].rows[0][2].velocity = 1.0f;
            data->patterns[0].rows[0][2].nudge = -0.5f;
            sliceforge::SeqPlaybackState featureState;
            sliceforge::SeqBlockOutput firstOut, secondOut;
            juce::Random random (12);
            host.ppq = 0.37;
            sliceforge::processSequencerBlock (*data, host, false, 48000.0, 720, random, featureState, firstOut);
            host.ppq = 0.40;
            sliceforge::processSequencerBlock (*data, host, false, 48000.0, 720, random, featureState, secondOut);
            check (firstOut.triggerCount + secondOut.triggerCount >= 2,
                   "micro-timed steps survive out-of-order block boundaries");
        }

        {
            auto data = makeFeatureSequence();
            data->patterns[0].laneLength[0] = 4;
            data->patterns[0].rows[0][0].on = true;
            data->patterns[0].rows[0][0].velocity = 1.0f;
            sliceforge::SeqPlaybackState featureState;
            sliceforge::SeqBlockOutput out;
            juce::Random random (9);
            host.ppq = 1.0; // absolute step 4, wrapped back to lane step 0
            sliceforge::processSequencerBlock (*data, host, false, 48000.0, 512, random, featureState, out);
            check (out.triggerCount > 0, "per-lane polymeter wraps independently");
        }

        {
            auto data = makeFeatureSequence();
            data->length = 4;
            data->songMode = true;
            data->patterns[0].rows[0][0].on = true;
            data->patterns[0].rows[0][0].velocity = 1.0f;
            data->patterns[1].rows[1][0].on = true;
            data->patterns[1].rows[1][0].velocity = 1.0f;
            sliceforge::SeqPlaybackState featureState;
            sliceforge::SeqBlockOutput out;
            juce::Random random (10);
            host.ppq = 1.0; // absolute step 4 -> chain slot B
            sliceforge::processSequencerBlock (*data, host, false, 48000.0, 512, random, featureState, out);
            check (out.triggerCount > 0 && out.triggers[0].pad == 1,
                   "song mode follows the pattern chain");
        }

        {
            auto data = makeFeatureSequence();
            auto& step = data->patterns[0].rows[0][0];
            step.on = true;
            step.velocity = 1.0f;
            step.locks.enabled = true;
            step.locks.pitch = 7.0f;
            step.locks.volume = -6.0f;
            step.locks.length = 0.12f;
            sliceforge::SeqPlaybackState featureState;
            sliceforge::SeqBlockOutput out;
            juce::Random random (11);
            host.ppq = 0.0;
            sliceforge::processSequencerBlock (*data, host, false, 48000.0, 512, random, featureState, out);
            check (out.triggerCount > 0 && out.triggers[0].hasLocks
                       && std::abs (out.triggers[0].locks.pitch - 7.0f) < 0.01f,
                   "parameter locks travel with sequencer triggers");
        }

        {
            auto data = makeFeatureSequence();
            data->patterns[0].laneLength[0] = 12;
            data->patterns[0].rows[0][0].on = true;
            data->patterns[0].rows[0][0].nudge = 0.25f;
            data->patterns[0].rows[0][0].flam = 2;
            data->patterns[0].rows[0][0].locks.enabled = true;
            data->patterns[0].rows[0][0].locks.tone = 3.0f;
            const auto block = sliceforge::serialiseSequencer (*data);
            const auto restored = sliceforge::deserialiseSequencer (block);
            check (restored != nullptr
                       && restored->patterns[0].laneLength[0] == 12
                       && std::abs (restored->patterns[0].rows[0][0].nudge - 0.25f) < 0.01f
                       && restored->patterns[0].rows[0][0].flam == 2
                       && restored->patterns[0].rows[0][0].locks.enabled,
                   "extended step/pattern data survives state serialization");
        }

        {
            const auto afro = sliceforge::makePatternPreset (sliceforge::PatternPreset::afro);
            check (afro.laneLength[6] == 12 && afro.rows[0][0].on,
                   "factory pattern preset contains polymeter and notes");
            auto data = makeFeatureSequence();
            data->patterns[0] = afro;
            const auto block = sliceforge::serialisePattern (afro);
            const auto restored = sliceforge::deserialisePattern (block);
            check (restored != nullptr && restored->laneLength[6] == 12,
                   "pattern presets save and load correctly");
        }

        {
            auto data = makeFeatureSequence();
            data->patterns[0].rows[0][0].on = true;
            data->patterns[0].rows[0][0].velocity = 0.9f;
            data->patterns[0].rows[0][0].flam = 1;
            data->patterns[0].rows[0][0].ratchet = 2;
            data->songMode = true;
            data->chainLength = 2;
            const auto midi = sliceforge::makePatternMidiFile (*data);
            const auto* bytes = static_cast<const char*> (midi.getData());
            check (midi.getSize() > 64 && bytes[0] == 'M' && bytes[1] == 'T'
                       && bytes[2] == 'h' && bytes[3] == 'd',
                   "MIDI pattern export produces a standard MIDI file");

            juce::MemoryInputStream midiStream (midi.getData(), midi.getSize(), false);
            juce::MidiFile parsed;
            check (parsed.readFrom (midiStream) && parsed.getNumTracks() > 0,
                   "exported MIDI file can be parsed again");
        }
    }

    // SKEER Sample Vault link. This is a graceful integration: when the vault is
    // installed/indexed it must expose searchable tracks; when it is absent the
    // status must explain why rather than crashing.
    {
        sliceforge::VaultLibrary vault;
        const bool available = vault.refreshStatus();
        if (available)
        {
            sliceforge::VaultQuery query;
            query.limit = 12;
            const auto tracks = vault.queryTracks (query);
            check (vault.getStatus().totalTracks > 0 && ! tracks.empty()
                       && tracks.front().path.isNotEmpty(),
                   "SKEER Sample Vault index is readable");

            // What the vault contributes that no other check here does is a path written
            // by *another app's* database. How many of those paths still resolve is a
            // property of this machine's music library — 152,273 of its rows have never
            // been analysed and most of the ones flagged `missing=0` are gone — so the
            // scan below reports and is deliberately not asserted. Asserting on it makes
            // the suite go red when nothing in SliceForge changed, which is what it did
            // on 2026-11-15 with a 4 KB macOS sidecar.
            int vaultTried = 0;
            int vaultLoaded = 0;
            SliceForgeAudioProcessor vaultProcessor;
            vaultProcessor.prepareToPlay (48000.0, 512);
            for (const auto& track : tracks)
            {
                if (vaultTried == 8)
                    break;
                const juce::File candidate (track.path);
                if (! candidate.existsAsFile() || ! candidate.hasFileExtension ("wav")
                        || candidate.getSize() > 64 * 1024 * 1024)
                    continue;
                ++vaultTried;
                if (vaultProcessor.loadUserSampleFromFile (candidate))
                    ++vaultLoaded;
            }
            vaultProcessor.releaseResources();
            std::cout << "  info  vault: " << vaultTried << " listed wavs readable, "
                      << vaultLoaded << " decoded\n";

            // The gate for the loader itself, on a file this test owns. The writer is
            // asserted first because a broken writer would otherwise make the loader
            // check pass by loading a file nobody can trust.
            const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory);
            const auto probeFile = temp.getChildFile ("SliceForgeLoaderProbe.wav");
            const auto sidecarFile = temp.getChildFile ("SliceForgeSidecarProbe.wav");
            juce::AudioBuffer<float> tone (2, 12000);
            for (int i = 0; i < tone.getNumSamples(); ++i)
            {
                const auto phase = juce::MathConstants<float>::twoPi * 220.0f * (float) i / 48000.0f;
                tone.setSample (0, i, 0.4f * std::sin (phase));
                tone.setSample (1, i, 0.4f * std::sin (phase * 1.5f));
            }
            check (sliceforge::writeStereoWav (probeFile, tone, 48000.0) && probeFile.existsAsFile(),
                   "the loader probe writes a wav");

            SliceForgeAudioProcessor loader;
            loader.prepareToPlay (48000.0, 512);
            const auto loaded = loader.loadUserSampleFromFile (probeFile);
            const auto loadedSample = loader.getSamplePtr();
            check (loaded && loadedSample != nullptr && loadedSample->buffer.getNumSamples() == 12000
                       && loadedSample->buffer.getNumChannels() == 2,
                   "a real wav loads through the user-sample path",
                   loaded ? juce::String (loadedSample == nullptr ? -1
                                                                  : loadedSample->buffer.getNumSamples())
                          : juce::String ("load returned false"));

            // The file that turned this suite red on a real library: a macOS AppleDouble
            // sidecar, `.wav` by suffix, 4096 bytes, `Mac ` at offset 8, not audio at all.
            // The vault lists it as a track, so refusing it is the plugin's job. Refusal is
            // measured as "the kit is still the one you had", because a fresh processor
            // already carries a factory source and a null sample is not what refusing looks
            // like — that mistake made this check fail on a plugin that was behaving.
            juce::MemoryBlock sidecar ((size_t) 4096);
            sidecar.fillWith (0);
            const char sidecarHeader[12] { '\x00', '\x05', '\x16', '\x07',
                                           '\x00', '\x02', '\x00', '\x00', 'M', 'a', 'c', ' ' };
            std::memcpy (sidecar.getData(), sidecarHeader, 12);
            check (sidecarFile.replaceWithData (sidecar.getData(), sidecar.getSize()),
                   "the sidecar probe writes its fixture");
            const auto refused = loader.loadUserSampleFromFile (sidecarFile);
            const auto afterRefusal = loader.getSamplePtr();
            check (! refused && afterRefusal != nullptr
                       && afterRefusal->buffer.getNumSamples() == 12000,
                   "a non-audio file named .wav is refused and does not replace the kit",
                   refused ? juce::String ("load returned true")
                           : juce::String (afterRefusal == nullptr ? -1
                                                                    : afterRefusal->buffer.getNumSamples()));
            loader.releaseResources();
            probeFile.deleteFile();
            sidecarFile.deleteFile();
        }
        else
        {
            std::cout << "  info  Sample Vault not available: "
                      << vault.getStatus().error << '\n';
            check (vault.getStatus().error.isNotEmpty(),
                   "Sample Vault absence is reported gracefully");
        }
    }

    // Sidechain recording.
    {
        SliceForgeAudioProcessor recorder;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        for (int i = 0; i < sliceforge::kNumBuses; ++i)
            layout.outputBuses.add (juce::AudioChannelSet::stereo());
        check (recorder.setBusesLayout (layout), "sidechain bus layout accepted");
        recorder.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (juce::jmax (2, recorder.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        recorder.toggleRecording();
        for (int block = 0; block < 24; ++block)
        {
            buffer.clear();
            auto input = recorder.getBusBuffer (buffer, true, 0);
            for (int channel = 0; channel < input.getNumChannels(); ++channel)
                for (int i = 0; i < input.getNumSamples(); ++i)
                    input.setSample (channel, i, 0.25f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                                                   * (float) (block * 512 + i) / 48000.0f));
            recorder.processBlock (buffer, midi);
        }
        recorder.toggleRecording();
        for (int block = 0; block < 3; ++block)
        {
            buffer.clear();
            recorder.processBlock (buffer, midi);
        }
        check (recorder.isRecordingTakeReady(), "recording take is ready");
        recorder.adoptRecordingTake();
        check (recorder.getSourceName() == "RECORDED INPUT", "recorded take becomes the source");
        check (recorder.getSamplePtr() != nullptr && recorder.getSamplePtr()->getDurationSeconds() > 0.15,
               "recorded sample has the expected length");
        juce::MemoryBlock customState;
        recorder.getStateInformation (customState);
        SliceForgeAudioProcessor restored;
        restored.setStateInformation (customState.getData(), (int) customState.getSize());
        check (restored.getSourceName() == "RECORDED INPUT", "custom sample survives state round trip");
        check (restored.getSamplePtr() != nullptr && restored.getSamplePtr()->getDurationSeconds() > 0.15,
               "custom sample audio survives state round trip");
    }

    // Vault audition: deterministic decode, one-shot end, loop wrap, rate conversion, the
    // limiter that keeps preview-plus-kit inside full scale, and the bus-0 wiring.
    {
        const auto makeTone = [] (int numSamples, double rate, float leftValue, float rightValue)
        {
            juce::MemoryBlock pcm ((size_t) numSamples * 4);
            auto* out = static_cast<int16_t*> (pcm.getData());
            for (int i = 0; i < numSamples; ++i)
            {
                out[(size_t) i * 2] = (int16_t) std::round (leftValue * 32767.0f);
                out[(size_t) i * 2 + 1] = (int16_t) std::round (rightValue * 32767.0f);
            }
            return sliceforge::decodePcm16 (pcm, 2, rate, "AUDITION TONE");
        };

        sliceforge::PreviewPlayer player;
        player.prepare (48000.0);
        auto tone = makeTone (100, 48000.0, 0.5f, -0.25f);
        check (tone != nullptr, "audition test source decodes");
        if (tone != nullptr)
        {
            juce::AudioBuffer<float> mix (2, 256);
            player.play (tone, false);
            check (player.isPlaying(), "audition starts playing on play()");
            mix.clear();
            player.render (mix, 64);
            check (std::abs (mix.getSample (0, 30) - 0.45f) < 0.002f
                       && std::abs (mix.getSample (1, 30) + 0.225f) < 0.002f,
                   "audition renders both channels at the trim level",
                   juce::String (mix.getSample (0, 30), 4) + " / " + juce::String (mix.getSample (1, 30), 4));
            check (player.getAdvanceStamp() == 1, "audition advances once per rendered block");

            mix.clear();
            player.render (mix, 64);
            check (! player.isPlaying(), "one-shot audition stops at the end of the file");
            check (player.getProgress() > 0.99, "playhead rests at the end",
                   juce::String (player.getProgress(), 3));

            mix.clear();
            player.render (mix, 64);
            check (mix.getMagnitude (0, 64) == 0.0f, "a finished audition adds nothing");

            player.play (tone, true);
            check (player.isLoopEnabled(), "loop request is remembered");
            for (int i = 0; i < 33; ++i)
            {
                mix.clear();
                player.render (mix, 3);
            }
            check (player.isPlaying(), "looping audition keeps playing past the end");
            check (player.getProgress() < 0.03, "loop wraps to the start",
                   juce::String (player.getProgress(), 3));
            player.stop();
            check (player.getProgress() == 0.0 && ! player.isPlaying(), "stop rewinds the playhead");

            auto halfRate = makeTone (100, 24000.0, 0.25f, 0.25f);
            player.play (halfRate, false);
            mix.clear();
            player.render (mix, 100);
            check (player.getProgress() > 0.45 && player.getProgress() < 0.55,
                   "a 24 kHz file consumes half the host block", juce::String (player.getProgress(), 3));

            player.play (tone, false);
            for (int channel = 0; channel < 2; ++channel)
                for (int i = 0; i < 64; ++i)
                    mix.setSample (channel, i, 0.9f);
            player.render (mix, 64);
            check (mix.getMagnitude (0, 64) > 0.9f && mix.getMagnitude (0, 64) <= 0.9991f
                       && std::abs (mix.getSample (1, 10) - 0.675f) < 0.002f,
                   "audition over a hot bus is limited, never clipped",
                   juce::String (mix.getMagnitude (0, 64), 4));
            player.stop();
        }

        SliceForgeAudioProcessor audition;
        audition.prepareToPlay (48000.0, 512);
        const auto idle = render (audition, 2, 1.0f, 0, -1, 1000);
        check (idle.total == 0.0f, "audition host is silent before a preview");
        audition.getPreviewPlayer().play (makeTone (4000, 48000.0, 0.5f, 0.5f), false);
        const auto during = render (audition, 4, 1.0f, 0, -1, 1000);
        audition.getPreviewPlayer().stop();
        audition.releaseResources();
        check (during.finite && during.peaks[0] > 0.3f && during.peaks[0] < 0.6f
                   && during.channelPeaks[0][1] > 0.3f,
               "audition reaches the host on output bus 1", juce::String (during.peaks[0], 4));
        check (during.peaks[1] < 0.0001f, "audition stays off the other output buses");
    }

    // Output stage: the soft clipper is bit-exact while bypassed, has a unity slope up to its
    // knee, bends monotonically to just under full scale, is trimmed by OUT GAIN after the
    // curve, and the last clamp keeps every host bus inside the ceiling.
    {
        sliceforge::OutputStage stage;
        stage.prepare();
        check (std::abs (stage.knee - sliceforge::OutputStage::kCeiling) < 1.0e-6f && stage.headroom == 0.0f
                   && stage.gain == 1.0f,
               "output stage boots bypassed",
               juce::String (stage.knee, 3) + " / " + juce::String (stage.gain, 3));

        juce::AudioBuffer<float> calm (2, 8);
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 8; ++i)
                calm.setSample (channel, i, (float) (i + 1) * 0.1f * (channel == 0 ? 1.0f : -1.0f));
        std::array<float, 16> original {};
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 8; ++i)
                original[(size_t) channel * 8 + (size_t) i] = calm.getSample (channel, i);
        stage.process (calm, 8);
        bool bitExact = true;
        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < 8; ++i)
                bitExact = bitExact && calm.getSample (channel, i) == original[(size_t) channel * 8 + (size_t) i];
        check (bitExact, "a bypassed output stage changes nothing bit-exact");

        stage.setParameters (1.0f, 0.0f, true);
        check (std::abs (stage.knee - 0.349f) < 1.0e-4f && std::abs (stage.headroom - 0.65f) < 1.0e-4f,
               "full clip drops the knee to 0.349", juce::String (stage.knee, 4));
        check (stage.shape (0.2f) == 0.2f && stage.shape (-0.3f) == -0.3f,
               "nothing is attenuated below the knee");
        check (std::abs (stage.shape (sliceforge::OutputStage::kCeiling) - 0.8440f) < 2.0e-3f,
               "a full-scale peak lands at 0.844 at full clip",
               juce::String (stage.shape (sliceforge::OutputStage::kCeiling), 4));
        const auto knee = stage.knee;
        const auto overKnee = stage.shape (knee + 1.0e-4f) - stage.shape (knee);
        check (overKnee > 0.9e-4f && overKnee < 1.1e-4f, "the curve leaves the knee with slope 1",
               juce::String (overKnee * 1.0e4f, 3));

        bool monotonic = true;
        float previous = -1.0f;
        for (float x = 0.0f; x <= 2.0f; x += 0.05f)
        {
            const auto y = stage.shape (x);
            if (y <= previous || y > sliceforge::OutputStage::kCeiling)
                monotonic = false;
            previous = y;
        }
        check (monotonic, "the curve never turns back and never reaches full scale");

        stage.setParameters (1.0f, 6.0f, true);
        const auto madeUp = stage.shape (0.5f) * 1.99526f;
        check (std::abs (stage.gain - 1.99526f) < 1.0e-4f, "OUT GAIN +6 dB is a 2x trim",
               juce::String (stage.gain, 5));
        juce::AudioBuffer<float> trimmed (2, 4);
        trimmed.clear();
        trimmed.setSample (0, 0, 0.5f);
        trimmed.setSample (1, 0, -0.5f);
        stage.process (trimmed, 4);
        check (std::abs (trimmed.getSample (0, 0) - madeUp) < 0.002f
                   && std::abs (trimmed.getSample (1, 0) + madeUp) < 0.002f,
               "OUT GAIN is applied after the clipper", juce::String (trimmed.getSample (0, 0), 4));

        stage.setParameters (1.0f, 40.0f, true);
        check (std::abs (stage.gain - 3.98107f) < 1.0e-4f, "OUT GAIN clamps to the top of its range",
               juce::String (stage.gain, 4));
        trimmed.clear();
        trimmed.setSample (0, 0, 0.5f);
        stage.process (trimmed, 4);
        check (trimmed.getSample (0, 0) == sliceforge::OutputStage::kCeiling,
               "the ceiling clamp runs last and catches the makeup gain",
               juce::String (trimmed.getSample (0, 0), 4));

        stage.setParameters (0.0f, 0.0f, true);
        trimmed.clear();
        trimmed.setSample (0, 0, 1.4f);
        trimmed.setSample (0, 1, -1.4f);
        stage.process (trimmed, 4);
        check (trimmed.getSample (0, 0) == sliceforge::OutputStage::kCeiling
                   && trimmed.getSample (0, 1) == -sliceforge::OutputStage::kCeiling,
               "even bypassed, the stage cannot pass a sample above full scale");

        // Wired through the plugin, measured on a known signal: a 0 dBFS audition loop enters
        // bus 0 at 0.9, so the three settings have deterministic peaks of 0.9, 0.797 and the
        // ceiling. Nothing here depends on how loud the kit happens to be.
        SliceForgeAudioProcessor out;
        out.prepareToPlay (48000.0, 512);
        auto dc = makeDcSource (4000, 48000.0, 1.0f);
        check (dc != nullptr, "output stage test source decodes");
        out.getPreviewPlayer().play (dc, true);
        const auto openClip = render (out, 8, 1.0f, 0, -1, 1000);
        setParam (out, "global.clip", 1.0f);
        const auto hardClip = render (out, 8, 1.0f, 0, -1, 1000);
        setParam (out, "global.out", 6.0f);
        const auto madeUpHit = render (out, 8, 1.0f, 0, -1, 1000);
        out.getPreviewPlayer().stop();
        out.releaseResources();
        check (openClip.finite && std::abs (openClip.peaks[0] - 0.9f) < 0.01f,
               "a bypassed output stage leaves the host bus at its input level",
               juce::String (openClip.peaks[0], 4));
        check (hardClip.finite && std::abs (hardClip.peaks[0] - 0.7968f) < 0.01f,
               "full clip bends the same 0.9 peak to 0.797", juce::String (hardClip.peaks[0], 4));
        check (madeUpHit.finite && madeUpHit.peaks[0] <= 0.9991f && madeUpHit.peaks[0] > hardClip.peaks[0],
               "OUT GAIN after the clipper is caught by the ceiling",
               juce::String (madeUpHit.peaks[0], 4));
        check (hardClip.peaks[1] < 0.0001f, "the output stage does not invent signal on other buses");

        // A kit saved before the output stage existed has no global.clip / global.out in its
        // state; APVTS matches by id, so it must land on the bypassed defaults.
        juce::MemoryBlock currentState;
        processor.getStateInformation (currentState);
        auto legacyTree = juce::ValueTree::readFromData (currentState.getData(), currentState.getSize());
        check (legacyTree.isValid(), "output stage state reads back as a value tree");
        if (legacyTree.isValid())
        {
            stripParameter (legacyTree, "global.clip");
            stripParameter (legacyTree, "global.out");
            juce::MemoryBlock legacyBlock;
            juce::MemoryOutputStream stream (legacyBlock, false);
            legacyTree.writeToStream (stream);
            SliceForgeAudioProcessor legacy;
            legacy.prepareToPlay (48000.0, 512);
            legacy.setStateInformation (legacyBlock.getData(), (int) legacyBlock.getSize());
            check (getParam (legacy, "global.clip") == 0.0f && getParam (legacy, "global.out") == 0.0f,
                   "an older kit restores with the output stage bypassed");
            const auto legacyHit = render (legacy, 20, 1.0f, processor.getSelectedPad());
            check (legacyHit.finite && legacyHit.total > 0.001f, "the older kit still plays");
            legacy.releaseResources();
        }
    }

    // Warp crossover versus the engine sample rate.
    {
        const auto toneSource = [] (double rate, float freq, float amp, double seconds)
        {
            const auto numSamples = (int) std::llround (rate * seconds);
            juce::MemoryBlock pcm ((size_t) numSamples * 4);
            auto* out = static_cast<int16_t*> (pcm.getData());
            for (int i = 0; i < numSamples; ++i)
            {
                const auto v = amp * std::sin (juce::MathConstants<float>::twoPi * freq
                                                                * (float) i / (float) rate);
                out[(size_t) i * 2] = (int16_t) std::round (v * 32767.0f);
                out[(size_t) i * 2 + 1] = (int16_t) std::round (v * 32767.0f);
            }
            return sliceforge::decodePcm16 (pcm, 2, rate, "WARP PROBE");
        };

        const auto warpRms = [&toneSource] (double rate, float warp)
        {
            sliceforge::EngineRuntime runtime;
            runtime.audible[0] = true;
            auto& pad = runtime.pads[0];
            pad.drumify = 0.0f;
            pad.originalGain = 0.0f;
            pad.drumifiedGain = 0.0f;
            pad.impact = 0.0f;
            pad.instability = 0.0f;
            pad.super = 0.0f;
            pad.toneShift = 0.0f;
            pad.eqLow = 0.0f;
            pad.eqMid = 0.0f;
            pad.eqHigh = 0.0f;
            pad.length = 1.0f;
            pad.volume = 0.0f;
            pad.warp = warp;
            sliceforge::DrumEngine engine;
            engine.prepare (rate, 512);
            engine.setSample (toneSource (rate, 300.0f, 0.3f, 1.5));
            engine.setRuntime (runtime);
            engine.trigger (0, 1.0f);
            double sum = 0.0;
            int count = 0;
            for (int block = 0; block < 40; ++block)
            {
                engine.clearBusBuffers (512);
                engine.processSegment (0, 512);
                engine.processMaster (512);
                auto& bus = engine.getBusBuffer (0);
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 512; ++i)
                    {
                        const auto value = (double) bus.getSample (channel, i);
                        sum += value * value;
                        ++count;
                    }
            }
            return (float) std::sqrt (sum / (double) juce::jmax (1, count));
        };

        // Gate for the warp crossover: what Warp does must not depend on the host's sample
        // rate. The 300 Hz probe tone is deliberate — the split is 200 Hz / 4.2 kHz, so a
        // coefficient pair computed for 48 kHz but running at 96 kHz moves the low corner to
        // 400 Hz and drops this tone into the boosted low band. Measured before the fix:
        // 2.19 / 2.41 / 4.13 dB across the three rates. After it: 2.39 / 2.41 / 2.50 dB,
        // and the 48 kHz numbers are bit-identical, so the reference rate does not move.
        const auto warpAmountDb = [&warpRms] (double rate)
        {
            const auto off = warpRms (rate, 0.0f);
            const auto on = warpRms (rate, 1.0f);
            return off > 0.0f && on > 0.0f ? 20.0 * std::log10 (on / off) : 0.0;
        };

        const auto at44 = warpAmountDb (44100.0);
        const auto at48 = warpAmountDb (48000.0);
        const auto at96 = warpAmountDb (96000.0);
        const auto spread = juce::jmax (juce::jmax (at44, at48), at96)
                              - juce::jmin (juce::jmin (at44, at48), at96);
        check (at48 > 1.0, "warp lifts a tone that sits above its low crossover",
               juce::String (at48, 2) + " dB");
        check (spread < 0.5, "warp does the same work at 44.1, 48 and 96 kHz",
               juce::String (at44, 2) + " / " + juce::String (at48, 2) + " / " + juce::String (at96, 2) + " dB");
    }

    // CPU benchmark.
    {
        processor.chokeAll();
        render (processor, 8, 1.0f, 0, -1, 999);
        const auto start = juce::Time::getHighResolutionTicks();
        for (int i = 0; i < 200; ++i)
            render (processor, 1, 1.0f, 0);
        const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        std::cout << "  info  CPU: " << juce::String (seconds * 100.0, 2) << "% of one core for 2.1s audio\n";
        check (seconds < 8.0, "engine is comfortably real-time");
    }

    std::cout << "\n" << (failures == 0 ? "PASS" : "FAIL") << " - " << (checks - failures) << "/" << checks
              << " checks passed\n";
    return failures == 0 ? 0 : 1;
}

