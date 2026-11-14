#include "PluginEditor.h"
#include "UI/Theme.h"

#include <array>

namespace
{
    constexpr int kBaseWidth = 1260;
    constexpr int kBaseHeight = 840;
}

SliceForgeAudioProcessorEditor::SliceForgeAudioProcessorEditor (SliceForgeAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setOpaque (true);
    setLookAndFeel (&lookAndFeel);
    setResizable (true, true);
    setResizeLimits (1008, 672, 1890, 1260);
    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio ((float) kBaseWidth / (float) kBaseHeight);
    setWantsKeyboardFocus (true);

    content = std::make_unique<juce::Component>();
    content->setSize (kBaseWidth, kBaseHeight);
    addAndMakeVisible (*content);

    header = std::make_unique<sliceforge::ui::HeaderBar> (processorRef);
    sourcePanel = std::make_unique<sliceforge::ui::SourcePanel> (processorRef);
    padsPanel = std::make_unique<sliceforge::ui::PadsPanel> (processorRef);
    padEditor = std::make_unique<sliceforge::ui::PadEditorPanel> (processorRef);
    sequencerPanel = std::make_unique<sliceforge::ui::SequencerPanel> (processorRef);
    sequencerPanel->setComponentID ("sliceforge-sequencer-page");
    vaultPanel = std::make_unique<sliceforge::ui::VaultPanel> (processorRef);
    vaultPanel->setComponentID ("sliceforge-vault-page");
    footer = std::make_unique<sliceforge::ui::FooterBar> (processorRef);

    content->addAndMakeVisible (*header);
    content->addAndMakeVisible (*sourcePanel);
    content->addAndMakeVisible (*padsPanel);
    content->addAndMakeVisible (*padEditor);
    content->addAndMakeVisible (*sequencerPanel);
    content->addAndMakeVisible (*vaultPanel);
    content->addAndMakeVisible (*footer);
    sequencerPanel->setVisible (false);
    sequencerPanel->setActive (false);
    vaultPanel->setVisible (false);
    vaultPanel->setActive (false);
    vaultPanel->onSampleLoaded = [this] { showPage (sliceforge::ui::Page::kit); };

    header->onSavePreset = [this] { savePresetFile(); };
    header->onLoadPreset = [this] { loadPresetFile(); };
    header->onHelp = [this] { showHelp(); };
    header->onAbout = [this] { showAbout(); };
    header->onPageSelected = [this] (sliceforge::ui::Page page) { showPage (page); };
    header->onThemeToggle = [this] { toggleTheme(); };

    sourcePanel->onLoadFile = [this] { chooseSampleFile(); };
    sourcePanel->onToggleRecording = [this]
    {
        if (processorRef.isRecordingTakeReady())
            processorRef.adoptRecordingTake();
        else
            processorRef.toggleRecording();
        sourcePanel->refreshFromProcessor();
        repaint();
    };
    sourcePanel->onSourceChanged = [this] { refreshAll(); };
    sourcePanel->onSliceNav = [this] (int direction) { navigateSlice (direction); };
    sourcePanel->onAddDeleteSlice = [this] (bool add) { addOrDeleteSlice (add); };

    padsPanel->onAutoSelectChanged = [this] (bool enabled)
    {
        autoSelect = enabled;
        updateAutoSelection();
    };
    padsPanel->getPadGrid().onPadTriggered = [this] (int pad, float)
    {
        if (autoSelect)
            selectPad (pad);
    };
    padsPanel->getPadGrid().onPadSelected = [this] (int pad) { selectPad (pad); };
    padsPanel->getPadGrid().onMuteClicked = [this] (int pad) { setBoolPadParam (pad, "mute"); };
    padsPanel->getPadGrid().onSoloClicked = [this] (int pad) { setBoolPadParam (pad, "solo"); };

    padEditor->onSelectedPadChanged = [this] (int pad) { selectPad (pad); };

    processorRef.getUiBroadcaster().addChangeListener (this);
    lastTriggerStamp = processorRef.getTriggerStamp();

    setSize (kBaseWidth, kBaseHeight);
    refreshAll();
    startTimerHz (30);
}

