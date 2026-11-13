#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/SequencerPanel.h"
#include "UI/VaultPanel.h"
#include "UI/Theme.h"

#include <iostream>

namespace
{
    template <typename T>
    T* findComponentOfType (juce::Component& parent)
    {
        for (int i = 0; i < parent.getNumChildComponents(); ++i)
        {
            auto* child = parent.getChildComponent (i);
            if (child == nullptr)
                continue;
            if (auto* match = dynamic_cast<T*> (child))
                return match;
            if (auto* nested = findComponentOfType<T> (*child))
                return nested;
        }
        return nullptr;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    vrac::ui::setThemeMode (vrac::ui::ThemeMode::light, false);
    VRACPlusAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    editor->setSize (1260, 840);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);

    // Check that every child is inside the editor bounds.
    std::function<void (juce::Component&)> checkChildren = [&] (juce::Component& parent)
    {
        // TextEditor owns internal viewport/scrollbar children that JUCE
        // deliberately positions just outside the viewport bounds.
        if (dynamic_cast<juce::TextEditor*> (&parent) != nullptr
            || dynamic_cast<juce::ListBox*> (&parent) != nullptr)
            return;
        for (int i = 0; i < parent.getNumChildComponents(); ++i)
        {
            auto* child = parent.getChildComponent (i);
            if (child == nullptr || child->getWidth() <= 0 || child->getHeight() <= 0)
            {
                std::cout << "  FAIL  empty child at " << i << " in "
                          << typeid (parent).name() << " parentBounds="
                          << parent.getLocalBounds().toString()
                          << " child=" << (child != nullptr ? typeid (*child).name() : "null")
                          << " bounds=" << (child != nullptr ? child->getBounds().toString() : "null")
                          << '\n';
                std::exit (1);
            }
            if (! parent.getLocalBounds().contains (child->getBounds()))
            {
                std::cout << "  FAIL  child outside parent: " << child->getBounds().toString()
                          << " parent=" << typeid (parent).name()
                          << " parentBounds=" << parent.getLocalBounds().toString()
                          << " child=" << typeid (*child).name()
                          << " id=" << child->getComponentID() << '\n';
                std::exit (1);
            }
            checkChildren (*child);
        }
    };
    checkChildren (*editor);

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto shots = juce::File::getCurrentWorkingDirectory().getChildFile ("native").getChildFile ("shots");
    if (! shots.isDirectory())
        shots = juce::File::getCurrentWorkingDirectory().getChildFile ("shots");
    shots.createDirectory();
    auto file = shots.getChildFile ("editor-light.png");
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    if (! png.writeImageToStream (image, stream))
    {
        std::cout << "FAIL could not write screenshot\n";
        return 1;
    }

    auto* vracEditor = dynamic_cast<VRACPlusAudioProcessorEditor*> (editor.get());
    if (vracEditor == nullptr)
    {
        std::cout << "FAIL editor has unexpected type\n";
        return 1;
    }

    // Also prove the layout scales without clipping.
    editor->setSize (1680, 1120);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    auto scaled = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto scaledFile = shots.getChildFile ("editor-large.png");
    scaledFile.deleteFile();
    juce::FileOutputStream scaledStream (scaledFile);
    if (! png.writeImageToStream (scaled, scaledStream))
        return 1;

    // Sequencer page at base size and scaled size. Run a short internal
    // preview so the green playhead and transport state appear in the shots.
    vracEditor->showSequencerPage (true);
    editor->setSize (1260, 840);
    processor.setSequencerInternalPlaying (true);
    {
        juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        for (int i = 0; i < 24; ++i)
        {
            block.clear();
            processor.processBlock (block, midi);
        }
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (160);
    auto sequencerImage = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto sequencerFile = shots.getChildFile ("sequencer-light.png");
    sequencerFile.deleteFile();
    juce::FileOutputStream sequencerStream (sequencerFile);
    if (! png.writeImageToStream (sequencerImage, sequencerStream))
        return 1;

    checkChildren (*editor);

    // Step inspector: launch it through the same path as a right-click and
    // snapshot the page with the callout open.
    if (auto* sequencer = findComponentOfType<vrac::ui::SequencerPanel> (*editor))
    {
        sequencer->showStepInspectorForTesting (0, 0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        auto inspectorImage = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto inspectorFile = shots.getChildFile ("sequencer-inspector.png");
        inspectorFile.deleteFile();
        juce::FileOutputStream inspectorStream (inspectorFile);
        if (! png.writeImageToStream (inspectorImage, inspectorStream))
            return 1;
        if (auto* callout = findComponentOfType<juce::CallOutBox> (*editor))
        {
            callout->dismiss();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
        }
    }

    editor->setSize (1680, 1120);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    auto sequencerLarge = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto sequencerLargeFile = shots.getChildFile ("sequencer-large.png");
    sequencerLargeFile.deleteFile();
    juce::FileOutputStream sequencerLargeStream (sequencerLargeFile);
    if (! png.writeImageToStream (sequencerLarge, sequencerLargeStream))
        return 1;

    vracEditor->showVaultPage (true);
    editor->setSize (1260, 840);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (180);
    if (auto* vault = findComponentOfType<vrac::ui::VaultPanel> (*editor))
    {
        vault->searchForTesting ("KICK");
        vault->selectFirstAnalysedRowForTesting();
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    auto vaultImage = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto vaultFile = shots.getChildFile ("vault-light.png");
    vaultFile.deleteFile();
    juce::FileOutputStream vaultStream (vaultFile);
    if (! png.writeImageToStream (vaultImage, vaultStream))
        return 1;

    // Dark theme snapshots.
    vracEditor->setDarkThemeForTesting (true);
    vracEditor->showSequencerPage (false);
    editor->setSize (1260, 840);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    auto darkKit = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto darkKitFile = shots.getChildFile ("editor-dark.png");
    darkKitFile.deleteFile();
    juce::FileOutputStream darkKitStream (darkKitFile);
    if (! png.writeImageToStream (darkKit, darkKitStream))
        return 1;

    vracEditor->showSequencerPage (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    auto darkSequencer = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto darkSequencerFile = shots.getChildFile ("sequencer-dark.png");
    darkSequencerFile.deleteFile();
    juce::FileOutputStream darkSequencerStream (darkSequencerFile);
    if (! png.writeImageToStream (darkSequencer, darkSequencerStream))
        return 1;

    if (auto* sequencer = findComponentOfType<vrac::ui::SequencerPanel> (*editor))
    {
        sequencer->showStepInspectorForTesting (0, 0);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
        auto darkInspector = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto darkInspectorFile = shots.getChildFile ("sequencer-inspector-dark.png");
        darkInspectorFile.deleteFile();
        juce::FileOutputStream darkInspectorStream (darkInspectorFile);
        if (! png.writeImageToStream (darkInspector, darkInspectorStream))
            return 1;
        if (auto* callout = findComponentOfType<juce::CallOutBox> (*editor))
        {
            callout->dismiss();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
        }
    }

    vracEditor->showVaultPage (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    if (auto* vault = findComponentOfType<vrac::ui::VaultPanel> (*editor))
    {
        vault->searchForTesting ("KICK");
        vault->selectFirstAnalysedRowForTesting();
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);
    auto darkVault = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto darkVaultFile = shots.getChildFile ("vault-dark.png");
    darkVaultFile.deleteFile();
    juce::FileOutputStream darkVaultStream (darkVaultFile);
    if (! png.writeImageToStream (darkVault, darkVaultStream))
        return 1;

    std::cout << "PASS layout screenshot: " << file.getFullPathName() << '\n';
    return 0;
}
