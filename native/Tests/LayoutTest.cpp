#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/SequencerPanel.h"
#include "UI/SourcePanel.h"
#include "UI/PadEditorPanel.h"
#include "UI/VaultPanel.h"
#include "UI/Theme.h"

#include <cmath>
#include <iostream>

namespace
{
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
}

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

    /** The mirror of sliceforge::ui::isPlacedOnScreen that says which link failed, because a bare
        "0 of 16 buttons" does not tell you whether the panel, the page or the whole editor is the
        one that stopped being visible. */
    juce::String firstUnplaced (const juce::Component& component)
    {
        for (const juce::Component* c = &component; c != nullptr; c = c->getParentComponent())
            if (! c->isVisible() || c->getWidth() <= 0 || c->getHeight() <= 0)
                return juce::String (typeid (*c).name()) + " " + c->getBounds().toString()
                     + (c->isVisible() ? " empty" : " hidden");
        return "placed";
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    sliceforge::ui::setThemeMode (sliceforge::ui::ThemeMode::light, false);
    SliceForgeAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (processor.createEditor());
    // A host or the standalone puts the editor in a window, which makes it visible; this harness
    // has no wrapper, and every placement check below walks the parent chain up to the root.
    editor->setVisible (true);
    editor->setSize (1260, 840);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (80);

    // Check that every child is inside the editor bounds. Failures accumulate: the first
    // out-of-bounds control used to end the run, so a second, unrelated violation could not be
    // seen until the first was fixed, and a whole page could be broken behind one early exit.
    std::function<void (juce::Component&, const char*)> checkChildren = [&] (juce::Component& parent,
                                                                            const char* lane)
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
                check (false, juce::String ("every child has a size (") + lane + ")",
                       juce::String ("child ") + juce::String (i) + " in "
                           + juce::String (typeid (parent).name()) + " parentBounds="
                           + parent.getLocalBounds().toString() + " child="
                           + (child != nullptr ? juce::String (typeid (*child).name()) : "null")
                           + " bounds=" + (child != nullptr ? child->getBounds().toString()
                                                            : juce::String ("null")));
                continue;
            }
            // The editor is one uniformly scaled copy of the base layout: the content component
            // keeps its 1260x840 local box and is shrunk with a transform, so a local-space
            // comparison calls a correctly scaled child an overflow. Compare where the pixels land
            // instead, with a pixel of slack for the float scale factor itself.
            const auto parentBox = parent.getScreenBounds().expanded (1, 1).toFloat();
            const auto childBox = child->getScreenBounds().toFloat();
            if (! parentBox.contains (childBox))
            {
                check (false, juce::String ("every child sits inside its parent (") + lane + ")",
                       "child " + child->getBounds().toString() + " mapped to " + childBox.toString()
                           + " parent=" + juce::String (typeid (parent).name()) + " box="
                           + parentBox.toString() + " local=" + parent.getLocalBounds().toString()
                           + " childType=" + juce::String (typeid (*child).name())
                           + " id=" + child->getComponentID());
                continue;
            }
            checkChildren (*child, lane);
        }
    };
    checkChildren (*editor, "kit at 1260x840");

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

    auto* sliceforgeEditor = dynamic_cast<SliceForgeAudioProcessorEditor*> (editor.get());
    if (sliceforgeEditor == nullptr)
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
    sliceforgeEditor->showSequencerPage (true);
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

    checkChildren (*editor, "sequencer at 1260x840");

    // Step inspector: launch it through the same path as a right-click and
    // snapshot the page with the callout open.
    if (auto* sequencer = findComponentOfType<sliceforge::ui::SequencerPanel> (*editor))
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

    sliceforgeEditor->showVaultPage (true);
    editor->setSize (1260, 840);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (180);
    if (auto* vault = findComponentOfType<sliceforge::ui::VaultPanel> (*editor))
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
    sliceforgeEditor->setDarkThemeForTesting (true);
    sliceforgeEditor->showSequencerPage (false);
    editor->setSize (1260, 840);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    auto darkKit = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto darkKitFile = shots.getChildFile ("editor-dark.png");
    darkKitFile.deleteFile();
    juce::FileOutputStream darkKitStream (darkKitFile);
    if (! png.writeImageToStream (darkKit, darkKitStream))
        return 1;

    sliceforgeEditor->showSequencerPage (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    auto darkSequencer = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    auto darkSequencerFile = shots.getChildFile ("sequencer-dark.png");
    darkSequencerFile.deleteFile();
    juce::FileOutputStream darkSequencerStream (darkSequencerFile);
    if (! png.writeImageToStream (darkSequencer, darkSequencerStream))
        return 1;

    if (auto* sequencer = findComponentOfType<sliceforge::ui::SequencerPanel> (*editor))
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

    sliceforgeEditor->showVaultPage (true);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
    if (auto* vault = findComponentOfType<sliceforge::ui::VaultPanel> (*editor))
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

    // Behavioural gates, kept after every snapshot so a synthetic key press or a live
    // recording cannot leak into the screenshots.
    sliceforgeEditor->showVaultPage (false);
    sliceforgeEditor->showSequencerPage (false);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (40);

    const auto press = [sliceforgeEditor] (int code, juce::ModifierKeys mods)
    {
        return sliceforgeEditor->keyPressed (juce::KeyPress (code, mods, 0));
    };

    // The guard is only evidence if the editor does swallow a bare letter: without this
    // precondition, "Ctrl+Q falls through" would also pass on an editor that ignores keys.
    check (press ('q', juce::ModifierKeys::noModifiers), "a bare letter triggers a pad");
    check (press ('q', juce::ModifierKeys::shiftModifier), "shift keeps its accent role");
    check (press ('q', juce::ModifierKeys::altModifier), "alt keeps its ghost role");
    check (! press ('q', juce::ModifierKeys::ctrlModifier), "Ctrl+letter falls through to the host");
    check (! press ('a', juce::ModifierKeys::commandModifier), "Cmd+letter falls through to the host");
    check (! press (' ', juce::ModifierKeys::ctrlModifier), "Ctrl+Space falls through to the host");
    check (press (' ', juce::ModifierKeys::noModifiers), "Space still chokes the buses");
    check (! press ('y', juce::ModifierKeys::noModifiers), "an unmapped letter is not swallowed");

    // The smallest window the editor is allowed to open at, walked on every page. The content is
    // one uniformly scaled copy of the base layout, so the local-bound walk is size-invariant as
    // long as resized() stays a plain scale - which is itself the claim, and a fixed pixel layout
    // anywhere in a panel would break it. This is the size a laptop or a small DAW window actually
    // gets, and until now nothing here ever drew at it.
    {
        auto* constrainer = sliceforgeEditor->getConstrainer();
        check (constrainer != nullptr && constrainer->getMinimumWidth() == 1008
                   && constrainer->getMinimumHeight() == 672,
               "the editor opens as small as the README promises",
               constrainer != nullptr
                   ? juce::String (constrainer->getMinimumWidth()) + "x" + juce::String (constrainer->getMinimumHeight())
                   : juce::String ("no constrainer"));

        const auto sweepMinimum = [&] (const char* page)
        {
            sliceforgeEditor->setSize (1008, 672);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            const auto shot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
            check (shot.isValid() && shot.getWidth() > 0 && shot.getHeight() > 0,
                   juce::String ("the ") + page + " page renders at the minimum window size",
                   juce::String (shot.getWidth()) + "x" + juce::String (shot.getHeight()));
            checkChildren (*editor, page);
            editor->setSize (1260, 840);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        };

        sliceforgeEditor->showSequencerPage (false);
        sliceforgeEditor->showVaultPage (false);
        sweepMinimum ("kit");
        if (auto* pads = findComponentOfType<sliceforge::ui::PadEditorPanel> (*editor))
        {
            // The three rows a smaller panel is most likely to drop: sixteen models, eight outputs
            // and five choke groups. If a resize ever stops placing them, this reads zero.
            check (pads->visibleModelButtonsForTesting() == sliceforge::kNumModels
                       && pads->visibleBusButtonsForTesting() == sliceforge::kNumBuses
                       && pads->visibleChokeButtonsForTesting() == 5,
                   "the pad editor shows every model, output and choke control",
                   juce::String (pads->visibleModelButtonsForTesting()) + "/"
                       + juce::String (pads->visibleBusButtonsForTesting()) + "/"
                       + juce::String (pads->visibleChokeButtonsForTesting())
                       + " panel=" + firstUnplaced (*pads));
        }
        else
        {
            check (false, "the pad editor shows every model, output and choke control", "no PadEditorPanel");
        }

        sliceforgeEditor->showSequencerPage (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        sweepMinimum ("sequencer");
        if (auto* sequencer = findComponentOfType<sliceforge::ui::SequencerPanel> (*editor))
        {
            const auto& copyTo = sequencer->copyToControlForTesting();
            check (sliceforge::ui::isPlacedOnScreen (copyTo) && copyTo.isEnabled()
                       && copyTo.getButtonText() == "COPY TO",
                   "the pattern duplicate control is on screen and live",
                   copyTo.getBounds().toString() + (copyTo.isEnabled() ? " enabled" : " disabled")
                       + " chain=" + firstUnplaced (copyTo));
        }
        else
        {
            check (false, "the pattern duplicate control is on screen and live", "no SequencerPanel");
        }
        sliceforgeEditor->showSequencerPage (false);

        sliceforgeEditor->showVaultPage (true);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        sweepMinimum ("vault");
        if (auto* vault = findComponentOfType<sliceforge::ui::VaultPanel> (*editor))
        {
            const auto placed = [] (const juce::Component& c) { return sliceforge::ui::isPlacedOnScreen (c); };
            editor->setSize (1008, 672);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
            // PLAY/LOOP/STOP are only revealed once a row is selected (VaultPanel::updateDetails),
            // so their absence with nothing picked is the design, and the check states it: the row
            // that survives the smallest window is the search field, the results list and LOAD
            // SAMPLE, all of which are laid out before the database has answered at all.
            check (placed (vault->vaultSearchForTesting()) && placed (vault->vaultListForTesting())
                       && placed (vault->vaultLoadForTesting()) && ! placed (vault->vaultPreviewForTesting()),
                   "the vault lays out search, results and load with nothing selected",
                   vault->vaultListForTesting().getBounds().toString()
                       + " chain=" + firstUnplaced (vault->vaultListForTesting()));
            vault->searchForTesting ("KICK");
            vault->selectFirstAnalysedRowForTesting();
            juce::MessageManager::getInstance()->runDispatchLoopUntil (120);
            check (placed (vault->vaultPreviewForTesting()),
                   "a selection at the minimum size still reaches the transport",
                   vault->vaultPreviewForTesting().getBounds().toString()
                       + " chain=" + firstUnplaced (vault->vaultPreviewForTesting()));
            editor->setSize (1260, 840);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        else
        {
            check (false, "the vault lays out search, results and load with nothing selected", "no VaultPanel");
        }
        sliceforgeEditor->showVaultPage (false);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }

    // The sidechain input ships disabled, so a recorder that would capture silence has
    // to say so instead of arming.
    if (auto* source = findComponentOfType<sliceforge::ui::SourcePanel> (*editor))
    {
        juce::AudioProcessor::BusesLayout shipped;
        shipped.inputBuses.add (juce::AudioChannelSet::disabled());
        for (int i = 0; i < sliceforge::kNumBuses; ++i)
            shipped.outputBuses.add (juce::AudioChannelSet::stereo());
        processor.setBusesLayout (shipped);
        processor.prepareToPlay (48000.0, 512);
        source->refreshRecordingState();
        check (! processor.isSidechainInputActive(), "a closed sidechain bus is reported as inactive");
        check (! source->isRecordControlEnabledForTesting(),
               "REC is unavailable while the host input bus is closed",
               source->getRecordControlTextForTesting());
        check (source->getRecordControlTextForTesting() == "REC" && ! source->isDiscardControlVisibleForTesting(),
               "the record chip reads REC with no take pending");

        // Same control, now with the bus open: proves the check above is not a dead button.
        juce::AudioProcessor::BusesLayout opened;
        opened.inputBuses.add (juce::AudioChannelSet::stereo());
        for (int i = 0; i < sliceforge::kNumBuses; ++i)
            opened.outputBuses.add (juce::AudioChannelSet::stereo());
        processor.setBusesLayout (opened);
        processor.prepareToPlay (48000.0, 512);
        source->refreshRecordingState();
        check (processor.isSidechainInputActive(), "an open sidechain bus is reported as active");
        check (source->isRecordControlEnabledForTesting(),
               "REC becomes available once the host opens the input bus");

        processor.toggleRecording();
        source->refreshRecordingState();
        check (source->getRecordControlTextForTesting() == "STOP" && source->isRecordControlEnabledForTesting(),
               "REC reads STOP while armed");
        juce::AudioBuffer<float> block (juce::jmax (2, processor.getTotalNumOutputChannels()), 512);
        juce::MidiBuffer midi;
        for (int b = 0; b < 24; ++b)
        {
            block.clear();
            auto input = processor.getBusBuffer (block, true, 0);
            for (int channel = 0; channel < input.getNumChannels(); ++channel)
                for (int i = 0; i < input.getNumSamples(); ++i)
                    input.setSample (channel, i, 0.25f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                                                   * (float) (b * 512 + i) / 48000.0f));
            processor.processBlock (block, midi);
        }
        processor.toggleRecording();
        for (int b = 0; b < 3; ++b)
        {
            block.clear();
            processor.processBlock (block, midi);
        }
        source->refreshRecordingState();
        check (processor.isRecordingTakeReady(), "the stopped take is reported ready");
        check (source->getRecordControlTextForTesting() == "USE" && source->isDiscardControlVisibleForTesting(),
               "a ready take offers USE and DISCARD", source->getRecordControlTextForTesting());
        checkChildren (*editor, "record take chips"); // the DISCARD chip is only on screen in this state, so measure it here

        // USE is the only way a take becomes the source now. The UI timer used to adopt it on
        // its own about 330 ms after STOP, which replaced the kit with no confirmation.
        if (source->onToggleRecording)
            source->onToggleRecording(); // the same callback, take-ready branch
        source->refreshRecordingState();
        check (processor.getSourceName() == "RECORDED INPUT" && ! processor.isRecordingTakeReady()
                   && ! source->isDiscardControlVisibleForTesting()
                   && source->getRecordControlTextForTesting() == "REC",
               "USE adopts the take through the control that armed REC", processor.getSourceName());

        // Then a second take, this one dropped instead of adopted.
        if (source->onToggleRecording)
            source->onToggleRecording();
        check (processor.isRecording(), "the UI record control reaches the processor");
        for (int b = 0; b < 2; ++b)
        {
            block.clear();
            processor.processBlock (block, midi);
        }
        processor.toggleRecording();
        for (int b = 0; b < 3; ++b)
        {
            block.clear();
            processor.processBlock (block, midi);
        }
        source->refreshRecordingState();
        check (processor.isRecordingTakeReady(), "a take of near-silence is still offered back");
        if (source->onDiscardRecording)
            source->onDiscardRecording();
        source->refreshRecordingState();
        check (! processor.isRecordingTakeReady() && ! source->isDiscardControlVisibleForTesting()
                   && source->getRecordControlTextForTesting() == "REC",
               "DISCARD drops the take and restores the idle chip");
    }
    else
    {
        std::cout << "  FAIL  no SourcePanel in the editor\n";
        ++failures;
    }

    if (failures > 0)
    {
        std::cout << "FAIL layout checks: " << failures << " of " << checks << '\n';
        return 1;
    }

    std::cout << "PASS layout screenshot: " << file.getFullPathName() << " (" << checks << " checks)\n";
    return 0;
}