SliceForgeAudioProcessorEditor::~SliceForgeAudioProcessorEditor()
{
    stopTimer();
    processorRef.getUiBroadcaster().removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void SliceForgeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (sliceforge::ui::palette().bg);
}

void SliceForgeAudioProcessorEditor::resized()
{
    if (content == nullptr)
        return;
    const auto scale = juce::jmin ((float) getWidth() / (float) kBaseWidth,
                                   (float) getHeight() / (float) kBaseHeight);
    if (scale <= 0.0f)
        return;
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setTopLeftPosition (juce::roundToInt ((getWidth() - kBaseWidth * scale) * 0.5f),
                                 juce::roundToInt ((getHeight() - kBaseHeight * scale) * 0.5f));

    header->setBounds (0, 0, kBaseWidth, 60);
    const int panelTop = 72;
    const int panelHeight = 726;
    sourcePanel->setBounds (12, panelTop, 338, panelHeight);
    padsPanel->setBounds (362, panelTop, 520, panelHeight);
    padEditor->setBounds (894, panelTop, 354, panelHeight);
    sequencerPanel->setBounds (12, panelTop, 1236, panelHeight);
    vaultPanel->setBounds (12, panelTop, 1236, panelHeight);
    footer->setBounds (0, 810, kBaseWidth, 30);
}

void SliceForgeAudioProcessorEditor::refreshAll()
{
    header->refresh();
    if (currentPage == sliceforge::ui::Page::sequencer)
    {
        sequencerPanel->refreshFromProcessor();
        footer->repaint();
        repaint();
        return;
    }
    if (currentPage == sliceforge::ui::Page::vault)
    {
        footer->repaint();
        repaint();
        return;
    }

    const auto selected = processorRef.getSelectedPad();
    sourcePanel->setSelectedPad (selected);
    sourcePanel->refreshFromProcessor();
    padsPanel->getPadGrid().setSelectedPad (selected);
    padsPanel->getPadGrid().repaint();
    padEditor->setSelectedPad (selected);
    padsPanel->repaint();
    footer->repaint();
    repaint();
}

void SliceForgeAudioProcessorEditor::showSequencerPage (bool shouldShow)
{
    showPage (shouldShow ? sliceforge::ui::Page::sequencer : sliceforge::ui::Page::kit);
}

void SliceForgeAudioProcessorEditor::showVaultPage (bool shouldShow)
{
    showPage (shouldShow ? sliceforge::ui::Page::vault : sliceforge::ui::Page::kit);
}

void SliceForgeAudioProcessorEditor::toggleTheme()
{
    sliceforge::ui::toggleThemeMode (true);
    applyThemeChange();
}

void SliceForgeAudioProcessorEditor::setDarkThemeForTesting (bool dark)
{
    sliceforge::ui::setThemeMode (dark ? sliceforge::ui::ThemeMode::dark : sliceforge::ui::ThemeMode::light, false);
    applyThemeChange();
}

void SliceForgeAudioProcessorEditor::repaintAll (juce::Component& component)
{
    component.repaint();
    for (int i = 0; i < component.getNumChildComponents(); ++i)
        if (auto* child = component.getChildComponent (i))
            repaintAll (*child);
}

void SliceForgeAudioProcessorEditor::applyThemeChange()
{
    lookAndFeel.refreshTheme();
    if (header != nullptr)
        header->refresh();
    if (content != nullptr)
    {
        // ComboBoxes and other composite widgets cache LAF-derived internal
        // label colours; re-notify the tree before repainting.
        content->sendLookAndFeelChange();
        if (sequencerPanel != nullptr)
            sequencerPanel->refreshTheme();
        if (vaultPanel != nullptr)
            vaultPanel->refreshTheme();
        repaintAll (*content);
    }
    repaint();
}

