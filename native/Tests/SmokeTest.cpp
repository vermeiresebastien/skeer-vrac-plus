#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VaultLibrary.h"

#include <array>
#include <cmath>
#include <iostream>

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

    void setParam (VRACPlusAudioProcessor& processor, const juce::String& id, float value)
    {
        if (auto* parameter = processor.getValueTreeState().getParameter (id))
        {
            const auto& range = parameter->getNormalisableRange();
            parameter->setValueNotifyingHost (range.convertTo0to1 (value));
        }
    }

    float getParam (VRACPlusAudioProcessor& processor, const juce::String& id)
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
        std::array<float, vrac::kNumBuses> peaks {};
        std::array<std::array<float, 2>, vrac::kNumBuses> channelPeaks {};
        float total = 0.0f;
        bool finite = true;
    };

    Rendered render (VRACPlusAudioProcessor& processor, int blocks, float velocity = 1.0f, int pad = 0,
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
            for (int bus = 0; bus < vrac::kNumBuses; ++bus)
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
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "VRAC+ native smoke test\n";

    VRACPlusAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    check (processor.getSlices().size() >= 8, "factory source is sliced",
           juce::String (processor.getSlices().size()) + " slices");
    check (processor.getSamplePtr() != nullptr, "factory sample is loaded");
    check (processor.getPadStart (3) > 0.0, "pad starts are assigned");
    check (processor.getNumPrograms() == 6, "six factory programs");

    auto silent = render (processor, 4, 1.0f, 0, -1, 1000);
    check (silent.total == 0.0f, "engine is silent before a trigger");

    // One pad, one bus.
    setParam (processor, vrac::padParamId (0, "bus"), 1.0f);
    auto hit = render (processor, 20, 1.0f, 0);
    check (hit.finite, "pad output is finite");
    check (hit.peaks[0] > 0.005f, "pad hit is audible on output 1", juce::String (hit.peaks[0], 4));
    check (hit.peaks[1] < 0.0001f, "pad does not leak to output 2");

    // Pan.
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    setParam (processor, vrac::padParamId (0, "pan"), -1.0f);
    auto panned = render (processor, 20, 1.0f, 0);
    check (panned.channelPeaks[0][0] > panned.channelPeaks[0][1] * 3.0f, "pad pan works",
           juce::String (panned.channelPeaks[0][0], 4) + " / " + juce::String (panned.channelPeaks[0][1], 4));
    setParam (processor, vrac::padParamId (0, "pan"), 0.0f);

    // All eight buses.
    bool allBuses = true;
    for (int bus = 1; bus <= vrac::kNumBuses; ++bus)
    {
        processor.chokeAll();
        render (processor, 6, 1.0f, 0, -1, 999);
        setParam (processor, vrac::padParamId (0, "bus"), (float) bus);
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
    setParam (processor, vrac::padParamId (0, "bus"), 1.0f);
    auto soft = render (processor, 20, 0.2f, 0);
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    auto loud = render (processor, 20, 1.0f, 0);
    check (loud.total > soft.total * 1.5f, "velocity changes level",
           juce::String (soft.total, 4) + " -> " + juce::String (loud.total, 4));

    // Mute and solo.
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    setParam (processor, vrac::padParamId (0, "mute"), 1.0f);
    auto muted = render (processor, 10, 1.0f, 0);
    check (muted.total < 0.0001f, "muted pad is silent");
    setParam (processor, vrac::padParamId (0, "mute"), 0.0f);

    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    setParam (processor, vrac::padParamId (2, "solo"), 1.0f);
    auto soloBlocked = render (processor, 10, 1.0f, 0);
    check (soloBlocked.total < 0.0001f, "solo silences other pads");
    auto soloAudible = render (processor, 20, 1.0f, 2);
    check (soloAudible.total > 0.002f, "soloed pad is audible");
    setParam (processor, vrac::padParamId (2, "solo"), 0.0f);

    // MIDI note triggering.
    processor.chokeAll();
    render (processor, 8, 1.0f, 0, -1, 999);
    auto midiNote = render (processor, 20, 0.9f, 0, vrac::kPadMidiBase + 1, 0);
    check (midiNote.total > 0.002f, "MIDI note triggers a pad", juce::String (midiNote.total, 4));

    // Every model.
    bool allModelsAudible = true;
    for (int model = 0; model < vrac::kNumModels; ++model)
    {
        processor.chokeAll();
        render (processor, 6, 1.0f, 0, -1, 999);
        setParam (processor, vrac::padParamId (0, "model"), (float) model);
        setParam (processor, vrac::padParamId (0, "drumify"), 1.0f);
        setParam (processor, vrac::padParamId (0, "bus"), 1.0f);
        setParam (processor, vrac::padParamId (0, "length"), 0.22f);
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
    processor.armMidiLearn (vrac::padParamId (0, "pitch"));
    {
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 127), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    }
    check (processor.getMidiMappingForCc (74) == vrac::padParamId (0, "pitch"), "MIDI CC learn captures a parameter");
    {
        setParam (processor, vrac::padParamId (0, "pitch"), 0.0f);
        juce::AudioBuffer<float> buffer (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 0), 0);
        buffer.clear();
        processor.processBlock (buffer, midi);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    }
    check (getParam (processor, vrac::padParamId (0, "pitch")) < -10.0f, "learned MIDI CC moves the parameter");

    // Step sequencer.
    {
        auto sequence = vrac::cloneSequencerData (*processor.getSequencerData());
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
        auto changed = vrac::cloneSequencerData (*processor.getSequencerData());
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
        auto undoEdit = vrac::cloneSequencerData (*processor.getSequencerData());
        undoEdit->patterns[(size_t) undoEdit->activePattern].rows[2][3].on = ! beforeSeqUndo;
        processor.setSequencerData (undoEdit);
        processor.undo();
        const auto undone = processor.getSequencerData();
        check (undone != nullptr
                   && undone->patterns[(size_t) undone->activePattern].rows[2][3].on == beforeSeqUndo,
               "sequencer edits participate in undo");

        vrac::SeqPattern randomized;
        std::array<int, vrac::kNumPads> families {};
        families.fill (2);
        juce::Random random (1234);
        vrac::randomizePattern (randomized, 16, families, 0.5f, random);
        int randomisedSteps = 0;
        for (const auto& row : randomized.rows)
            for (int step = 0; step < 16; ++step)
                randomisedSteps += row[(size_t) step].on ? 1 : 0;
        check (randomisedSteps > 8, "step-randomizer creates a pattern");

        vrac::euclideanLane (randomized, 0, 16, 5, 0, 0.8f);
        int euclidSteps = 0;
        for (int step = 0; step < 16; ++step)
            euclidSteps += randomized.rows[0][(size_t) step].on ? 1 : 0;
        check (euclidSteps == 5, "Euclidean lane generator produces the requested pulses");
    }

    // Host-synchronised transport, using a mock playhead.
    {
        MockHostPlayHead playHead;
        VRACPlusAudioProcessor hostProcessor;
        hostProcessor.setPlayHead (&playHead);
        hostProcessor.prepareToPlay (48000.0, 512);
        auto sequence = vrac::cloneSequencerData (*hostProcessor.getSequencerData());
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
        VRACPlusAudioProcessor lockedProcessor;
        lockedProcessor.setPlayHead (&playHead);
        lockedProcessor.prepareToPlay (48000.0, 512);
        auto sequence = vrac::cloneSequencerData (*lockedProcessor.getSequencerData());
        sequence->hostSync = true;
        vrac::clearPattern (sequence->patterns[0]);
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
            auto data = std::make_shared<vrac::SequencerData>();
            data->length = 16;
            data->division = 4;
            data->bpm = 120.0;
            data->hostSync = true;
            data->songMode = false;
            data->chainLength = vrac::kSeqChainLength;
            data->chainPatterns = { 0, 1, 0, 2, 0, 1, 3, 2 };
            return data;
        };

        vrac::SeqHostInfo host;
        host.valid = true;
        host.playing = true;
        host.bpm = 120.0;

        {
            auto data = makeFeatureSequence();
            auto& step = data->patterns[0].rows[0][0];
            step.on = true;
            step.velocity = 1.0f;
            step.nudge = 0.5f;
            vrac::SeqPlaybackState featureState;
            vrac::SeqBlockOutput out;
            juce::Random random (7);
            host.ppq = 0.0;
            vrac::processSequencerBlock (*data, host, false, 48000.0, 7000, random, featureState, out);
            check (out.triggerCount == 1 && out.triggers[0].sampleOffset > 2500,
                   "step nudge shifts trigger timing", juce::String (out.triggerCount));
        }

        {
            auto data = makeFeatureSequence();
            auto& step = data->patterns[0].rows[0][0];
            step.on = true;
            step.velocity = 1.0f;
            step.nudge = 0.5f;
            step.flam = 1;
            vrac::SeqPlaybackState featureState;
            vrac::SeqBlockOutput out;
            juce::Random random (8);
            host.ppq = 0.0;
            vrac::processSequencerBlock (*data, host, false, 48000.0, 7000, random, featureState, out);
            check (out.triggerCount == 2, "flam generates a grace hit",
                   juce::String (out.triggerCount));
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
            vrac::SeqPlaybackState featureState;
            vrac::SeqBlockOutput firstOut, secondOut;
            juce::Random random (12);
            host.ppq = 0.37;
            vrac::processSequencerBlock (*data, host, false, 48000.0, 720, random, featureState, firstOut);
            host.ppq = 0.40;
            vrac::processSequencerBlock (*data, host, false, 48000.0, 720, random, featureState, secondOut);
            check (firstOut.triggerCount + secondOut.triggerCount >= 2,
                   "micro-timed steps survive out-of-order block boundaries");
        }

        {
            auto data = makeFeatureSequence();
            data->patterns[0].laneLength[0] = 4;
            data->patterns[0].rows[0][0].on = true;
            data->patterns[0].rows[0][0].velocity = 1.0f;
            vrac::SeqPlaybackState featureState;
            vrac::SeqBlockOutput out;
            juce::Random random (9);
            host.ppq = 1.0; // absolute step 4, wrapped back to lane step 0
            vrac::processSequencerBlock (*data, host, false, 48000.0, 512, random, featureState, out);
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
            vrac::SeqPlaybackState featureState;
            vrac::SeqBlockOutput out;
            juce::Random random (10);
            host.ppq = 1.0; // absolute step 4 -> chain slot B
            vrac::processSequencerBlock (*data, host, false, 48000.0, 512, random, featureState, out);
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
            vrac::SeqPlaybackState featureState;
            vrac::SeqBlockOutput out;
            juce::Random random (11);
            host.ppq = 0.0;
            vrac::processSequencerBlock (*data, host, false, 48000.0, 512, random, featureState, out);
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
            const auto block = vrac::serialiseSequencer (*data);
            const auto restored = vrac::deserialiseSequencer (block);
            check (restored != nullptr
                       && restored->patterns[0].laneLength[0] == 12
                       && std::abs (restored->patterns[0].rows[0][0].nudge - 0.25f) < 0.01f
                       && restored->patterns[0].rows[0][0].flam == 2
                       && restored->patterns[0].rows[0][0].locks.enabled,
                   "extended step/pattern data survives state serialization");
        }

        {
            const auto afro = vrac::makePatternPreset (vrac::PatternPreset::afro);
            check (afro.laneLength[6] == 12 && afro.rows[0][0].on,
                   "factory pattern preset contains polymeter and notes");
            auto data = makeFeatureSequence();
            data->patterns[0] = afro;
            const auto block = vrac::serialisePattern (afro);
            const auto restored = vrac::deserialisePattern (block);
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
            const auto midi = vrac::makePatternMidiFile (*data);
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
        vrac::VaultLibrary vault;
        const bool available = vault.refreshStatus();
        if (available)
        {
            vrac::VaultQuery query;
            query.limit = 12;
            const auto tracks = vault.queryTracks (query);
            check (vault.getStatus().totalTracks > 0 && ! tracks.empty()
                       && tracks.front().path.isNotEmpty(),
                   "SKEER Sample Vault index is readable");

            juce::File loadable;
            for (const auto& track : tracks)
            {
                const juce::File candidate (track.path);
                if (candidate.existsAsFile() && candidate.hasFileExtension ("wav"))
                {
                    loadable = candidate;
                    break;
                }
            }
            if (loadable.existsAsFile())
            {
                VRACPlusAudioProcessor vaultProcessor;
                vaultProcessor.prepareToPlay (48000.0, 512);
                check (vaultProcessor.loadUserSampleFromFile (loadable),
                       "a Sample Vault track loads into VRAC+");
            }
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
        VRACPlusAudioProcessor recorder;
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add (juce::AudioChannelSet::stereo());
        for (int i = 0; i < vrac::kNumBuses; ++i)
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
        VRACPlusAudioProcessor restored;
        restored.setStateInformation (customState.getData(), (int) customState.getSize());
        check (restored.getSourceName() == "RECORDED INPUT", "custom sample survives state round trip");
        check (restored.getSamplePtr() != nullptr && restored.getSamplePtr()->getDurationSeconds() > 0.15,
               "custom sample audio survives state round trip");
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

