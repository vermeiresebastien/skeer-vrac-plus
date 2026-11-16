#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/Controls.h"
#include "VaultLibrary.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <tuple>
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

    /** Renders `blocks` blocks of 512 samples.
        `midiOut`, when given, accumulates whatever the plugin left in the MIDI buffer, stamped with
        its absolute sample position - that is the whole point of Step 28's checks, and it is opt-in
        so every pre-existing caller keeps the behaviour it was written against. */
    Rendered render (SliceForgeAudioProcessor& processor, int blocks, float velocity = 1.0f, int pad = 0,
                     int note = -1, int startBlock = 0, juce::MidiBuffer* midiOut = nullptr)
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
            if (midiOut != nullptr)
                midiOut->addEvents (midi, 0, buffer.getNumSamples(), block * buffer.getNumSamples());
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

    // The loops below walk the engine's own constants, so each of those claims ("all eight
    // outputs", "every model") would still pass if a constant shrank. Pin the shape once, here, in
    // numbers that do not come from the code under test.
    check (sliceforge::kNumBuses == 8 && sliceforge::kNumPads == 16
               && sliceforge::kNumFamilies == 4 && sliceforge::kNumModels == 16,
               "the engine is the 8-bus, 16-pad, 16-model instrument the checks assume",
               juce::String (sliceforge::kNumBuses) + "/" + juce::String (sliceforge::kNumPads)
                   + "/" + juce::String (sliceforge::kNumFamilies) + "/" + juce::String (sliceforge::kNumModels));
    // 21 controls per pad, nine globals before the pad loop, the two output-stage parameters after
    // it and `seq.pattern` appended last. A host that numbers parameters counts them, so this surface
    // is part of the file format. The expected value below is a literal on purpose: the derived form
    // `16 * 21 + 9 + 2 + 1` re-uses the layout's own arithmetic, so it would go green even if
    // `seq.pattern` were never registered - do not replace 348 with that expression.
    check (processor.getParameters().size() == 348,
           "the parameter surface is the documented 348 controls",
           juce::String ((int) processor.getParameters().size()));
    {
        // The member that earns the 348th, checked by identity rather than by arithmetic: range,
        // step, default and the fact that it is appended last, because every index below it has to
        // keep pointing at the control a saved session already numbered.
        auto* patternParam = processor.getValueTreeState().getParameter ("seq.pattern");
        const auto range = processor.getValueTreeState().getParameterRange ("seq.pattern");
        auto* lastParam = dynamic_cast<juce::HostedAudioProcessorParameter*> (
            processor.getParameters().getLast());
        const juce::String detail = "param="
            + (patternParam == nullptr ? juce::String ("null") : patternParam->getName (128))
            + " range=" + juce::String (range.start, 3) + ".." + juce::String (range.end, 3)
            + " step=" + juce::String (range.interval, 3)
            + " last=" + (lastParam == nullptr ? juce::String ("none") : lastParam->getParameterID());
        check (patternParam != nullptr && range.start == 0.0f && range.end == 3.0f
                   && range.interval == 1.0f && patternParam->getDefaultValue() == 0.0f
                   && patternParam->isDiscrete() && lastParam != nullptr
                   && lastParam->getParameterID() == "seq.pattern",
               "seq.pattern is a four-choice parameter appended after every existing id", detail);
    }

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
        // Pinned for the same reason as the host fixture below: this assertion is arithmetic on the
        // clock, so it must not be arithmetic on a default.
        sequence->division = 4;
        sequence->swing = 0.0f;
        processor.setSequencerData (sequence);
        processor.setSequencerInternalPlaying (true);
        const auto sequenceHit = render (processor, 40, 1.0f, 0, -1, 999);
        check (sequenceHit.total > 0.001f, "step sequencer produces audio",
               juce::String (sequenceHit.total, 4));
        /*  40 blocks of 512 at 120 BPM on the internal clock is 40 x 0.0213333 ppq, and the internal
            path reports the step at the *end* of the block (`Sequencer.cpp:357`), so this is
            floor (0.853 x 4) = 3 - not merely "some step". `>= 0` here accepted a clock that never
            moved, which is the one thing the name promised. */
        check (processor.getSequencerPlayheadStep() == 3, "step sequencer advances the playhead",
               juce::String (processor.getSequencerPlayheadStep()));
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
        // Pinned, not inherited: the expectation below is `floor (ppq * division)` and a default that
        // one day moves would turn this into a check on the factory rather than on the transport.
        sequence->division = 4;
        sequence->swing = 0.0f;
        hostProcessor.setSequencerData (sequence);

        playHead.playing = true;
        playHead.ppq = 0.0;
        const auto hostHit = render (hostProcessor, 8, 1.0f, 0, -1, 999);
        check (hostHit.total > 0.001f, "host-synced sequencer produces audio",
               juce::String (hostHit.total, 4));
        check (hostProcessor.isSequencerHostPlaying(), "host playhead is detected");
        /*  This head was frozen for the eight blocks above, so the step it reports is still 0 and the
            honest assertion here is not `>= 0` - that only proves the host path answered rather than
            returning the -1 "no usable transport" value, and a wire that latched on step 0 would print
            0 forever and read as a pass. Drive the head and name the step. Sixteen 512-sample blocks at
            120 BPM are 0.3413 ppq, floor (0.3413 * 4) + 1e-6 is 1 (`Sequencer.cpp:321`), one past where
            the engage above left it. The advance is exactly the engine's own block step, so each block's
            ppqStart meets the previous ppqEnd and the resync tolerance at `Sequencer.cpp:291` is never
            tripped - a bigger per-block jump would test the seek path and the render path at once. */
        for (int block = 0; block < 16; ++block)
        {
            playHead.ppq += 512.0 * 120.0 / 60.0 / 48000.0;
            render (hostProcessor, 1, 1.0f, 0, -1, 999);
        }
        check (hostProcessor.getSequencerPlayheadStep() == 1, "host-synced playhead advances",
               juce::String (hostProcessor.getSequencerPlayheadStep()));

        playHead.ppq += 8.0 * 512.0 * 120.0 / 60.0 / 48000.0;
        playHead.playing = false;
        render (hostProcessor, 1, 1.0f, 0, -1, 999);
        check (! hostProcessor.isSequencerTransportPlaying(), "host stop stops the sequencer");
        hostProcessor.setPlayHead (nullptr);
    }

    /*  Step 28 - the sequencer's own triggers have to reach the host as MIDI notes.
        The clock here is chosen so the expectation is arithmetic rather than transcription: 225 BPM
        with division 2 (two steps per quarter, so an eighth note per step) is 60/225 x 48000 / 2
        = 6400 samples per step at 48 kHz, and a block is 512, so the four lit steps below sit at
        absolute samples 0, 6400, 12800 and 19200 - block 0 offset 0, block 12 offset 256, block 25
        offset 0, block 37 offset 256. Both the on-grid and the mid-block case are therefore in the
        expected list, which is the part of the claim that "a note event lands at the sample offset
        the scheduler reported" actually has to survive. Each head position is written from
        `block * blockPpq` instead of accumulated, so the only rounding in the fixture is one
        multiplication per block. That same arithmetic is why the head has to report 225 BPM and not
        the 120 it is constructed with: scheduleHostBlock prefers the host's tempo over the document's
        (Sequencer.cpp:261), so the ppq-to-sample conversion that decides an offset is the head's. A
        head lying about the tempo would still be self-consistent - the audio would play the same
        wrong sample - but the list below would be pinning a transcription of the head's fiction
        rather than the step grid, and the two would only be reconcilable by re-deriving it. */
    {
        MockHostPlayHead playHead;
        playHead.bpm = 225.0;
        SliceForgeAudioProcessor midiProcessor;
        midiProcessor.setPlayHead (&playHead);
        midiProcessor.prepareToPlay (48000.0, 512);
        auto sequence = sliceforge::cloneSequencerData (*midiProcessor.getSequencerData());
        sequence->hostSync = true;
        sequence->bpm = 225.0;
        sequence->division = 2;
        sequence->swing = 0.0f;
        sequence->length = 16;
        sequence->activePattern = 0;
        sliceforge::clearPattern (sequence->patterns[0]);
        const auto lightStep = [] (sliceforge::SeqPattern& pattern, int pad, int step, float velocity)
        {
            // A whole SeqStep, not just `on`: flam 0, ratchet 1, nudge 0 and probability 1 mean this
            // fixture owes exactly one hit per lit step, so the expected list has no hidden sub-hits.
            auto& cell = pattern.rows[(size_t) pad][(size_t) step];
            cell = sliceforge::SeqStep {};
            cell.on = true;
            cell.velocity = velocity;
        };
        lightStep (sequence->patterns[0], 0, 0, 1.0f);
        lightStep (sequence->patterns[0], 0, 1, 1.0f);
        lightStep (sequence->patterns[0], 0, 2, 0.5f);
        lightStep (sequence->patterns[0], 5, 3, 1.0f);
        midiProcessor.setSequencerData (sequence);
        setParam (midiProcessor, "seq.pattern", 0.0f);

        juce::MidiBuffer emitted;
        juce::AudioBuffer<float> midiBuffer (juce::jmax (2, midiProcessor.getTotalNumOutputChannels()), 512);
        playHead.playing = true;
        for (int block = 0; block < 40; ++block)
        {
            playHead.ppq = (double) block * (512.0 * 225.0 / 60.0 / 48000.0);
            juce::MidiBuffer incoming;   // nothing is sent in: this is the plugin's own output bus
            midiBuffer.clear();
            midiProcessor.processBlock (midiBuffer, incoming);
            emitted.addEvents (incoming, 0, 512, block * 512);
        }

        /*  The expected list is arithmetic, not a transcript: note = 36 + pad (`kPadMidiBase`), one
            step = 6400 samples, note-off 32 samples after the note-on (`kPadMidiGateSamples`, and
            every one of these four sits far enough inside its block for the clamp to be inert).
            Velocity is compared separately, below, so a failure names which of the three properties
            the wire got wrong. */
        struct Expected
        {
            int sample;
            bool noteOn;
            int note;
        };
        const std::array<Expected, 8> expected { { { 0, true, 36 },      { 32, false, 36 },
                                                   { 6400, true, 36 },   { 6432, false, 36 },
                                                   { 12800, true, 36 },  { 12832, false, 36 },
                                                   { 19200, true, 41 },  { 19232, false, 41 } } };
        // 1.0 -> 127, 1.0 -> 127, 0.5 -> round (63.5) = 64, 1.0 -> 127: the exporter's own mapping.
        const std::array<int, 4> expectedVelocities { 127, 127, 64, 127 };

        std::vector<Expected> got;
        std::vector<int> noteOnVelocities;
        int otherEvents = 0, noteOns = 0, noteOffs = 0, wrongChannel = 0;
        for (const auto metadata : emitted)
        {
            const auto message = metadata.getMessage();
            if (! message.isNoteOn() && ! message.isNoteOff())
            {
                ++otherEvents;
                continue;
            }
            if (message.isNoteOn())
            {
                ++noteOns;
                noteOnVelocities.push_back ((int) message.getVelocity());
            }
            else
            {
                ++noteOffs;
            }
            if (message.getChannel() != 10)   // literal, not kPadMidiChannel: the map is the claim
                ++wrongChannel;
            got.push_back ({ (int) metadata.samplePosition, message.isNoteOn(), message.getNoteNumber() });
        }

        juce::String gotList;
        for (const auto& event : got)
            gotList += juce::String (event.sample) + (event.noteOn ? ":on " : ":off ")
                     + juce::String (event.note) + " ";

        check (noteOns == 4 && noteOffs == 4 && otherEvents == 0,
               "MIDI out carries nothing but four note-on/note-off pairs",
               juce::String (noteOns) + "/" + juce::String (noteOffs) + " notes, "
                   + juce::String (otherEvents) + " other events");

        bool listMatches = got.size() == expected.size();
        if (listMatches)
            for (size_t i = 0; i < expected.size(); ++i)
                listMatches = listMatches && got[i].sample == expected[i].sample
                                    && got[i].noteOn == expected[i].noteOn
                                    && got[i].note == expected[i].note;
        check (listMatches, "MIDI out notes carry the scheduler offset, 36 + pad and the 32-sample gate",
               juce::String ((int) got.size()) + " events: " + gotList);

        check (wrongChannel == 0 && ! got.empty(), "MIDI out notes are all on channel 10",
               juce::String (wrongChannel) + " off-channel of " + juce::String ((int) got.size()));

        bool velocitiesMatch = noteOnVelocities.size() == expectedVelocities.size();
        if (velocitiesMatch)
            for (size_t i = 0; i < expectedVelocities.size(); ++i)
                velocitiesMatch = velocitiesMatch && noteOnVelocities[i] == expectedVelocities[i];
        juce::String gotVelocities;
        for (const auto v : noteOnVelocities)
            gotVelocities += juce::String (v) + " ";
        check (velocitiesMatch, "MIDI out velocity uses the exporter's round(velocity x 127) mapping",
               juce::String ((int) noteOnVelocities.size()) + " note-ons: " + gotVelocities);

        playHead.playing = false;
        playHead.ppq = 40.0 * (512.0 * 225.0 / 60.0 / 48000.0);
        juce::MidiBuffer stopped;
        midiBuffer.clear();
        midiProcessor.processBlock (midiBuffer, stopped);
        check (stopped.getNumEvents() == 0, "a stopped host transport emits no MIDI",
               juce::String (stopped.getNumEvents()) + " events");

        // Both halves are load-bearing: the buffer must stay empty *and* the note must still be
        // audible, or "no pass-through" would also be satisfied by ignoring incoming MIDI.
        juce::MidiBuffer forwarded;
        const auto forwardedHit = render (midiProcessor, 1, 1.0f, 4, 40, 0, &forwarded);
        int forwardedNotes = 0;
        for (const auto metadata : forwarded)
            if (metadata.getMessage().isNoteOn() || metadata.getMessage().isNoteOff())
                ++forwardedNotes;
        check (forwardedNotes == 0 && forwardedHit.total > 0.001f,
               "incoming MIDI plays but is not passed through to the output bus",
               juce::String (forwarded.getNumEvents()) + " events, audio "
                   + juce::String (forwardedHit.total, 4));

        midiProcessor.setPlayHead (nullptr);
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
            // Step 23. The tolerance test compares the position a host reports with where the
            // *previous block* said this one would start, so the disagreement it tolerates has to
            // be a fraction of a block, not a constant. A host that quantises the position it
            // reports disagrees by a little every block, and at 90 BPM a 2048-sample block is
            // 0.064 ppq long: rounding under half of that is ordinary playback, but the fixed
            // 0.02 ppq window called it a seek and cleared the step history on every single block.
            // Clearing the history throws away the guard that stops the neighbouring block re-
            // emitting a step that straddles the boundary, so this is an audible defect, and the
            // clear count below is the direct measurement of it.
            const auto ramp = [] (int count, double advance)
            {
                std::vector<double> positions;
                for (int b = 0; b < count; ++b)
                    positions.push_back (advance * (double) b);
                return positions;
            };

            const auto renderPositions = [] (const sliceforge::SequencerData& data,
                                             sliceforge::SeqHostInfo host,
                                             const std::vector<double>& positions,
                                             int blockSize,
                                             sliceforge::SeqPlaybackState& state)
            {
                juce::Random random (5);
                for (const double ppq : positions)
                {
                    host.ppq = ppq;
                    sliceforge::SeqBlockOutput out;
                    sliceforge::processSequencerBlock (data, host, false, 48000.0, blockSize,
                                                       random, state, out);
                }
                return state.historyClears;
            };

            auto data = makeFeatureSequence();
            for (int s = 0; s < 4; ++s)
            {
                auto& step = data->patterns[0].rows[0][(size_t) s];
                step.on = true;
                step.velocity = 1.0f;
            }

            auto host90 = host;
            host90.bpm = 90.0;
            const double block90 = 2048.0 * (90.0 / 60.0) / 48000.0; // 0.064 ppq per block
            const auto report = [] (int clears, int expected)
            {
                juce::String detail = "clears=";
                detail << clears << " expected=" << expected;
                return detail;
            };

            {
                // A host that reports exactly where the block says the next one starts: the only
                // clear is the transport engage. Unchanged by the fix.
                sliceforge::SeqPlaybackState st;
                const auto clears = renderPositions (*data, host90, ramp (9, block90), 2048, st);
                check (clears == 1, "a block-accurate host clears its history once", report (clears, 1));
            }
            {
                // The defect. 0.025 ppq of per-block rounding: inside the block, outside the old
                // 0.02 ppq window. Must fail before the tolerance is scaled to the block.
                sliceforge::SeqPlaybackState st;
                const auto clears = renderPositions (*data, host90,
                                                     ramp (9, block90 + 0.025), 2048, st);
                check (clears == 1, "sub-block host position rounding is not a seek",
                       report (clears, 1));
            }
            {
                // Widening must not blind the scheduler to a real bar of seeking.
                sliceforge::SeqPlaybackState st;
                auto positions = ramp (9, block90);
                positions.push_back (positions.back() + block90 + 4.0); // one 90 BPM bar
                const auto clears = renderPositions (*data, host90, positions, 2048, st);
                check (clears == 2, "a one-bar host jump still clears the history", report (clears, 2));
            }
            {
                // How far the ceiling actually sits from the cases here: at 90 BPM on a
                // 2048-sample block half a block is 0.032 ppq, the tolerated rounding above is
                // 0.025 (0.39 of a block) and this seek is 0.048 (0.75 of one). So the pair
                // brackets the ceiling somewhere in (0.39, 0.75] of a block - it does not pin it
                // at a half, and nothing here would notice a formula that put it at 0.6.
                // Three quarters of a block has to be a seek; the rounding case above has not.
                sliceforge::SeqPlaybackState st;
                auto positions = ramp (9, block90);
                positions.push_back (positions.back() + block90 * 1.75);
                const auto clears = renderPositions (*data, host90, positions, 2048, st);
                check (clears == 2, "a three-quarter-block host seek clears the history", report (clears, 2));
            }
            {
                // The widening is one-sided: it may only ever widen. Half a 512-sample block at
                // 120 BPM is 0.0107 ppq, below the original window, so 0.015 ppq of rounding must
                // still be tolerated exactly as it was.
                sliceforge::SeqPlaybackState st;
                const double block512 = 512.0 * (120.0 / 60.0) / 48000.0; // 0.0213 ppq
                const auto clears = renderPositions (*data, host,
                                                     ramp (9, block512 + 0.015), 512, st);
                check (clears == 1, "a small block keeps the original tolerance window",
                       report (clears, 1));
            }
            {
                // The two directions of Step 23's own rule, measured on the fine grid, because that is
                // where they stop being compatible. At 4096 samples and 200 BPM a block is 0.2844 ppq,
                // so half a block - the tolerance - is 0.1422, WIDER than a whole 1/32 step (0.125 ppq).
                // This case therefore documents what the committed predicate actually does: a deliberate
                // one-step edit is swallowed - it adds no clear of its own, the single counted clear being
                // the transport engage - and leaves the pass counter alone, so a looped bar keeps its old
                // dropout set. It is a KNOWN DEFECT, recorded and
                // not fixed: the seam where the two directions separate is `numSamples x bpm = 720000`
                // at 1/32 on 48 kHz, and past it no tolerance can do both jobs. If anybody changes the
                // tolerance rule, flipping the expected value below to 2 is the falsifier - and the
                // control immediately after it is what such a change has to keep passing.
                sliceforge::SeqPlaybackState st;
                auto fineGrid = *data;
                fineGrid.division = 8; // 1/32: the finest grid the UI exposes
                const double block200 = 4096.0 * (200.0 / 60.0) / 48000.0; // 0.2844 ppq
                auto seekHost = host;
                seekHost.bpm = 200.0;
                auto positions = ramp (4, block200);
                positions[3] = positions[2] + block200 + 0.125; // one 1/32 step of real seeking
                const auto clears = renderPositions (fineGrid, seekHost, positions, 4096, st);
                check (clears == 1, "KNOWN DEFECT: a one-step 1/32 edit inside a 0.28 ppq block is not a seek",
                       report (clears, 1));
            }
            {
                // The other direction, and the reason capping the tolerance at half a step is not free.
                // At 4096 samples and 120 BPM a block is 0.1707 ppq, so 0.08 ppq of per-block rounding
                // is ordinary playback. A tolerance anywhere in (0.0853, 0.125) separates this pair from
                // the case above; `jmin (blockPpq * 0.5, 0.5 / division)` would put it at 0.0625, below
                // this 0.08, and this check would go red with clears = 9.
                sliceforge::SeqPlaybackState st;
                auto fineGrid = *data;
                fineGrid.division = 8;
                const double block120 = 4096.0 * (120.0 / 60.0) / 48000.0; // 0.1707 ppq
                auto noisyHost = host;
                noisyHost.bpm = 120.0;
                const auto clears = renderPositions (fineGrid, noisyHost,
                                                     ramp (9, block120 + 0.08), 4096, st);
                check (clears == 1, "sub-block rounding on a 1/32 grid is not a seek",
                       report (clears, 1));
            }
            {
                // The audible consequence, measured rather than argued. A host that reports a
                // little *behind* the prediction makes consecutive blocks' windows overlap, and
                // the step history is what stops the second block re-emitting the hit the first
                // one already scheduled. Clearing that history every block plays the hit twice.
                // 90 BPM / 2048 samples: a step is 8000 samples, so reporting 0.039 ppq per block
                // (1248 samples) against a 2048-sample window overlaps neighbours by 800, and the
                // hit at absolute step 8 falls inside both block 50 and block 51.
                sliceforge::SeqPlaybackState st;
                data->patterns[0].rows[0][8].on = true;
                data->patterns[0].rows[0][8].velocity = 1.0f;
                const auto positions = ramp (60, block90 - 0.025);
                auto overlapHost = host90;
                juce::Random random (5);
                std::vector<int> timeline;
                for (size_t b = 0; b < positions.size(); ++b)
                {
                    overlapHost.ppq = positions[b];
                    sliceforge::SeqBlockOutput out;
                    sliceforge::processSequencerBlock (*data, overlapHost, false, 48000.0, 2048,
                                                       random, st, out);
                    for (int i = 0; i < out.triggerCount; ++i)
                        timeline.push_back ((int) std::llround (positions[b] * 32000.0
                                                                 + (double) out.triggers[(size_t) i].sampleOffset));
                }
                const int fires = countAt (timeline, 64000);
                check (fires == 1, "overlapping host blocks do not play one hit twice",
                       [&] { juce::String s = "fires at 64000 = ";
                             s << fires << " expected=1 clears=" << st.historyClears;
                             return s; }());
            }
        }

        {
            // Step 22. The draw is a pure function of (pattern, pad, absolute step), which is what
            // stopped a step from being split across blocks. It also means a host that loops a bar
            // asks the same steps again and gets the same answers again, so the dropout set is
            // frozen for the whole session - the first bar of a loop is the last bar of a loop.
            // A rewind is already detected (it is the clear above), so the fix has to carry that
            // event into the key: same step, different pass, different draw.
            //
            // Why this cannot pass for the wrong reason: probabilityRollFor is pure splitmix64
            // over (pattern, pad, absolute step, pass). There is no RNG on the playback path at
            // all - every random.nextFloat() in Sequencer.cpp belongs to the pattern generator and
            // the randomiser - so the juce::Random carried across both renderPass calls below
            // cannot be what makes the two passes differ. The only thing that changes between them
            // is the pass counter, which is what the plan's invariant asks for: same seed, same
            // bar, second time through, a different set of steps sounds.
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

            const auto renderPass = [&] (const sliceforge::SequencerData& seq,
                                         sliceforge::SeqHostInfo passHost,
                                         int numBlocks, int blockSize,
                                         juce::Random& random,
                                         sliceforge::SeqPlaybackState& playState,
                                         std::vector<int>& offsets)
            {
                const double blockPpq = (double) blockSize * (passHost.bpm / 60.0) / 48000.0;
                for (int b = 0; b < numBlocks; ++b)
                {
                    passHost.ppq = blockPpq * (double) b; // same bar, twice: the host loops
                    sliceforge::SeqBlockOutput out;
                    sliceforge::processSequencerBlock (seq, passHost, false, 48000.0, blockSize,
                                                       random, playState, out);
                    for (int i = 0; i < out.triggerCount; ++i)
                        offsets.push_back (b * blockSize + out.triggers[(size_t) i].sampleOffset);
                }
            };

            // P = all three events of the step (grace, main, ratchet), - = none, x = a torn step.
            // `codes` keeps the same verdict per step as a vector, so two passes can be compared
            // by counting the steps that moved rather than by eyeballing one signature.
            const auto classify = [&] (const std::vector<int>& offsets, juce::String& signature,
                                       std::vector<char>& codes)
            {
                int mixed = 0, played = 0, skipped = 0;
                for (int s = 1; s <= 33; ++s) // s = 0 is skipped: its grace precedes the timeline
                {
                    const int events = countAt (offsets, s * 6000 - 720)
                                         + countAt (offsets, s * 6000)
                                         + countAt (offsets, s * 6000 + 3000);
                    if (events == 3)
                    {
                        ++played;
                        signature << "P";
                        codes.push_back ('P');
                    }
                    else if (events == 0)
                    {
                        ++skipped;
                        signature << "-";
                        codes.push_back ('-');
                    }
                    else
                    {
                        ++mixed;
                        signature << "x";
                        codes.push_back ('x');
                    }
                }
                return std::make_tuple (mixed, played, skipped);
            };

            sliceforge::SeqPlaybackState loopState;
            juce::Random random (11);
            std::vector<int> first, second;
            renderPass (*data, host, 400, 512, random, loopState, first);
            const int clearsBeforeLoop = loopState.historyClears;
            renderPass (*data, host, 400, 512, random, loopState, second);

            juce::String passOne, passTwo;
            std::vector<char> codesOne, codesTwo;
            const auto one = classify (first, passOne, codesOne);
            const auto two = classify (second, passTwo, codesTwo);
            juce::String detail;
            detail << "\n    pass 1 " << passOne << "\n    pass 2 " << passTwo;

            // Counted on top of the inequality above, not instead of it: `passOne != passTwo` is
            // satisfied if one character out of thirty-three flips, which is not the property the
            // plan asked for. The number is measured, not derived - the seed, the geometry and the
            // roll are all deterministic, so it is a literal.
            int reRolled = 0;
            for (size_t s = 0; s < codesOne.size (); ++s)
                if (codesOne[s] != codesTwo[s])
                    ++reRolled;

            check (clearsBeforeLoop == 1 && loopState.historyClears == 2,
                   "looping back to the start is one clear, not one per block",
                   [&] { juce::String s = "at loop=";
                         s << loopState.historyClears << " expected=2 before=" << clearsBeforeLoop;
                         return s; }());

            check (passOne != passTwo, "a looped bar re-rolls its probability", detail);

            check (reRolled == 21, "the re-roll moves whole steps, not one stray event",
                   [&] { juce::String s;
                         s << "re-rolled=" << reRolled << " of " << (int) codesOne.size()
                           << " steps  expected=21" << detail;
                         return s; }());

            const bool coherent = std::get<0> (one) == 0 && std::get<0> (two) == 0
                                     && std::get<1> (one) > 0 && std::get<2> (one) > 0
                                     && std::get<1> (two) > 0 && std::get<2> (two) > 0;
            check (coherent, "a re-rolled step keeps all of its events",
                   [&] { juce::String s;
                         s << "pass1 torn=" << std::get<0> (one)
                           << " played=" << std::get<1> (one)
                           << " skipped=" << std::get<2> (one)
                           << " | pass2 torn=" << std::get<0> (two)
                           << " played=" << std::get<1> (two)
                           << " skipped=" << std::get<2> (two);
                         return s; } ());
        }

        {
            /*  Step 22's spec asks for "probability 0.5 on all sixteen lanes" and what `4abe4b0` shipped
                was rows[0] - one pad, sixteen steps - so the `(pad + 1)` term of probabilityRollFor was
                only ever exercised at pad 0: delete the term and that gate stays green. This is the
                widening, added beside the single-lane check instead of in place of it, so the shipped
                record still describes what landed there.

                The naive version would have cost the other half of the gate. classify counts events at
                three offsets in one flat list, so with sixteen pads firing on the same step "this step's
                three sub-hits arrived or left together" becomes "forty-eight arrived" and the torn-step
                assertion stops meaning anything. Everything below is therefore filtered by
                SeqTrigger::pad and classified one lane at a time. */
            auto data = makeFeatureSequence();
            data->length = 16;
            data->swing = 0.0f;
            for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
                for (int s = 0; s < 16; ++s)
                {
                    auto& step = data->patterns[0].rows[(size_t) pad][(size_t) s];
                    step.on = true;
                    step.velocity = 1.0f;
                    step.probability = 0.5f;
                    step.flam = 1;
                    step.ratchet = 2;
                }

            constexpr int laneBlock = 512;
            const double laneBlockPpq = (double) laneBlock * (host.bpm / 60.0) / 48000.0;

            const auto renderLanePass = [&] (sliceforge::SeqPlaybackState& playState,
                                             juce::Random& random,
                                             std::vector<std::pair<int, int>>& hits)
            {
                for (int b = 0; b < 400; ++b)
                {
                    auto passHost = host;
                    passHost.ppq = laneBlockPpq * (double) b; // same bar, twice: the host loops
                    sliceforge::SeqBlockOutput out;
                    sliceforge::processSequencerBlock (*data, passHost, false, 48000.0, laneBlock,
                                                       random, playState, out);
                    for (int i = 0; i < out.triggerCount; ++i)
                        hits.push_back ({ b * laneBlock + out.triggers[(size_t) i].sampleOffset,
                                          out.triggers[(size_t) i].pad });
                }
            };

            // The same three offsets the single-lane classify uses, restricted to one pad's triggers.
            // P = all three events of the step, - = none, x = a torn step.
            const auto laneCode = [&] (const std::vector<std::pair<int, int>>& hits, int pad, int step)
            {
                int events = 0;
                for (const int position : { step * 6000 - 720, step * 6000, step * 6000 + 3000 })
                    for (const auto& hit : hits)
                        if (hit.second == pad && hit.first == position)
                            ++events;
                return events == 3 ? 'P' : (events == 0 ? '-' : 'x');
            };

            sliceforge::SeqPlaybackState laneState;
            juce::Random laneRandom (11);
            std::vector<std::pair<int, int>> laneFirst, laneSecond;
            renderLanePass (laneState, laneRandom, laneFirst);
            renderLanePass (laneState, laneRandom, laneSecond);

            int reRolledLanes = 0, tornLanes = 0;
            juce::String laneZero, laneDetails;
            std::vector<juce::String> signatures;
            for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
            {
                juce::String one, two;
                bool moved = false, torn = false;
                for (int s = 1; s <= 33; ++s) // s = 0 is skipped: its grace precedes the timeline
                {
                    const auto before = laneCode (laneFirst, pad, s);
                    const auto after = laneCode (laneSecond, pad, s);
                    one << juce::String::charToString (before);
                    two << juce::String::charToString (after);
                    moved = moved || before != after;
                    torn = torn || before == 'x' || after == 'x';
                }
                if (moved)
                    ++reRolledLanes;
                else
                    laneDetails << "\n    lane " << pad << " held its draws: " << one;
                if (torn)
                {
                    ++tornLanes;
                    laneDetails << "\n    lane " << pad << " tore a step: " << one << " / " << two;
                }
                if (pad == 0)
                    laneZero = one;
                else if (one == laneZero)
                    laneDetails << "\n    lane " << pad << " repeats lane 0";
                signatures.push_back (one);
            }

            int distinctLanes = 0;
            for (size_t pad = 0; pad < signatures.size(); ++pad)
            {
                bool unique = true;
                for (size_t other = 0; other < signatures.size(); ++other)
                    if (other != pad && signatures[other] == signatures[pad])
                        unique = false;
                if (unique)
                    ++distinctLanes;
            }

            check (reRolledLanes == sliceforge::kNumPads,
                   "every lane re-rolls on the second pass through the bar",
                   [&] { juce::String s = "lanes moved=";
                         s << reRolledLanes << " of " << sliceforge::kNumPads
                           << " expected=16" << laneDetails;
                         return s; } ());

            check (tornLanes == 0, "no lane loses a sub-hit while its neighbours keep theirs",
                   [&] { juce::String s = "torn lanes=";
                         s << tornLanes << " expected=0" << laneDetails;
                         return s; } ());

            /*  The falsifier for the term this whole block is about. probabilityRollFor hashes the pad
                in, so each lane gets its own dropout set; delete that term and all sixteen lanes draw
                from the same number, every signature collapses onto one, and this is the check that
                says so. The single-lane gate cannot see it at pad 0 no matter how wide it is. */
            check (distinctLanes == sliceforge::kNumPads,
                   "the sixteen lanes do not share one dropout set",
                   [&] { juce::String s = "unique lane signatures=";
                         s << distinctLanes << " of " << sliceforge::kNumPads
                           << " expected=16" << laneDetails << "\n    lane 0 " << laneZero;
                         return s; } ());
        }

        {
            /*  Step 26. `seq.pattern` is the choice the render path makes, and the document's own
                field is the mirror the offline callers read. Every case above proves nothing about
                that split: they all leave `liveActivePattern` defaulted, so the document is the
                only candidate. Here each pattern lights exactly one step, on a pad numbered after
                itself, so "which pattern did the scheduler play" is answered by which pad fired. */
            auto data = makeFeatureSequence();
            data->songMode = false;
            data->length = 16;
            data->swing = 0.0f;
            data->hostSync = true;
            for (auto& pattern : data->patterns)
                sliceforge::clearPattern (pattern);
            for (int pattern = 0; pattern < sliceforge::kSeqPatterns; ++pattern)
            {
                auto& step = data->patterns[(size_t) pattern].rows[(size_t) pattern][4];
                step.on = true;
                step.velocity = 1.0f;
            }

            const auto firedPads = [&] (int livePattern)
            {
                std::array<bool, sliceforge::kNumPads> heard {};
                sliceforge::SeqPlaybackState state;
                juce::Random random (7);
                constexpr int blockSize = 512;
                const double blockPpq = (double) blockSize * (host.bpm / 60.0) / 48000.0;
                for (int block = 0; block < 60; ++block)
                {
                    auto stepHost = host;
                    stepHost.ppq = blockPpq * (double) block;
                    sliceforge::SeqBlockOutput out;
                    sliceforge::processSequencerBlock (*data, stepHost, false, 48000.0, blockSize,
                                                       random, state, out, livePattern);
                    for (int i = 0; i < out.triggerCount; ++i)
                        heard[(size_t) out.triggers[(size_t) i].pad] = true;
                }
                juce::String list;
                for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
                    if (heard[(size_t) pad])
                        list << pad << " ";
                return list;
            };

            bool everyPatternItsOwn = true;
            juce::String wrong;
            for (int pattern = 0; pattern < sliceforge::kSeqPatterns; ++pattern)
            {
                // The mirror never gets to agree with the argument by construction: the callee falls
                // back to `data.activePattern` on the sentinel, so a document left on pattern 0 would
                // hand arm 0 a free pass and only three of the four arms would be load-bearing. Each
                // arm therefore stores the *next* pattern, so ignoring the parameter mispredicts every
                // one of them and the printed list says which value did land.
                data->activePattern = (pattern + 1) % sliceforge::kSeqPatterns;
                const auto heard = firedPads (pattern);
                // Exact list, not "contains": silence would satisfy a containsOnly check, and a
                // gate that a broken scheduler passes by playing nothing is no gate at all.
                if (heard != juce::String (pattern) + " ")
                {
                    everyPatternItsOwn = false;
                    wrong << "param " << pattern << " played [" << heard << "] expected ["
                          << pattern << "] ";
                }
            }
            check (everyPatternItsOwn,
                   "the scheduler plays the pattern the parameter names, not the document's", wrong);

            // Out of range is a host doing something legal-but-stupid, so it has to land inside the
            // bank rather than index past it.
            check (firedPads (99) == "3 ", "a pattern above the bank is clamped, not walked off",
                   firedPads (99));

            // Song mode is the one case where the parameter is deliberately silent: the chain owns
            // the choice, and a live selector that overrode it would fight the timeline.
            {
                auto songData = *data;
                songData.songMode = true;
                songData.chainPatterns = { 1, 1, 1, 1, 1, 1, 1, 1 };
                songData.chainLength = 8;
                songData.activePattern = 0;
                sliceforge::SeqPlaybackState songState;
                juce::Random random (7);
                std::array<bool, sliceforge::kNumPads> heard {};
                int fires = 0;
                constexpr int blockSize = 512;
                const double blockPpq = (double) blockSize * (host.bpm / 60.0) / 48000.0;
                for (int block = 0; block < 60; ++block)
                {
                    auto stepHost = host;
                    stepHost.ppq = blockPpq * (double) block;
                    sliceforge::SeqBlockOutput out;
                    // The parameter says pattern 3; the chain says 1, and the chain wins.
                    sliceforge::processSequencerBlock (songData, stepHost, false, 48000.0, blockSize,
                                                       random, songState, out, 3);
                    for (int i = 0; i < out.triggerCount; ++i)
                    {
                        heard[(size_t) out.triggers[(size_t) i].pad] = true;
                        ++fires;
                    }
                }
                // The set, not the last entry: overwriting one int inside the loop passes as long as the
                // final trigger of the final block happens to be pad 1, which is just how a leaky live
                // selector would still look clean. The literal is the fixture, not a measurement: every
                // one of the eight chain slots holds pattern 1, and the loop above gives pattern p its
                // only step on row p, so whichever slot the chain is on the fired pad is 1.
                juce::String list;
                for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
                    if (heard[(size_t) pad])
                        list << pad << " ";
                check (fires > 0 && list == "1 ",
                       "song mode lets the chain pick the pattern and the parameter stay silent",
                   [&] { juce::String s = "fires=";
                         s << fires << " heard=[" << list << "] expected=[1 ]";
                         return s; } ());
            }

            // The sentinel is what every caller without a running processor passes - and after this
            // step that includes the MIDI exporter, which now shares patternIndexForStep with the
            // render path instead of re-deriving the choice.
            {
                data->activePattern = 2;
                const auto heard = firedPads (sliceforge::kSeqPatternFromDocument);
                check (heard == "2 ", "with no parameter in sight the document still decides",
                       heard);
                data->activePattern = 0;
            }
        }

        {
            // Step 26, through the processor: the parameter is live state, the field inside the
            // pattern blob is what a session stores. These are the two directions a host can put
            // them in disagreement, and which one wins.
            SliceForgeAudioProcessor live;
            live.prepareToPlay (48000.0, 512);
            check (live.getPatternParameter() == 0, "a fresh plugin starts on pattern A",
                   juce::String (live.getPatternParameter()));

            /*  "Tells the host" has to be observed, not read back off the same object. Three conjuncts,
                three different proofs: `valueLanded` reads the parameter object, so it is true of a bare
                `setValue` and proves only that the value arrived; `getPatternParameter()` loads the APVTS
                adapter's denormalised atomic, which JUCE writes only inside its `parameterValueChanged`
                override (`juce_AudioProcessorValueTreeState.cpp:146-158`) - the listener dispatch that
                `setValueNotifyingHost` performs - so that is the conjunct that dies when the notify is
                dropped; and the spy watches the same dispatch from outside and pins the value *reported*
                to a listener, which the atomic by itself does not. */
            struct NotifySpy : juce::AudioProcessorValueTreeState::Listener
            {
                void parameterChanged (const juce::String& id, float value) override
                {
                    if (id == "seq.pattern")
                    {
                        ++calls;
                        last = value;
                    }
                }
                int calls = 0;
                float last = -1000.0f;
            };
            NotifySpy spy;
            live.getValueTreeState().addParameterListener ("seq.pattern", &spy);
            live.setPatternParameter (2);
            live.getValueTreeState().removeParameterListener ("seq.pattern", &spy);
            auto* parameter = live.getValueTreeState().getParameter ("seq.pattern");
            const bool valueLanded = parameter != nullptr
                && juce::roundToInt (parameter->convertFrom0to1 (parameter->getValue())) == 2;
            check (live.getPatternParameter() == 2 && valueLanded && spy.calls == 1
                       && juce::roundToInt (spy.last) == 2,
                   "selecting a pattern moves the parameter and tells the host",
                   [&] { juce::String s = "value=";
                         s << live.getPatternParameter() << " landed=" << (valueLanded ? "yes" : "no")
                           << " notifications=" << spy.calls << " notified=" << spy.last
                           << " expected=2 at 1 notification of 2";
                         return s; } ());

            juce::MemoryBlock saved;
            live.getStateInformation (saved);
            SliceForgeAudioProcessor restored;
            restored.prepareToPlay (48000.0, 512);
            restored.setStateInformation (saved.getData(), (int) saved.getSize());
            const auto restoredDoc = restored.getSequencerData();
            check (restored.getPatternParameter() == 2 && restoredDoc != nullptr
                       && restoredDoc->activePattern == 2,
                   "a session saved with the parameter on pattern C comes back on pattern C",
                   [&] { juce::String s = "param=";
                         s << restored.getPatternParameter()
                           << " doc=" << (restoredDoc == nullptr ? -99 : restoredDoc->activePattern);
                         return s; } ());
            restored.releaseResources();
            live.releaseResources();
        }

        {
            /*  The migration, from the shape an older session actually has: no seq.pattern child at
                all, and the pattern choice only inside the blob. The state is built by hand rather
                than by saving, because a current save always carries the child. */
            const auto legacyState = [] (int storedPattern, int parameterPattern, bool withParameter)
            {
                SliceForgeAudioProcessor source;
                source.prepareToPlay (48000.0, 512);
                source.setPatternParameter (parameterPattern);
                auto document = sliceforge::cloneSequencerData (*sliceforge::makeDefaultSequencerData());
                document->activePattern = storedPattern;
                auto state = source.getValueTreeState().copyState();
                // Only the legacy shape has the child removed. Taking it out unconditionally made
                // the third case a second copy of the first, and the disagreement gate would have
                // passed by falling into the migration branch.
                if (! withParameter)
                    if (const auto child = state.getChildWithProperty ("id", "seq.pattern"); child.isValid())
                        state.removeChild (child, nullptr);
                juce::ValueTree extra ("VRAC_EXTRA");
                extra.setProperty ("sequencer", sliceforge::serialiseSequencer (*document), nullptr);
                state.addChild (extra, -1, nullptr);
                juce::MemoryBlock blob;
                juce::MemoryOutputStream stream (blob, false);
                state.writeToStream (stream);
                source.releaseResources();
                return blob;
            };

            SliceForgeAudioProcessor fromOld;
            fromOld.prepareToPlay (48000.0, 512);
            const auto old = legacyState (3, 1, false);
            fromOld.setStateInformation (old.getData(), (int) old.getSize());
            const auto oldDoc = fromOld.getSequencerData();
            check (fromOld.getPatternParameter() == 3 && oldDoc != nullptr
                       && oldDoc->activePattern == 3,
                   "a session written before seq.pattern existed keeps its stored pattern",
                   [&] { juce::String s = "param=";
                         s << fromOld.getPatternParameter()
                           << " doc=" << (oldDoc == nullptr ? -99 : oldDoc->activePattern);
                         return s; } ());
            check (fromOld.getStateNotice().isEmpty(), "the legacy state reloads without a complaint",
                   fromOld.getStateNotice());
            fromOld.releaseResources();

            SliceForgeAudioProcessor split;
            split.prepareToPlay (48000.0, 512);
            const auto disagreeing = legacyState (3, 1, true);
            split.setStateInformation (disagreeing.getData(), (int) disagreeing.getSize());
            const auto splitDoc = split.getSequencerData();
            check (split.getPatternParameter() == 1 && splitDoc != nullptr
                       && splitDoc->activePattern == 1,
                   "when the parameter and the blob disagree, the parameter plays and the mirror follows",
                   [&] { juce::String s = "param=";
                         s << split.getPatternParameter()
                           << " doc=" << (splitDoc == nullptr ? -99 : splitDoc->activePattern);
                         return s; } ());
            split.releaseResources();
        }

        {
            /*  The wire between the parameter cache and the render path, gated end to end. Every other
                sequencer check calls processSequencerBlock and hands it the pattern index, so a
                processBlock passing a literal there would keep them green: the two ends were gated and
                the middle was not. This is the only route where the host decides - mock playhead, real
                processBlock - and the observable is which pads got stamped, because updatePadHit runs
                for every event the scheduler emits (PluginProcessor.cpp:398) and getPadHitStamp is
                public. The renders pass 999 as startBlock on purpose: the helper's default is 0, which
                fires a manual triggerPad and would make "nothing but the sequencer can be heard" false
                of this fixture.

                Each arm gets its own processor and its own fresh document and asserts only its own two
                pads, not a running total. That is deliberate: with one shared processor the stamps
                accumulate, so the last arm's expectation is the complete set and it goes green on
                "everything fired eventually" - measured, not theorised, on the first cut of this gate,
                where the document-fallback mutation reddened three arms and silently satisfied the
                fourth.

                The playhead advances by exactly one block per render call, and each pattern carries a
                second lit step so that advance is load-bearing: this fixture runs division 4, four steps
                to the quarter note, so step 1 sits at 0.25 ppq while a 512-sample block at 120 BPM is
                0.021333 ppq - step 1 lands in the twelfth block of sixteen, which the transport engage
                cannot produce. With the head frozen instead, the observable would be what the engage
                path plays and the hit count would be a function of the resync tolerance rule
                (Sequencer.cpp:291) rather than of a host that is moving. Stamps give pad identity, not
                iteration, so nothing here measures how often a step repeated, and the mid-block half of
                the once-per-block parameter read (PluginProcessor.cpp:315-316) is still untested -
                section 8C of docs/next-steps-plan.md says so in both places. */
            // Two pads per pattern - one on step 0, one on step 1 - in the order the scan below reads
            // them, and no pad equal to its own pattern number, so neither the bank order nor a document
            // left on its default can be mistaken for the parameter working.
            constexpr std::array<int, 2 * sliceforge::kSeqPatterns> wirePads
                { { 3, 10, 9, 4, 1, 12, 6, 7 } };
            constexpr double blockPpq = 512.0 * 120.0 / 60.0 / 48000.0;

            for (int p = 0; p < sliceforge::kSeqPatterns; ++p)
            {
                MockHostPlayHead playHead;
                SliceForgeAudioProcessor wire;
                wire.setPlayHead (&playHead);
                wire.prepareToPlay (48000.0, 512);
                auto document = sliceforge::cloneSequencerData (*wire.getSequencerData());
                document->hostSync = true;
                document->songMode = false;
                document->bpm = 120.0;
                document->length = 16;
                document->division = 4;
                document->swing = 0.0f;
                for (auto& pattern : document->patterns)
                {
                    sliceforge::clearPattern (pattern);
                    pattern.laneMute.fill (false);
                    pattern.laneSolo.fill (false);
                    pattern.laneLength.fill (0);
                }
                for (int q = 0; q < sliceforge::kSeqPatterns; ++q)
                {
                    document->patterns[(size_t) q].rows[(size_t) wirePads[(size_t) (2 * q)]][0].on = true;
                    document->patterns[(size_t) q].rows[(size_t) wirePads[(size_t) (2 * q + 1)]][1].on = true;
                }
                /*  The mirror deliberately disagrees with the parameter on every arm - the next
                    pattern, wrapping - because the callee falls back to `data.activePattern` on the
                    sentinel. Left at its default, the arm whose parameter equals the document would
                    pass by construction and the differential would be three arms wide, not four. */
                document->activePattern = (p + 1) % sliceforge::kSeqPatterns;
                wire.setSequencerData (document);
                wire.setPatternParameter (p);

                juce::String detail;
                // Both records read back before the first render. setPatternParameter writes the cache
                // through setValueNotifyingHost today; if that ever hops to the message thread this arm
                // stops being evidence, and this line says so instead of blaming the wire.
                if (wire.getPatternParameter() != p
                    || document->activePattern != (p + 1) % sliceforge::kSeqPatterns)
                {
                    detail << "fixture invalid param=" << wire.getPatternParameter()
                           << " want " << p << " mirror=" << document->activePattern;
                }
                else
                {
                    playHead.ppq = 0.0;
                    playHead.playing = false;
                    render (wire, 1, 1.0f, 0, -1, 999); // stop, so the arm re-engages on step 0
                    playHead.playing = true;
                    float peak = 0.0f;
                    for (int block = 0; block < 16; ++block)
                    {
                        peak = juce::jmax (peak, render (wire, 1, 1.0f, 0, -1, 999).total);
                        playHead.ppq += blockPpq; // after the block, so step 0 stays inside this run
                    }

                    juce::String heard, expected;
                    for (const auto pad : wirePads)
                        if (wire.getPadHitStamp (pad) != 0)
                            heard << pad << " ";
                    expected << wirePads[(size_t) (2 * p)] << " "
                             << wirePads[(size_t) (2 * p + 1)] << " ";
                    if (heard != expected)
                    {
                        // Naming the mirror's pattern in the failure says which record the render read:
                        // the pads listed are that pattern's two, so "fell back to the document" and
                        // "emitted nothing at all" cannot be mistaken for each other.
                        detail << "saw [" << heard << "] expected [" << expected
                               << "] mirror holds pattern " << (p + 1) % sliceforge::kSeqPatterns
                               << " peak " << peak;
                    }
                }

                juce::String name = "the host path plays the pattern the parameter names (pattern ";
                name << static_cast<char> ('A' + p) << ")";
                check (detail.isEmpty(), name, detail);
                wire.setPlayHead (nullptr);
                wire.releaseResources();
            }
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
            /* A short blob used to decode into whatever the stream reader handed back past the
               end of the buffer, which is zero for the step flags: a saved pattern came back empty,
               and the next save wrote that emptiness over the user's work. The decoder now refuses
               anything it cannot fill, so the pattern stays on disk until it can be read properly. */
            auto data = makeFeatureSequence();
            data->patterns[0].rows[0][0].on = true;
            const auto intact = sliceforge::serialiseSequencer (*data);
            check (sliceforge::deserialiseSequencer (intact) != nullptr,
                   "the serialiser output passes its own decoder");
            // Pins the framing so a field added to writePattern without patternPayloadBytes fails
            // here instead of mis-gating real user files: 70 bytes of header plus 4 patterns of
            // 16 lanes x (6 + 32 steps x 36).
            check (intact.getSize() == 70 + (size_t) 4 * 16 * (6 + 32 * 36),
                   "the sequencer blob is the documented size",
                   juce::String ((juce::int64) intact.getSize()));

            bool truncatedRejected = true;
            int acceptedRemoval = 0;
            for (const int remove : { 1, 8, (int) intact.getSize() / 2 })
            {
                const juce::MemoryBlock truncated (intact.getData(),
                                                   intact.getSize() - (size_t) remove);
                if (sliceforge::deserialiseSequencer (truncated) != nullptr)
                {
                    truncatedRejected = false;
                    acceptedRemoval = remove;
                }
            }
            check (truncatedRejected, "a blob missing 1, 8 or half its bytes is rejected",
                   acceptedRemoval > 0 ? "accepted a blob short by " + juce::String (acceptedRemoval)
                                       : juce::String());

            auto poisoned = intact;
            auto* bytes = static_cast<juce::uint8*> (poisoned.getData());
            // Pattern 0, lane 0, step 0: the velocity float sits right after the 70-byte header,
            // the 6-byte lane block and the step's on-flag. writeFloat/readFloat are little-endian,
            // so the quiet NaN 0x7fc00000 goes on the wire byte-reversed.
            const int velocityOffset = 70 + 6 + 1;
            bytes[velocityOffset] = 0x00; bytes[velocityOffset + 1] = 0x00;
            bytes[velocityOffset + 2] = 0xc0; bytes[velocityOffset + 3] = 0x7f;
            // Without this, a wrong offset or byte order would make the check below pass by
            // accident: it would be reading an untouched field and calling it a cleaned one.
            juce::MemoryInputStream poisonProbe (poisoned, false);
            poisonProbe.setPosition (velocityOffset);
            check (std::isnan (poisonProbe.readFloat()), "the poison lands as a NaN on velocity");
            const auto cleaned = sliceforge::deserialiseSequencer (poisoned);
            check (cleaned != nullptr && std::isfinite (cleaned->patterns[0].rows[0][0].velocity)
                       && std::abs (cleaned->patterns[0].rows[0][0].velocity - 0.8f) < 1.0e-6f,
                   "a NaN in a saved step falls back to the default instead of latching",
                   cleaned != nullptr ? juce::String (cleaned->patterns[0].rows[0][0].velocity, 4) : "rejected");
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

        {
            /*  Step 27: the printed clip has to be the pattern you heard.

                Until here the exporter read `step.on` and nothing else, so swing, mute, solo and
                probability were each decided twice: once by the render path and once by a straighter
                copy of it that knew about none of them. A clip dragged into a host therefore played a
                grid the plugin never played, and its dropout set belonged to no pass at all.

                The reference is the render path itself - the same fixture, the same host tempo, the
                same pass counter, scheduled block by block - and both sides are reduced to the same
                two currencies: (pad, step) for *which* hits, ticks for *when*. The lists have to be
                equal, not similar and not a subset.

                Pads 0 and 5 carry the differential because they have no flam and no ratchet, so one
                trigger is exactly one main hit and the step can be recovered from the position without
                ambiguity (0.3 swing moves a hit by 0.3 of a step, well inside the nearest one). Pad 1
                deliberately carries both sub-hit kinds, which is why it is gated by its own exact tick
                list rather than by membership: its grace sits 0.88 of a step before the hit and its
                ratchet half a step after, and either would round into a neighbouring step.

                The fixture is 120 BPM on 48 kHz at division 4: one step is 6000 samples, 240 ticks,
                so 25 samples per tick. */
            auto data = makeFeatureSequence();
            data->length = 16;
            data->division = 4;
            data->bpm = 120.0;
            data->swing = 0.3f;

            auto& exportPattern = data->patterns[0];
            for (int s = 0; s < 16; ++s)
            {
                auto& backbone = exportPattern.rows[0][(size_t) s];
                backbone.on = true;
                backbone.velocity = 1.0f;

                auto& rolled = exportPattern.rows[5][(size_t) s];
                rolled.on = true;
                rolled.velocity = 1.0f;
                rolled.probability = 0.5f;
            }
            auto& decorated = exportPattern.rows[1][4];
            decorated.on = true;
            decorated.velocity = 0.5f; // the other end of the round(velocity x 127) map
            decorated.flam = 1;
            decorated.ratchet = 2;
            for (int s = 1; s < 16; s += 4) // the swung side of the grid, so mute is proved off-grid
            {
                auto& quieted = exportPattern.rows[3][(size_t) s];
                quieted.on = true;
                quieted.velocity = 1.0f;
            }
            exportPattern.laneMute[3] = true;

            const double stepSamples = 6000.0;
            const double stepTicks = 240.0;
            const int pad0Note = 36, pad1Note = 37, pad3Note = 39, pad5Note = 41;

            // One bar of the render path, as absolute sample positions for one pad. The state is
            // seeded one pass short because the block that engages the transport is the block that
            // advances the counter: pass 2 is the first pass anybody hears. 188 blocks of 512 covers
            // the 16 steps (187.5 blocks) plus the look-ahead the scheduler runs past the end.
            const auto liveEvents = [&] (int64_t pass, int pad)
            {
                std::vector<int> positions;
                sliceforge::SeqPlaybackState state;
                state.passCount = pass - 1;
                juce::Random random (42);
                constexpr int block = 512;
                const double blockPpq = (double) block * (host.bpm / 60.0) / 48000.0;
                for (int b = 0; b < 188; ++b)
                {
                    auto passHost = host;
                    passHost.ppq = blockPpq * (double) b;
                    sliceforge::SeqBlockOutput out;
                    sliceforge::processSequencerBlock (*data, passHost, false, 48000.0, block,
                                                       random, state, out);
                    for (int i = 0; i < out.triggerCount; ++i)
                        if (out.triggers[(size_t) i].pad == pad)
                            positions.push_back (b * block + out.triggers[(size_t) i].sampleOffset);
                }
                return positions;
            };

            const auto liveSteps = [&] (int64_t pass, int pad)
            {
                juce::String list;
                for (const int position : liveEvents (pass, pad))
                    if (position >= 0 && position < 16 * 6000)
                        list << (int) std::llround ((double) position / stepSamples) << " ";
                return list;
            };

            // The same positions in the clip's currency. The live path quantises a hit to the block
            // that carries it, so a position can be a sample off the exact grid - never enough to move
            // a rounded tick, since one tick is 25 samples.
            const auto liveTicks = [&] (int64_t pass, int pad)
            {
                juce::String list;
                for (const int position : liveEvents (pass, pad))
                    if (position >= 0 && position < 16 * 6000)
                        list << (int) std::llround ((double) position / 25.0) << " ";
                return list;
            };

            struct ClipNote
            {
                int tick;
                int note;
                int channel;
                int velocity;
            };

            /*  Reads the clip back the way a host would. `createMatchingNoteOffs` is false on purpose:
                the gate has to judge what the exporter wrote, not the note-offs a parser invents to
                close the file. Events, not bytes - the sizes of two clips that hold different
                decisions are not comparable, and a re-encode inflates. */
            const auto readClip = [] (const juce::MemoryBlock& midi)
            {
                std::vector<ClipNote> notes;
                juce::MemoryInputStream stream (midi.getData(), midi.getSize(), false);
                juce::MidiFile file;
                if (file.readFrom (stream, false) && file.getNumTracks() > 0)
                    if (const auto* track = file.getTrack (0))
                        for (int i = 0; i < track->getNumEvents(); ++i)
                        {
                            const auto& message = track->getEventPointer (i)->message;
                            if (message.isNoteOn())
                                notes.push_back ({ (int) std::llround (message.getTimeStamp()),
                                                   message.getNoteNumber(),
                                                   message.getChannel(),
                                                   (int) message.getVelocity() });
                        }
                return notes;
            };

            const auto clipTicksFor = [&] (const std::vector<ClipNote>& notes, int note)
            {
                juce::String list;
                for (const auto& event : notes)
                    if (event.note == note)
                        list << event.tick << " ";
                return list;
            };

            const auto clipStepsFor = [&] (const std::vector<ClipNote>& notes, int note)
            {
                juce::String list;
                for (const auto& event : notes)
                    if (event.note == note)
                        list << (int) std::llround ((double) event.tick / stepTicks) << " ";
                return list;
            };

            const auto clipCountFor = [] (const std::vector<ClipNote>& notes, int note)
            {
                int seen = 0;
                for (const auto& event : notes)
                    if (event.note == note)
                        ++seen;
                return seen;
            };

            const auto clipNoteSet = [] (const std::vector<ClipNote>& notes)
            {
                std::vector<int> found;
                for (const auto& event : notes)
                    found.push_back (event.note);
                std::sort (found.begin(), found.end());
                found.erase (std::unique (found.begin(), found.end()), found.end());
                juce::String list;
                for (const int note : found)
                    list << note << " ";
                return list;
            };

            // The two fields a re-encode can quietly rewrite: the channel nibble and the velocity
            // byte. Both come back as sets, so a clip that mixed channels or folded two velocities
            // into one shows up in the list rather than in a count.
            const auto clipChannelSet = [] (const std::vector<ClipNote>& notes)
            {
                std::vector<int> found;
                for (const auto& event : notes)
                    found.push_back (event.channel);
                std::sort (found.begin(), found.end());
                found.erase (std::unique (found.begin(), found.end()), found.end());
                juce::String list;
                for (const int channel : found)
                    list << channel << " ";
                return list;
            };

            const auto clipVelocitySet = [&] (const std::vector<ClipNote>& notes, int note)
            {
                std::vector<int> found;
                for (const auto& event : notes)
                    if (event.note == note)
                        found.push_back (event.velocity);
                std::sort (found.begin(), found.end());
                found.erase (std::unique (found.begin(), found.end()), found.end());
                juce::String list;
                for (const int velocity : found)
                    list << velocity << " ";
                return list;
            };

            const auto clipText = [] (const juce::MemoryBlock& midi)
            {
                juce::String found;
                juce::MemoryInputStream stream (midi.getData(), midi.getSize(), false);
                juce::MidiFile file;
                if (file.readFrom (stream, false) && file.getNumTracks() > 0)
                    if (const auto* track = file.getTrack (0))
                        for (int i = 0; i < track->getNumEvents(); ++i)
                        {
                            const auto& message = track->getEventPointer (i)->message;
                            if (message.isTextMetaEvent())
                                found = message.getTextFromTextMetaEvent();
                        }
                return found;
            };

            const auto midi2 = sliceforge::makePatternMidiFile (*data, 2);
            const auto midi3 = sliceforge::makePatternMidiFile (*data, 3);
            const auto midiDefault = sliceforge::makePatternMidiFile (*data);
            const auto clip2 = readClip (midi2);
            const auto clip3 = readClip (midi3);
            const auto clipDefault = readClip (midiDefault);

            auto soloFixture = *data;
            soloFixture.patterns[0].laneSolo[0] = true;
            const auto clipSolo = readClip (sliceforge::makePatternMidiFile (soloFixture, 2));

            // Every list is built once and named: a check's detail argument is evaluated whether the
            // check passes or fails, so recomputing a 188-block render inside the detail string would
            // make the cost of a green run the same as the cost of a red one.
            const juce::String clip0Ticks = clipTicksFor (clip2, pad0Note);
            const juce::String clip1Ticks = clipTicksFor (clip2, pad1Note);
            const juce::String clip2Five = clipStepsFor (clip2, pad5Note);
            const juce::String clip3Five = clipStepsFor (clip3, pad5Note);
            const juce::String clipDefaultFive = clipStepsFor (clipDefault, pad5Note);
            const juce::String live2Five = liveSteps (2, 5);
            const juce::String live3Five = liveSteps (3, 5);
            const juce::String live0Ticks = liveTicks (2, 0);
            const int mutedLaneClipNotes = clipCountFor (clip2, pad3Note);
            const int mutedLaneLiveEvents = (int) liveEvents (2, 3).size ();
            const int rolledLaneClipNotes = clipCountFor (clip2, pad5Note);
            const juce::String soloLaneNotes = clipNoteSet (clipSolo);
            const juce::String clipChannels = clipChannelSet (clip2);
            const juce::String clip0Velocities = clipVelocitySet (clip2, pad0Note);
            const juce::String clip1Velocities = clipVelocitySet (clip2, pad1Note);
            const juce::String defaultClipText = clipText (midiDefault);

            // The swung grid, as the file's own note-on ticks: an even step on s*240, an odd step
            // 72 ticks (0.3 of a step) later. The literal is the fixture's arithmetic, and the render
            // path agrees with it independently - live0Ticks, printed in the onsets check below, came
            // out of the scheduler's own block-by-block positions.
            check (clip0Ticks == "0 312 480 792 960 1272 1440 1752 1920 2232 2400 2712 2880 3192 3360 3672 ",
                   "the exported clip swings the odd steps",
                   "clip 36=[" + clip0Ticks + "]");

            check (mutedLaneClipNotes == 0 && mutedLaneLiveEvents == 0,
                   "a muted lane reaches the clip and is silent in the render",
                   "clip notes=" + juce::String (mutedLaneClipNotes)
                       + " live events=" + juce::String (mutedLaneLiveEvents));

            // Grace 960 - 0.12 * 240 = 931.2 -> 931, main 960, ratchet 960 + 240 / 2 = 1080. The old
            // exporter already wrote flams and ratchets, so this was green before the change: it is a
            // characterisation of what the change must not break, falsified by deleting the two loops.
            check (clip1Ticks == "931 960 1080 ",
                   "flam and ratchet survive into the exported clip",
                   "clip 37=[" + clip1Ticks + "]");

            // Both pass literals are the render path's own fired-step lists for the fixture above -
            // live2Five and live3Five, measured - not a coin flip and not probabilityRollFor derived a
            // second time. Pinned as literals so the differential below cannot be satisfied by two
            // empty lists.
            check (clip2Five == "2 3 4 5 7 10 12 14 15 ",
                   "the clip's probability lane is pass 2's draw",
                   "clip 41=[" + clip2Five + "] live=[" + live2Five + "]");

            // The differential, both ways round, for two passes: which steps sound has one answer.
            // The last two conjuncts keep it honest - an export that dropped every rolled step would
            // agree with a render that dropped every rolled step, and a lane that never drops anything
            // would agree with itself at full length.
            check (live2Five == clip2Five && live3Five == clip3Five
                       && ! clip2Five.isEmpty() && rolledLaneClipNotes < 16,
                   "the render path and the clip fire the same steps of the probability lane",
                   "live2=[" + live2Five + "] clip2=[" + clip2Five
                       + "] live3=[" + live3Five + "] clip3=[" + clip3Five + "]");

            check (live0Ticks == clip0Ticks,
                   "the clip's onsets are the onsets the scheduler picked",
                   "live=[" + live0Ticks + "] clip=[" + clip0Ticks + "]");

            check (clip3Five == "0 1 2 5 6 7 8 10 11 13 15 "
                       && clip2Five != clip3Five
                       && clipDefaultFive == clip2Five,
                   "the printed clip is a named pass that re-rolls on request",
                   "pass2=[" + clip2Five + "] pass3=[" + clip3Five
                       + "] default=[" + clipDefaultFive + "]");

            check (soloLaneNotes == "36 ",
                   "a soloed lane is the only lane the clip prints",
                   "notes=[" + soloLaneNotes + "]");

            // The rest of the event fields a host actually reads, taken out of the file rather than
            // assumed from the code that wrote it: one channel for everything, and both of this
            // fixture's velocities at both ends of the map. The velocity is read with getVelocity(),
            // which in JUCE is the raw 0..127 byte - getFloatVelocity() is the 0..1 one, and scaling
            // the byte by 127 again is what first made this check print 16129.
            //
            // The pad-1 list is all three of its hits at 64, which is the exporter's map and NOT what
            // the render path plays: the scheduler dims its grace to 0.62 and its ratchet to 0.72 of
            // the lane velocity. That asymmetry is the residual Step 27 leaves open, written up in
            // docs/next-steps-plan.md, and pinning what the file really contains is what makes a later
            // change to either side show up here.
            check (clipChannels == "10 ",
                   "every note of the exported clip is on the drum channel",
                   "channels=[" + clipChannels + "]");

            check (clip0Velocities == "127 " && clip1Velocities == "64 ",
                   "the clip's velocity byte is the exporter's round(velocity x 127) map",
                   "36=[" + clip0Velocities + "] 37=[" + clip1Velocities + "]");

            check (defaultClipText.containsIgnoreCase ("pass 2"),
                   "the clip says which pass it is a snapshot of",
                   "text=[" + defaultClipText + "]");
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

        // Every comparison fails against NaN, so the old clamp let a non-finite sample walk
        // straight out of the plugin — and a poisoned filter state keeps producing it, which is
        // why the last stage has to be the one that stops it.
        const auto nan = std::numeric_limits<float>::quiet_NaN();
        const auto inf = std::numeric_limits<float>::infinity();
        check (sliceforge::OutputStage::clampToCeiling (nan) == 0.0f
                   && sliceforge::OutputStage::clampToCeiling (inf) == sliceforge::OutputStage::kCeiling
                   && sliceforge::OutputStage::clampToCeiling (-inf) == -sliceforge::OutputStage::kCeiling,
               "the safety ceiling rejects non-finite samples instead of passing them");
        trimmed.clear();
        trimmed.setSample (0, 0, nan);
        trimmed.setSample (0, 1, inf);
        stage.process (trimmed, 4);
        check (trimmed.getSample (0, 0) == 0.0f
                   && trimmed.getSample (0, 1) == sliceforge::OutputStage::kCeiling,
               "a poisoned block leaves the stage finite", juce::String (trimmed.getSample (0, 1), 4));

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

    // A reload with bytes it cannot use, and a block bigger than the one the engine was prepared
    // for. Both used to fail quietly: the first left the engine on one sample with the slice map
    // cleared and the header naming another, the second wrote past buffers sized smaller.
    {
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        auto tree = juce::ValueTree::readFromData (saved.getData(), saved.getSize());
        check (tree.isValid(), "the live session state reads back as a value tree");
        if (tree.isValid())
        {
            auto extra = tree.getChildWithName ("VRAC_EXTRA");
            extra.setProperty ("sourceIsFactory", false, nullptr);
            extra.setProperty ("sourceId", "custom-1700000000000", nullptr);
            extra.setProperty ("sourceName", "MY TAKE", nullptr);
            extra.setProperty ("customPcm", juce::MemoryBlock (1, true), nullptr);
            extra.setProperty ("sequencer", juce::MemoryBlock (64, true), nullptr);
            juce::MemoryBlock broken;
            juce::MemoryOutputStream brokenStream (broken, false);
            tree.writeToStream (brokenStream);

            SliceForgeAudioProcessor reloaded;
            reloaded.prepareToPlay (48000.0, 512);
            reloaded.setStateInformation (broken.getData(), (int) broken.getSize());
            const auto notice = reloaded.getStateNotice();
            check (notice.containsIgnoreCase ("RECORDING") && notice.containsIgnoreCase ("PATTERNS"),
                   "an unreadable take and an unreadable pattern blob are both reported", notice);
            const auto loaded = reloaded.getSamplePtr();
            check (loaded != nullptr && reloaded.getSourceId() == "foley"
                       && reloaded.sourceIsFactorySample() && reloaded.getSourceName() == loaded->name,
                   "the reloaded identity names the sample that is actually loaded",
                   reloaded.getSourceId() + " / " + reloaded.getSourceName());
            check (! reloaded.getSlices().empty(), "the reloaded source is sliced, not left with an empty map");
            const auto reloadedSequence = reloaded.getSequencerData();
            check (reloadedSequence != nullptr && reloadedSequence->patterns[0].rows[0][0].on,
                   "a rejected pattern blob restores defaults instead of an empty grid");
            const auto reloadedHit = render (reloaded, 20, 1.0f, reloaded.getSelectedPad());
            check (reloadedHit.finite && reloadedHit.total > 0.001f, "the reloaded session plays");

            // The dead bytes must not survive into the next save: a state that claims a custom
            // source it cannot decode is how the take got lost in the first place.
            juce::MemoryBlock resaved;
            reloaded.getStateInformation (resaved);
            auto resavedExtra = juce::ValueTree::readFromData (resaved.getData(), resaved.getSize())
                                    .getChildWithName ("VRAC_EXTRA");
            check (resavedExtra.isValid() && ! resavedExtra.hasProperty ("customPcm"),
                   "the abandoned take is not written back into the session state");
            SliceForgeAudioProcessor again;
            again.prepareToPlay (48000.0, 512);
            again.setStateInformation (resaved.getData(), (int) resaved.getSize());
            check (again.getStateNotice().isEmpty(), "a state the plugin wrote itself reloads clean",
                   again.getStateNotice());
            again.releaseResources();
            reloaded.releaseResources();
        }

        SliceForgeAudioProcessor wide;
        wide.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> oversized (juce::jmax (2, wide.getTotalNumOutputChannels()), 2048);
        juce::MidiBuffer wideMidi;
        wide.triggerPad (0, 1.0f);
        bool wideFinite = true;
        float widePeak = 0.0f;
        for (int block = 0; block < 4; ++block)
        {
            oversized.clear();
            wide.processBlock (oversized, wideMidi);
            for (int channel = 0; channel < oversized.getNumChannels(); ++channel)
                for (int i = 0; i < oversized.getNumSamples(); ++i)
                {
                    const auto value = oversized.getSample (channel, i);
                    if (! std::isfinite (value))
                        wideFinite = false;
                    widePeak = juce::jmax (widePeak, std::abs (value));
                }
        }
        check (wideFinite && widePeak <= sliceforge::OutputStage::kCeiling,
               "a block bigger than the prepared size stays finite and inside the ceiling",
               juce::String (widePeak, 4));
        wide.releaseResources();
    }

    // Parameter units: a knob whose value is a percentage has to say so, and a parameter added to
    // the layout without a formatting branch must fail here rather than read "0.50" on screen.
    {
        check (sliceforge::ui::formatParamValue ("global.drumify", 100.0) == "100%",
               "global drumify reads as a percent", sliceforge::ui::formatParamValue ("global.drumify", 100.0));
        check (sliceforge::ui::formatParamValue ("p01_drumify", 0.72) == "72%",
               "pad drumify reads as a percent", sliceforge::ui::formatParamValue ("p01_drumify", 0.72));
        // A sweep proves a branch exists; these prove it is the right unit. Global tone is a tilt
        // amount, pad tone moves a filter centre by semitones, and both used to print "4.00".
        check (sliceforge::ui::formatParamValue ("global.tone", 4.0) == "+4.0"
                   && sliceforge::ui::formatParamValue ("global.tone", -0.02) == "0.0",
               "global tone reads as a signed tilt", sliceforge::ui::formatParamValue ("global.tone", 4.0));
        check (sliceforge::ui::formatParamValue ("p01_tone", 3.0).containsIgnoreCase ("st"),
               "pad tone keeps its semitone unit", sliceforge::ui::formatParamValue ("p01_tone", 3.0));
        juce::String unknown;
        const auto& parameterState = processor.getValueTreeState().state;
        for (int i = 0; i < parameterState.getNumChildren(); ++i)
        {
            const auto id = parameterState.getChild (i).getProperty ("id").toString();
            if (id.isNotEmpty() && ! sliceforge::ui::isKnownParamId (id))
                unknown = unknown + id + " ";
        }
        check (unknown.isEmpty(), "every exposed parameter id has a formatting branch", unknown);
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

    // CPU cost. Reported, not asserted: the number depends on what else is running, so a bound
    // here fails on a loaded machine rather than on an engine that got slower. Real-time safety is
    // the finite-and-in-ceiling checks above; this line is for spotting a regression by eye.
    {
        processor.chokeAll();
        render (processor, 8, 1.0f, 0, -1, 999);
        const auto start = juce::Time::getHighResolutionTicks();
        for (int i = 0; i < 200; ++i)
            render (processor, 1, 1.0f, 0);
        const auto seconds = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        std::cout << "  info  CPU: " << juce::String (seconds * 100.0, 2) << "% of one core for 2.1s audio\n";
    }

    std::cout << "\n" << (failures == 0 ? "PASS" : "FAIL") << " - " << (checks - failures) << "/" << checks
              << " checks passed\n";
    return failures == 0 ? 0 : 1;
}

