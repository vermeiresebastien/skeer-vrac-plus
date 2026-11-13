#include <JuceHeader.h>

#include <iostream>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    auto bundle = juce::File::getCurrentWorkingDirectory()
                      .getChildFile ("native").getChildFile ("build")
                      .getChildFile ("VRACPlus_artefacts").getChildFile ("Release")
                      .getChildFile ("VST3").getChildFile ("VRAC+.vst3");
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
    std::cout << "  parameters: " << instance->getParameters().size() << '\n';
    std::cout << "  buses: " << instance->getBusCount (true) << " in / " << instance->getBusCount (false)
              << " out\n";

    if (instance->getParameters().size() < 300)
    {
        std::cout << "FAIL expected the full automatable parameter set\n";
        return 1;
    }

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
