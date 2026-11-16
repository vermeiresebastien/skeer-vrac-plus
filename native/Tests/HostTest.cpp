#include <JuceHeader.h>

#include <iostream>

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