void SliceForgeAudioProcessorEditor::showPage (sliceforge::ui::Page page)
{
    currentPage = page;
    header->setPageActive (page);
    const bool kitVisible = page == sliceforge::ui::Page::kit;
    sourcePanel->setVisible (kitVisible);
    padsPanel->setVisible (kitVisible);
    padEditor->setVisible (kitVisible);
    sequencerPanel->setVisible (page == sliceforge::ui::Page::sequencer);
    sequencerPanel->setActive (page == sliceforge::ui::Page::sequencer);
    vaultPanel->setVisible (page == sliceforge::ui::Page::vault);
    vaultPanel->setActive (page == sliceforge::ui::Page::vault);
    refreshAll();
    repaint();
}

void SliceForgeAudioProcessorEditor::selectPad (int pad)
{
    pad = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
    processorRef.setSelectedPad (pad);
    sourcePanel->setSelectedPad (pad);
    padsPanel->getPadGrid().setSelectedPad (pad);
    padEditor->setSelectedPad (pad);
    sourcePanel->repaint();
    padsPanel->getPadGrid().repaint();
}

void SliceForgeAudioProcessorEditor::navigateSlice (int direction)
{
    const auto slices = processorRef.getSlices();
    if (slices.empty())
        return;
    const auto pad = processorRef.getSelectedPad();
    const auto current = processorRef.getPadStart (pad);
    int nearest = 0;
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < (int) slices.size(); ++i)
    {
        const auto d = std::abs (slices[(size_t) i].time - current);
        if (d < best)
        {
            best = d;
            nearest = i;
        }
    }
    int next = nearest;
    if (direction == 0)
        next = juce::Random::getSystemRandom().nextInt ((int) slices.size());
    else
        next = (nearest + direction + (int) slices.size()) % (int) slices.size();
    processorRef.pushUndoSnapshot();
    processorRef.setPadStart (pad, slices[(size_t) next].time);
    sourcePanel->refreshFromProcessor();
    padEditor->refreshFromProcessor();
    repaint();
}

void SliceForgeAudioProcessorEditor::addOrDeleteSlice (bool add)
{
    const auto pad = processorRef.getSelectedPad();
    const auto time = processorRef.getPadStart (pad);
    processorRef.pushUndoSnapshot();
    if (add)
        processorRef.addSliceAt (time);
    else
        processorRef.deleteNearestSlice (time);
    sourcePanel->refreshFromProcessor();
    padEditor->refreshFromProcessor();
    repaint();
}

void SliceForgeAudioProcessorEditor::setBoolPadParam (int pad, const juce::String& suffix)
{
    const auto padSettings = processorRef.getParameterCache().readPad (pad);
    const bool current = suffix == "mute" ? padSettings.mute : padSettings.solo;
    processorRef.pushUndoSnapshot();
    const auto id = sliceforge::padParamId (pad, suffix.toRawUTF8());
    if (auto* parameter = processorRef.getValueTreeState().getParameter (id))
        parameter->setValueNotifyingHost (parameter->getNormalisableRange().convertTo0to1 (current ? 0.0f : 1.0f));
    padsPanel->getPadGrid().repaint();
    padEditor->refreshFromProcessor();
}

void SliceForgeAudioProcessorEditor::updateAutoSelection()
{
    const auto stamp = processorRef.getTriggerStamp();
    if (stamp == lastTriggerStamp)
        return;
    lastTriggerStamp = stamp;
    if (autoSelect)
        selectPad (processorRef.getLastTriggeredPad());
}

void SliceForgeAudioProcessorEditor::timerCallback()
{
    updateAutoSelection();
    ++uiTicks;
    ++statusTicks;

    if (currentPage == sliceforge::ui::Page::sequencer || currentPage == sliceforge::ui::Page::vault)
    {
        if (uiTicks % 10 == 0)
        {
            header->refresh();
            footer->repaint();
        }
        return;
    }

    if (uiTicks % 2 == 0)
    {
        padsPanel->getPadGrid().repaint();
        padsPanel->repaint();
    }
    if (uiTicks % 10 == 0)
    {
        header->refresh();
        padEditor->refreshFromProcessor();
        footer->repaint();
        if (processorRef.isRecordingTakeReady() && ! processorRef.isRecording())
        {
            processorRef.adoptRecordingTake();
            sourcePanel->refreshFromProcessor();
            padEditor->refreshFromProcessor();
            repaint();
        }
        if (processorRef.isRecording())
            sourcePanel->refreshFromProcessor();
    }
    juce::ignoreUnused (statusTicks);
}

void SliceForgeAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshAll();
}

bool SliceForgeAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto code = key.getKeyCode();
    if (currentPage == sliceforge::ui::Page::vault)
        return false;
    if (code == juce::KeyPress::spaceKey)
    {
        if (currentPage == sliceforge::ui::Page::sequencer)
            sequencerPanel->togglePlayback();
        else
            processorRef.chokeAll();
        return true;
    }
    if (currentPage == sliceforge::ui::Page::sequencer
        && juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code) == (juce::juce_wchar) 'g')
    {
        sequencerPanel->randomizeShortcut();
        return true;
    }
    static const std::array<std::array<int, 4>, 4> rows {{
        {{ '1', '2', '3', '4' }},
        {{ 'q', 'w', 'e', 'r' }},
        {{ 'a', 's', 'd', 'f' }},
        {{ 'z', 'x', 'c', 'v' }}
    }};
    const auto character = juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) code);
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            if (character == (juce::juce_wchar) rows[(size_t) row][(size_t) col])
            {
                const auto pad = (3 - row) * 4 + col;
                auto velocity = 0.82f;
                if (key.getModifiers().isShiftDown())
                    velocity = 1.0f;
                if (key.getModifiers().isAltDown())
                    velocity = 0.3f;
                processorRef.triggerPad (pad, velocity);
                if (autoSelect)
                    selectPad (pad);
                return true;
            }
        }
    }
    return false;
}

bool SliceForgeAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
        if (juce::File (path).hasFileExtension ("wav;aif;aiff;mp3;flac;ogg;m4a"))
            return true;
    return false;
}

void SliceForgeAudioProcessorEditor::filesDropped (const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        const auto file = juce::File (path);
        if (! file.existsAsFile() || ! file.hasFileExtension ("wav;aif;aiff;mp3;flac;ogg;m4a"))
            continue;
        const auto loaded = processorRef.loadUserSampleFromFile (file);
        refreshAll();
        if (! loaded)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "SKEER SliceForge could not load that file",
                                                    file.getFileName() + " could not be decoded as audio.");
        return;
    }
}

void SliceForgeAudioProcessorEditor::chooseSampleFile()
{
    static const juce::String wildcard = "*.wav;*.aif;*.aiff;*.mp3;*.flac;*.ogg;*.m4a";
    fileChooser = std::make_unique<juce::FileChooser> ("Load a sound into SKEER SliceForge",
                                                      juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                      wildcard);
    auto safe = juce::Component::SafePointer<SliceForgeAudioProcessorEditor> (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& chooser)
                              {
                                  if (safe == nullptr)
                                      return;
                                  const auto file = chooser.getResult();
                                  if (file.existsAsFile())
                                  {
                                      const auto loaded = safe->processorRef.loadUserSampleFromFile (file);
                                      safe->refreshAll();
                                      if (! loaded)
                                          juce::AlertWindow::showMessageBoxAsync (
                                              juce::MessageBoxIconType::WarningIcon,
                                              "SKEER SliceForge could not load that file",
                                              file.getFileName() + " could not be decoded as audio.");
                                  }
                              });
}

