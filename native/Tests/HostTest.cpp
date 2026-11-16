#include <JuceHeader.h>

#include <cstdint>
#include <iostream>

namespace
{
    /** Enough of a DAW transport for the plugin to sync to: sample clock, ppq, tempo, playing. */
    class MockHostPlayHead final : public juce::AudioPlayHead
    {
    public:
        bool playing = false;
        double ppq = 0.0;
        double bpm = 120.0;
        std::int64_t timeInSamples = 0;

        juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
        {
            juce::AudioPlayHead::PositionInfo position;
            position.setIsPlaying (playing);
            position.setPpqPosition (ppq);
            position.setBpm (bpm);
            position.setTimeInSamples (timeInSamples);
            return position;
        }
    };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    auto bundle = juce::File::getCurrentWorkingDirectory()
                      .getChildFile ("native").getChildFile ("build")
                      .getChildFile ("SliceForge_artefacts").getChildFile ("Release")
                      .getChildFile ("VST3").getChildFile ("SliceForge.vst3");
    if (argc > 1)
        bundle = juce::File (juce::String::fromUTF8 (argv[1]));

    std::cout << "VST3 host test: " << bundle.getFullPathName() << '\n';
    if (! bundle.exists())
    {
        std::cout << "FAIL bundle not found\n";
        return 1;
    }

    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();

    juce::AudioPluginFormat* vst3 = nullptr;
    for (int i = 0; i < formats.getNumFormats(); ++i)
    {
        auto* format = formats.getFormat (i);
        if (format->getName().containsIgnoreCase ("VST3"))
        {
            vst3 = format;
            break;
        }
    }
    if (vst3 == nullptr)
    {
        std::cout << "FAIL VST3 host format unavailable\n";
        return 1;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3->findAllTypesForFile (descriptions, bundle.getFullPathName());
    std::cout << "  found " << descriptions.size() << " plugin(s)\n";
    if (descriptions.isEmpty())
    {
        std::cout << "FAIL no plugin description\n";
        return 1;
    }

    juce::String error;
    auto instance = std::unique_ptr<juce::AudioPluginInstance> (
        vst3->createInstanceFromDescription (*descriptions[0], 48000.0, 512, error));
    if (instance == nullptr)
    {
        std::cout << "FAIL could not instantiate: " << error << '\n';
        return 1;
    }

    std::cout << "  name: " << instance->getName() << '\n';
    std::cout << "  buses: " << instance->getBusCount (true) << " in / " << instance->getBusCount (false)
              << " out\n";

    /*  Step 28: a host only routes an event bus it can see. `producesMidi()` on a hosted instance is
        NOT our own flag read back - it is derived from the VST3 bus enumeration of the binary on disk
        (juce_VST3PluginFormat.cpp:3508-3509 counts event-direction output buses, :3186 reports that
        count), so this line is the advertisement itself rather than a second copy of the CMake option.
        It is also why an existing session has to reload the plugin to get the MIDI bus: the bus list is
        read when the host scans the module, not when it restores the session. */
    if (! instance->producesMidi())
    {
        std::cout << "FAIL hosted plugin advertises no MIDI output bus (producesMidi() is false)\n";
        return 1;
    }
    if (! instance->acceptsMidi())
    {
        std::cout << "FAIL hosted plugin no longer accepts MIDI input\n";
        return 1;
    }

    // 348 parameters come from createParameterLayout (nine globals before the pad loop, 21 for each of
    // 16 pads, the two output-stage parameters after it and seq.pattern appended last), and JUCE's VST3
    // wrapper always adds Bypass and Program on top, so a host sees 350. Asserting the exact total is the
    // only thing that keeps the count in the READMEs from drifting: printing it and checking it against
    // 300 let 347, 349 and 345 all look like a pass.
    if (instance->getParameters().size() != 350)
    {
        std::cout << "FAIL expected 348 layout parameters plus the wrapper's Bypass and Program, got "
                  << instance->getParameters().size() << '\n';
        return 1;
    }
    // The total can be right while the parameter that changed it is absent, so the host's own view of
    // the pattern control is looked up by id. Measured here rather than assumed: a VST3 id is NOT the
    // APVTS string. The export hashes it (`juce_audio_plugin_client_VST3.cpp:659-674`,
    // `generateVSTParamIDForParam` -> `juceParamID.hashCode()`, because `JUCE_FORCE_USE_LEGACY_PARAM_IDS`
    // is off) and the host reads the number back (`juce_VST3PluginFormat.cpp:2579-2582`,
    // `String (cachedInfo.id)`), so "seq.pattern" surfaces as 825878785. The literal is pinned from what
    // this run printed instead of recomputed from the hash function, and the mapping is why P4 freezes
    // ids: renaming the parameter would point every host's saved automation at a different number.
    {
        const auto& parameters = instance->getParameters();
        int patternIndex = -1;
        for (int i = 0; i < parameters.size(); ++i)
            if (auto* hosted = dynamic_cast<juce::HostedAudioProcessorParameter*> (parameters[i]))
                if (hosted->getParameterID() == "825878785")
                    patternIndex = i;
        auto* patternParam = patternIndex >= 0 ? parameters[patternIndex] : nullptr;
        if (patternParam == nullptr || patternIndex != parameters.size() - 3
            || ! patternParam->isDiscrete() || patternParam->getNumSteps() != 4
            || patternParam->getDefaultValue() != 0.0f
            || ! patternParam->getName (128).containsIgnoreCase ("pattern"))
        {
            std::cout << "FAIL seq.pattern is not the appended four-step control a host must see: index "
                      << patternIndex << " of " << parameters.size() << ", steps "
                      << (patternParam != nullptr ? patternParam->getNumSteps() : -1) << ", default "
                      << (patternParam != nullptr ? patternParam->getDefaultValue() : -1.0f) << '\n';
            return 1;
        }
        std::cout << "  seq.pattern: host id 825878785, index " << patternIndex << ", "
                  << patternParam->getNumSteps() << " steps, default "
                  << patternParam->getDefaultValue() << ", named "
                  << patternParam->getName (128) << '\n';
    }
    std::cout << "  parameters: 350 = 348 layout + Bypass + Program\n";

    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::disabled());
    for (int i = 0; i < 8; ++i)
        layout.outputBuses.add (juce::AudioChannelSet::stereo());
    if (! instance->setBusesLayout (layout))
    {
        std::cout << "FAIL eight stereo outputs not accepted by host wrapper\n";
        return 1;
    }