void SliceForgeAudioProcessorEditor::savePresetFile()
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("SliceForge")
                         .getChildFile ("Presets");
    directory.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Save SKEER SliceForge preset",
                                                      directory.getChildFile (processorRef.getPresetName() + ".vracpreset"),
                                                      "*.vracpreset");
    auto safe = juce::Component::SafePointer<SliceForgeAudioProcessorEditor> (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& chooser)
                              {
                                  if (safe == nullptr)
                                      return;
                                  auto file = chooser.getResult();
                                  if (file == juce::File())
                                      return;
                                  if (! file.hasFileExtension ("vracpreset"))
                                      file = file.withFileExtension ("vracpreset");
                                  juce::MemoryBlock data;
                                  safe->processorRef.getStateInformation (data);
                                  if (file.replaceWithData (data.getData(), data.getSize()))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                                              "SKEER SliceForge preset saved",
                                                                              file.getFullPathName());
                              });
}

void SliceForgeAudioProcessorEditor::loadPresetFile()
{
    auto directory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("SliceForge")
                         .getChildFile ("Presets");
    directory.createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Load SKEER SliceForge preset", directory, "*.vracpreset");
    auto safe = juce::Component::SafePointer<SliceForgeAudioProcessorEditor> (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& chooser)
                              {
                                  if (safe == nullptr)
                                      return;
                                  const auto file = chooser.getResult();
                                  if (! file.existsAsFile())
                                      return;
                                  juce::MemoryBlock data;
                                  if (file.loadFileAsData (data))
                                  {
                                      safe->processorRef.setStateInformation (data.getData(), (int) data.getSize());
                                      safe->refreshAll();
                                  }
                              });
}

void SliceForgeAudioProcessorEditor::showHelp()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "SKEER SliceForge - quick guide",
        "LOAD A SOUND\nDrop or load WAV / AIFF / MP3 / FLAC / OGG / M4A, choose a library source, or record a sidechain input.\n\n"
        "SLICE IT\nSet Auto Slice sensitivity, step through hits with < > RND, add or delete a slice, and drag the orange marker to choose any pad's start.\n\n"
        "MORPH IT\nEvery pad blends between the original sample and one of 16 drum models. Use Drumify, then Warp / Impact / Instability / Super / Tone / EQ to finish the hit.\n\n"
        "PLAY IT\nClicks, MIDI notes C1-D#2, and the QWERTY keys 1-4 / QWER / ASDF / ZXCV all trigger pads. MIDI notes also follow Auto Select.\n\n"
        "MIX & LEARN\nUse Mute / Solo, choke groups and one of eight stereo outputs. SOFT CLIP decides how early the output peaks bend - 0 % is bypassed and nothing under the knee is ever attenuated - and OUT trims what comes after it, under a ceiling that keeps the plugin inside full scale. Right-click any knob and move a MIDI CC to learn it.\n\n"
        "BROWSE THE VAULT\nIf SKEER Sample Vault is installed, the VAULT tab reads its sample index directly. Search, filter, check indexed metadata and load a sample into the kit. Clicking a row auditions it once - PLAY hears it again, LOOP keeps it going and STOP cuts it. Use LOCATE DB if the index is in a custom location.\n\n"
        "SEQUENCE IT\nThe SEQUENCER tab adds four patterns, host-synced playback, probability, ratchets, nudge, flams, parameter locks, polymeter and an eight-slot song chain. COPY TO - or a right-click on a pattern letter - duplicates that pattern into another slot; the destination is replaced and one undo step brings back what was there.\n\n"
        "The host session saves the full kit, including the slice map, sequencer and a custom sample.",
        "GOT IT");
}

void SliceForgeAudioProcessorEditor::showAbout()
{
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "SKEER SliceForge - drumify any sound",
        "One sound. One drum kit.\n\n"
        "SKEER SliceForge turns any recording into a sixteen-pad instrument. Auto-slice it, morph each pad across kick, snare, hat and clap models, then shape the result with Warp, Impact, Instability, Tone Shift, Super and a three-band EQ.\n\n"
        "16 pads  /  16 models  /  8 stereo outputs  /  SKEER interface and factory library.\n\n"
        "An independent VST3/standalone interpretation inspired by VRAC+ by Beatsurfing, with SKEER branding. Not affiliated with or endorsed by Beatsurfing / Druw Audio. Factory sound content is original procedural material.",
        "CLOSE");
}