    instance->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (juce::jmax (2, instance->getTotalNumOutputChannels()), 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);
    buffer.clear();
    instance->processBlock (buffer, midi);

    float peak = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        const auto* data = buffer.getReadPointer (channel);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (! std::isfinite (data[i]))
            {
                std::cout << "FAIL non-finite output\n";
                return 1;
            }
            peak = juce::jmax (peak, std::abs (data[i]));
        }
    }
    std::cout << "  first note peak: " << peak << '\n';
    if (peak <= 0.0001f)
    {
        std::cout << "FAIL host instance produced silence\n";
        return 1;
    }

    /*  Step 28, end to end. The smoke program proves the event list; only a hosted run can prove that
        the events survive the real VST3 event marshalling, because it goes through
        juce_audio_plugin_client_VST3.cpp:3621-3624 (leftover events out to data.outputEvents) and
        juce_VST3PluginFormat.cpp:2984-2985 (that list back into the host's MidiBuffer). Which steps
        the factory pattern happens to light is not this test's business, so the claims are the
        structural ones a hosted bus makes possible: events arrive, they pair, they are pad notes on
        the drum channel, they sit inside the block, and opening an event output bus did not cost the
        audio anything. */
    {
        MockHostPlayHead playHead;
        instance->setPlayHead (&playHead);
        playHead.playing = true;

        int noteOns = 0, noteOffs = 0, otherEvents = 0, offMap = 0, outsideBlock = 0;
        float midiRunPeak = 0.0f;
        for (int block = 0; block < 48; ++block)
        {
            playHead.ppq = (double) block * (512.0 * 120.0 / 60.0 / 48000.0);
            playHead.timeInSamples = static_cast<std::int64_t> (block) * 512;
            buffer.clear();
            juce::MidiBuffer hostedMidi;   // in: nothing; out: whatever the plugin leaves behind
            instance->processBlock (buffer, hostedMidi);

            for (const auto metadata : hostedMidi)
            {
                const auto message = metadata.getMessage();
                if (message.isNoteOn())
                    ++noteOns;
                else if (message.isNoteOff())
                    ++noteOffs;
                else
                    ++otherEvents;
                if (message.isNoteOn() || message.isNoteOff())
                {
                    if (message.getChannel() != 10
                        || message.getNoteNumber() < 36 || message.getNoteNumber() > 51)
                        ++offMap;
                    if (metadata.samplePosition < 0 || metadata.samplePosition >= 512)
                        ++outsideBlock;
                }
            }

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const auto sample = buffer.getSample (channel, i);
                    if (! std::isfinite (sample))
                    {
                        std::cout << "FAIL non-finite output during the MIDI-out run\n";
                        return 1;
                    }
                    midiRunPeak = juce::jmax (midiRunPeak, std::abs (sample));
                }
        }
        instance->setPlayHead (nullptr);

        std::cout << "  midi out: " << noteOns << " note-ons / " << noteOffs << " note-offs over 48 blocks, "
                  << "peak " << midiRunPeak << "\n";
        if (noteOns < 1 || noteOns != noteOffs || otherEvents != 0 || offMap != 0 || outsideBlock != 0
            || midiRunPeak <= 0.0001f)
        {
            std::cout << "FAIL sequencer MIDI out is not a clean pad-note stream: "
                      << noteOns << " on / " << noteOffs << " off, " << otherEvents << " non-note events, "
                      << offMap << " off channel 10 or outside 36..51, " << outsideBlock
                      << " outside the block, audio peak " << midiRunPeak << "\n";
            return 1;
        }
    }

    juce::MemoryBlock state;
    instance->getStateInformation (state);
    std::cout << "  state: " << state.getSize() << " bytes\n";
    if (state.getSize() < 1000)
    {
        std::cout << "FAIL state looks empty\n";
        return 1;
    }

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (instance->createEditorIfNeeded());
    if (editor == nullptr)
    {
        std::cout << "FAIL no editor from host instance\n";
        return 1;
    }
    editor->setSize (1260, 840);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    std::cout << "  editor: " << editor->getWidth() << " x " << editor->getHeight() << '\n';
    if (editor->getWidth() < 1000 || editor->getHeight() < 600)
    {
        std::cout << "FAIL host editor has an unexpected size\n";
        return 1;
    }

    instance->releaseResources();
    std::cout << "PASS VST3 host test\n";
    return 0;
}
