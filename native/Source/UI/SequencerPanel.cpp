#include "SequencerPanel.h"
#include "Theme.h"
#include "Controls.h"

namespace sliceforge::ui
{
    namespace
    {
        void styleSeqSlider (juce::Slider& slider)
        {
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
            slider.setColour (juce::Slider::textBoxTextColourId, palette().ink);
            slider.setColour (juce::Slider::textBoxBackgroundColourId, palette().panel);
            slider.setColour (juce::Slider::textBoxOutlineColourId, palette().line);
            slider.setPopupMenuEnabled (false);
        }

        bool patternHasNotes (const sliceforge::SeqPattern& pattern)
        {
            for (const auto& row : pattern.rows)
                for (const auto& step : row)
                    if (step.on)
                        return true;
            return false;
        }
        class StepInspector final : public juce::Component
        {
        public:
            StepInspector (int pad, int stepNumber, const sliceforge::SeqStep& initial,
                           const sliceforge::PadSettings& baseSettings,
                           std::function<void (const sliceforge::SeqStep&)> changedCallback)
                : step (initial), base (baseSettings), onChanged (std::move (changedCallback)),
                  padIndex (pad), stepIndex (stepNumber)
            {
                setOpaque (true);
                setSize (340, 300);
                styleChip (lockButton, step.locks.enabled);
                lockButton.setButtonText (step.locks.enabled ? "PARAMETER LOCK  ON" : "PARAMETER LOCK  OFF");
                addAndMakeVisible (lockButton);
                lockButton.onClick = [this]
                {
                    const bool enable = ! step.locks.enabled;
                    step.locks.enabled = enable;
                    if (enable)
                    {
                        step.locks.pitch = base.pitch;
                        step.locks.volume = base.volume;
                        step.locks.pan = base.pan;
                        step.locks.tone = base.toneShift;
                        step.locks.length = juce::jlimit (0.02f, 2.0f, base.length);
                    }
                    styleChip (lockButton, enable);
                    lockButton.setButtonText (enable ? "PARAMETER LOCK  ON" : "PARAMETER LOCK  OFF");
                    refresh();
                    notify();
                };

                auto setup = [this] (juce::Slider& slider, double min, double max, double interval,
                                     const juce::String& suffix)
                {
                    slider.setRange (min, max, interval);
                    slider.setTextValueSuffix (suffix);
                    styleSeqSlider (slider);
                    addAndMakeVisible (slider);
                };
                setup (nudge, -50.0, 50.0, 1.0, " %");
                setup (flam, 0.0, 2.0, 1.0, "");
                setup (pitch, -24.0, 24.0, 0.1, " st");
                setup (volume, -60.0, 6.0, 0.1, " dB");
                setup (pan, -1.0, 1.0, 0.01, "");
                setup (tone, -12.0, 12.0, 0.1, " st");
                setup (length, 0.02, 2.0, 0.01, " s");

                nudge.onValueChange = [this] { if (updating) return; step.nudge = (float) (nudge.getValue() / 100.0); notify(); };
                flam.onValueChange = [this] { if (updating) return; step.flam = juce::jlimit (0, 2, (int) flam.getValue()); notify(); };
                pitch.onValueChange = [this] { if (updating) return; step.locks.pitch = (float) pitch.getValue(); notify(); };
                volume.onValueChange = [this] { if (updating) return; step.locks.volume = (float) volume.getValue(); notify(); };
                pan.onValueChange = [this] { if (updating) return; step.locks.pan = (float) pan.getValue(); notify(); };
                tone.onValueChange = [this] { if (updating) return; step.locks.tone = (float) tone.getValue(); notify(); };
                length.onValueChange = [this] { if (updating) return; step.locks.length = (float) length.getValue(); notify(); };

                for (auto* slider : { &nudge, &flam, &pitch, &volume, &pan, &tone, &length })
                    slider->onDragStart = [this] { if (onGestureStart) onGestureStart(); };

                refresh();
            }

            std::function<void()> onGestureStart;

            void paint (juce::Graphics& g) override
            {
                const auto& c = palette();
                drawPanelBackground (g, getLocalBounds().toFloat(), true);
                g.setColour (c.ink);
                g.setFont (font (11.0f, true));
                g.drawText ("STEP " + juce::String (stepIndex + 1) + "  /  " + sliceforge::padNotes()[(size_t) padIndex],
                            12, 8, getWidth() - 24, 16, juce::Justification::centredLeft, false);

                g.setColour (c.muted);
                g.setFont (font (6.8f, true));
                static const char* labels[] { "NUDGE", "FLAM", "PITCH", "VOLUME", "PAN", "TONE", "LENGTH" };
                for (int i = 0; i < 7; ++i)
                    g.drawText (labels[i], 12, 66 + i * 30, 82, 20, juce::Justification::centredLeft, false);

                g.setFont (font (6.2f, true));
                g.drawText ("RIGHT-CLICK ANY STEP TO EDIT MICRO-TIMING, FLAMS AND LOCKS",
                            12, getHeight() - 22, getWidth() - 24, 12, juce::Justification::centredLeft, false);
            }

            void resized() override
            {
                lockButton.setBounds (12, 30, 160, 24);
                nudge.setBounds (96, 62, 232, 22);
                flam.setBounds (96, 92, 232, 22);
                pitch.setBounds (96, 122, 232, 22);
                volume.setBounds (96, 152, 232, 22);
                pan.setBounds (96, 182, 232, 22);
                tone.setBounds (96, 212, 232, 22);
                length.setBounds (96, 242, 232, 22);
            }

        private:
            void refresh()
            {
                updating = true;
                nudge.setValue (step.nudge * 100.0, juce::dontSendNotification);
                flam.setValue (step.flam, juce::dontSendNotification);
                pitch.setValue (step.locks.pitch, juce::dontSendNotification);
                volume.setValue (step.locks.volume, juce::dontSendNotification);
                pan.setValue (step.locks.pan, juce::dontSendNotification);
                tone.setValue (step.locks.tone, juce::dontSendNotification);
                length.setValue (step.locks.length, juce::dontSendNotification);
                for (auto* slider : { &pitch, &volume, &pan, &tone, &length })
                    slider->setEnabled (step.locks.enabled);
                updating = false;
            }

            void notify()
            {
                if (onChanged)
                    onChanged (step);
            }

            sliceforge::SeqStep step;
            sliceforge::PadSettings base;
            std::function<void (const sliceforge::SeqStep&)> onChanged;
            int padIndex = 0;
            int stepIndex = 0;
            juce::TextButton lockButton { "PARAMETER LOCK" };
            juce::Slider nudge, flam, pitch, volume, pan, tone, length;
            bool updating = false;
        };
    } // namespace

    SequencerPanel::SequencerPanel (SliceForgeAudioProcessor& processor)
        : processorRef (processor), grid (processor)
    {
        addAndMakeVisible (grid);
        grid.setEditMode (editMode);
        grid.onGestureStart = [this] { pushUndo(); };
        grid.onStepChanged = [this] (int pad, int step, const sliceforge::SeqStep& newStep)
        {
            if (data == nullptr)
                return;
            auto copy = sliceforge::cloneSequencerData (*data);
            const auto pattern = juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern);
            copy->patterns[(size_t) pattern].rows[(size_t) pad][(size_t) step] = newStep;
            commit (copy, false);
        };
        grid.onLaneMuteClicked = [this] (int pad)
        {
            edit ([pad] (sliceforge::SequencerData& d)
                  {
                      auto& p = d.patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, d.activePattern)];
                      p.laneMute[(size_t) pad] = ! p.laneMute[(size_t) pad];
                  }, true);
        };
        grid.onLaneSoloClicked = [this] (int pad)
        {
            edit ([pad] (sliceforge::SequencerData& d)
                  {
                      auto& p = d.patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, d.activePattern)];
                      p.laneSolo[(size_t) pad] = ! p.laneSolo[(size_t) pad];
                  }, true);
        };
        grid.onLaneSelected = [this] (int pad)
        {
            selectedLane = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
            grid.setSelectedLane (selectedLane);
            updateControls();
            repaint();
        };
        grid.onStepInspectorRequested = [this] (int pad, int step, juce::Point<int> screenPosition)
        {
            openStepInspector (pad, step, screenPosition);
        };

        for (int i = 0; i < sliceforge::kSeqPatterns; ++i)
        {
            auto& button = patternButtons[(size_t) i];
            button.setButtonText (juce::String::charToString ((juce::juce_wchar) ('A' + i)));
            styleChip (button, i == 0);
            addAndMakeVisible (button);
            button.onClick = [this, i]
            {
                // A right-click that opened the copy menu must not also select the pattern.
                if (chipClickToIgnore == i)
                    return;
                setActivePattern (i);
            };
            button.setTooltip ("Pattern " + juce::String::charToString ((juce::juce_wchar) ('A' + i))
                                   + "  -  click to play it, right-click to copy it to another pattern");
            chipPopups[(size_t) i] = std::make_unique<ChipPopupMenu> (i, this);
            button.addMouseListener (chipPopups[(size_t) i].get(), false);
        }

        styleChip (playButton, false);
        playButton.setColour (juce::TextButton::buttonOnColourId, palette().green);
        styleChip (hostButton, true);
        hostButton.setClickingTogglesState (true);
        addAndMakeVisible (playButton);
        addAndMakeVisible (hostButton);
        playButton.onClick = [this] { togglePlayback(); };
        hostButton.onClick = [this]
        {
            if (updatingControls)
                return;
            edit ([this] (sliceforge::SequencerData& d) { d.hostSync = hostButton.getToggleState(); }, true);
        };

        bpmSlider.setRange (40.0, 300.0, 1.0);
        swingSlider.setRange (0.0, 66.0, 1.0);
        lengthSlider.setRange (1.0, (double) sliceforge::kSeqSteps, 1.0);
        styleSeqSlider (bpmSlider);
        styleSeqSlider (swingSlider);
        styleSeqSlider (lengthSlider);
        bpmSlider.setTextValueSuffix (" BPM");
        swingSlider.setTextValueSuffix (" %");
        lengthSlider.setTextValueSuffix (" ST");
        addAndMakeVisible (bpmSlider);
        addAndMakeVisible (swingSlider);
        addAndMakeVisible (lengthSlider);
        bpmSlider.onDragStart = [this] { pushUndo(); };
        swingSlider.onDragStart = [this] { pushUndo(); };
        lengthSlider.onDragStart = [this] { pushUndo(); };
        bpmSlider.onValueChange = [this]
        {
            if (updatingControls)
                return;
            edit ([this] (sliceforge::SequencerData& d) { d.bpm = bpmSlider.getValue(); }, false);
        };
        swingSlider.onValueChange = [this]
        {
            if (updatingControls)
                return;
            edit ([this] (sliceforge::SequencerData& d) { d.swing = (float) (swingSlider.getValue() / 100.0); }, false);
        };
        lengthSlider.onValueChange = [this]
        {
            if (updatingControls)
                return;
            edit ([this] (sliceforge::SequencerData& d) { d.length = juce::jlimit (1, sliceforge::kSeqSteps, (int) lengthSlider.getValue()); }, false);
        };

        divisionBox.addItem ("1/8", 2);
        divisionBox.addItem ("1/8T", 3);
        divisionBox.addItem ("1/16", 4);
        divisionBox.addItem ("1/16T", 6);
        divisionBox.addItem ("1/32", 8);
        addAndMakeVisible (divisionBox);
        divisionBox.onChange = [this]
        {
            if (updatingControls)
                return;
            const auto division = divisionBox.getSelectedId();
            if (division <= 0)
                return;
            edit ([division] (sliceforge::SequencerData& d) { d.division = division; }, true);
        };

        const char* modeNames[] { "DRAW", "VELOCITY", "PROB", "RATCHET", "NUDGE", "FLAM" };
        for (int i = 0; i < 6; ++i)
        {
            auto& button = modeButtons[(size_t) i];
            button.setButtonText (modeNames[i]);
            styleChip (button, i == 0);
            addAndMakeVisible (button);
            button.onClick = [this, i] { setEditMode ((SequencerGrid::EditMode) i); };
        }

        for (auto* button : { &randomButton, &clearButton, &copyButton, &pasteButton, &copyToButton,
                              &laneRandomButton, &euclidButton, &songButton,
                              &patternPresetButton, &midiButton })
        {
            styleChip (*button, false);
            addAndMakeVisible (*button);
        }
        randomButton.onClick = [this] { showRandomMenu(); };
        clearButton.onClick = [this] { clearCurrentPattern(); };
        copyButton.onClick = [this] { copyCurrentPattern(); };
        pasteButton.onClick = [this] { pasteCurrentPattern(); };
        copyToButton.onClick = [this]
        {
            showPatternCopyMenu (data != nullptr
                                     ? juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern)
                                     : 0,
                                 &copyToButton);
        };
        copyToButton.setTooltip ("Copy the pattern you are editing to another of the four slots. "
                                 "The destination is replaced; one undo step brings back what was there.");
        laneRandomButton.onClick = [this] { randomizeLane (selectedLane, 0.5f); };
        euclidButton.onClick = [this] { euclidLane (5); };
        songButton.setClickingTogglesState (true);
        songButton.onClick = [this]
        {
            if (! updatingControls)
                setSongMode (songButton.getToggleState());
        };
        patternPresetButton.onClick = [this] { showPatternPresetMenu(); };
        midiButton.onClick = [this] { showMidiMenu(); };

        for (int i = 0; i < sliceforge::kSeqChainLength; ++i)
        {
            auto& button = chainButtons[(size_t) i];
            styleChip (button, false);
            addAndMakeVisible (button);
            button.onClick = [this, i] { cycleChainSlot (i); };
        }

        laneLengthBox.addItem ("GLOBAL", 1);
        for (int length = 1; length <= sliceforge::kSeqSteps; ++length)
            laneLengthBox.addItem (juce::String (length), length + 1);
        addAndMakeVisible (laneLengthBox);
        laneLengthBox.onChange = [this]
        {
            if (updatingControls)
                return;
            const auto id = laneLengthBox.getSelectedId();
            if (id <= 0)
                return;
            setLaneLength (id == 1 ? 0 : juce::jlimit (1, sliceforge::kSeqSteps, id - 1));
        };

        refreshFromProcessor();
        startTimerHz (30);
    }

    void SequencerPanel::setActive (bool shouldBeActive)
    {
        if (shouldBeActive)
        {
            refreshFromProcessor();
            startTimerHz (30);
        }
        else
        {
            stopTimer();
        }
    }

    void SequencerPanel::refreshFromProcessor()
    {
        data = processorRef.getSequencerData();
        if (data == nullptr)
            return;
        selectedLane = juce::jlimit (0, sliceforge::kNumPads - 1, selectedLane);
        grid.setData (data);
        grid.setSelectedLane (selectedLane);
        updateControls();
        updateTransportButton();
        repaint();
    }

    void SequencerPanel::refreshPlayhead()
    {
        grid.setPlayheadStep (processorRef.getSequencerPlayheadStep());
        updateTransportButton();
        repaint();
    }

    void SequencerPanel::timerCallback()
    {
        refreshPlayhead();
    }

    void SequencerPanel::updateTransportButton()
    {
        const bool hostPlaying = processorRef.isSequencerHostPlaying();
        const bool internal = processorRef.isSequencerInternalPlaying();
        const bool active = hostPlaying || internal;
        playButton.setEnabled (! hostPlaying);
        playButton.setButtonText (hostPlaying ? "HOST" : (internal ? "STOP" : "PLAY"));
        styleChip (playButton, active);
    }

    void SequencerPanel::togglePlayback()
    {
        if (processorRef.isSequencerHostPlaying())
            return;
        processorRef.setSequencerInternalPlaying (! processorRef.isSequencerInternalPlaying());
        updateTransportButton();
        grid.repaint();
    }

    void SequencerPanel::randomizeShortcut()
    {
        applyRandomChoice (2);
    }

    void SequencerPanel::refreshTheme()
    {
        styleSeqSlider (bpmSlider);
        styleSeqSlider (swingSlider);
        styleSeqSlider (lengthSlider);
        grid.repaint();
        repaint();
    }

    void SequencerPanel::showStepInspectorForTesting (int pad, int step)
    {
        openStepInspector (pad, step, getScreenBounds().getCentre());
    }

    void SequencerPanel::pushUndo()
    {
        processorRef.pushUndoSnapshot();
    }

    void SequencerPanel::commit (std::shared_ptr<sliceforge::SequencerData> newData, bool refreshControls)
    {
        data = std::move (newData);
        processorRef.setSequencerData (data);
        grid.setData (data);
        if (refreshControls)
            updateControls();
        repaint();
    }

    void SequencerPanel::edit (const std::function<void (sliceforge::SequencerData&)>& mutation, bool undo)
    {
        if (data == nullptr)
            return;
        if (undo)
            pushUndo();
        auto copy = sliceforge::cloneSequencerData (*data);
        mutation (*copy);
        commit (std::move (copy), true);
    }

    void SequencerPanel::updateControls()
    {
        if (data == nullptr)
            return;
        updatingControls = true;

        const int activePattern = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
        for (int i = 0; i < sliceforge::kSeqPatterns; ++i)
            styleChip (patternButtons[(size_t) i], i == activePattern);

        bpmSlider.setValue (juce::jlimit (40.0, 300.0, data->bpm), juce::dontSendNotification);
        swingSlider.setValue (juce::jlimit (0.0, 66.0, (double) data->swing * 100.0),
                              juce::dontSendNotification);
        lengthSlider.setValue (juce::jlimit (1, sliceforge::kSeqSteps, data->length), juce::dontSendNotification);
        const auto division = data->division == 2 || data->division == 3 || data->division == 4
                                  || data->division == 6 || data->division == 8
                              ? data->division : 4;
        divisionBox.setSelectedId (division, juce::dontSendNotification);
        hostButton.setToggleState (data->hostSync, juce::dontSendNotification);
        styleChip (hostButton, data->hostSync);
        songButton.setToggleState (data->songMode, juce::dontSendNotification);
        styleChip (songButton, data->songMode);

        for (int i = 0; i < 6; ++i)
            styleChip (modeButtons[(size_t) i], (int) editMode == i);

        for (int i = 0; i < sliceforge::kSeqChainLength; ++i)
        {
            const auto value = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->chainPatterns[(size_t) i]);
            chainButtons[(size_t) i].setButtonText (
                juce::String::charToString ((juce::juce_wchar) ('A' + value)));
            styleChip (chainButtons[(size_t) i], false);
        }

        const auto& pattern = data->patterns[(size_t) activePattern];
        const auto laneLength = juce::jlimit (0, sliceforge::kSeqSteps,
                                              pattern.laneLength[(size_t) selectedLane]);
        laneLengthBox.setSelectedId (laneLength == 0 ? 1 : laneLength + 1,
                                     juce::dontSendNotification);

        grid.setData (data);
        grid.setSelectedLane (selectedLane);
        grid.setPlayheadStep (processorRef.getSequencerPlayheadStep());
        pasteButton.setEnabled (patternClipboard.has_value());
        clearButton.setEnabled (data != nullptr);
        patternPresetButton.setEnabled (data != nullptr);
        midiButton.setEnabled (data != nullptr);

        updatingControls = false;
    }

    void SequencerPanel::setActivePattern (int pattern)
    {
        pattern = juce::jlimit (0, sliceforge::kSeqPatterns - 1, pattern);
        if (data == nullptr || pattern == data->activePattern)
            return;
        edit ([pattern] (sliceforge::SequencerData& d) { d.activePattern = pattern; }, true);
    }

    void SequencerPanel::setEditMode (SequencerGrid::EditMode mode)
    {
        editMode = mode;
        grid.setEditMode (mode);
        for (int i = 0; i < 6; ++i)
            styleChip (modeButtons[(size_t) i], (int) mode == i);
        repaint();
    }

    void SequencerPanel::showRandomMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Random groove  -  light");
        menu.addItem (2, "Random groove  -  medium");
        menu.addItem (3, "Random groove  -  busy");
        menu.addSeparator();
        menu.addItem (4, "Mutate pattern");
        menu.addItem (5, "Humanize velocities");
        menu.addItem (6, "Accent downbeats");
        menu.addItem (7, "Shift left");
        menu.addItem (8, "Shift right");
        menu.addSeparator();
        menu.addItem (9, "Randomize selected lane");
        menu.addItem (10, "Euclid selected lane  -  3 pulses");
        menu.addItem (11, "Euclid selected lane  -  5 pulses");
        menu.addItem (12, "Euclid selected lane  -  7 pulses");
        menu.addSeparator();
        menu.addItem (13, "Clear pattern");
        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&randomButton),
                            [safe] (int result)
                            {
                                if (safe != nullptr && result > 0)
                                    safe->applyRandomChoice (result);
                            });
    }

    void SequencerPanel::applyRandomChoice (int choice)
    {
        if (data == nullptr)
            return;

        if (choice == 9)
        {
            randomizeLane (selectedLane, 0.5f);
            return;
        }
        if (choice >= 10 && choice <= 12)
        {
            const int pulses = choice == 10 ? 3 : (choice == 11 ? 5 : 7);
            euclidLane (pulses);
            return;
        }

        pushUndo();
        auto copy = sliceforge::cloneSequencerData (*data);
        auto& pattern = copy->patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern)];
        juce::Random random;
        random.setSeed ((juce::int64) juce::Time::currentTimeMillis());

        std::array<int, sliceforge::kNumPads> families {};
        for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
            families[(size_t) pad] = sliceforge::familyForModel (processorRef.getParameterCache().readPad (pad).modelIndex);

        if (choice >= 1 && choice <= 3)
        {
            const float density = choice == 1 ? 0.32f : (choice == 2 ? 0.5f : 0.72f);
            sliceforge::randomizePattern (pattern, copy->length, families, density, random);
        }
        else if (choice == 4)
        {
            sliceforge::mutatePattern (pattern, copy->length, 0.055f, random);
        }
        else if (choice == 5)
        {
            sliceforge::humanizePattern (pattern, copy->length, random);
        }
        else if (choice == 6)
        {
            sliceforge::accentDownbeats (pattern, copy->length);
        }
        else if (choice == 7)
        {
            sliceforge::rotatePattern (pattern, copy->length, -1);
        }
        else if (choice == 8)
        {
            sliceforge::rotatePattern (pattern, copy->length, 1);
        }
        else if (choice == 13)
        {
            sliceforge::clearPattern (pattern);
        }
        else
        {
            return;
        }

        commit (std::move (copy), true);
    }

    void SequencerPanel::randomizeLane (int pad, float density)
    {
        if (data == nullptr)
            return;
        pad = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
        pushUndo();
        auto copy = sliceforge::cloneSequencerData (*data);
        auto& pattern = copy->patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern)];
        std::array<int, sliceforge::kNumPads> families {};
        for (int i = 0; i < sliceforge::kNumPads; ++i)
            families[(size_t) i] = sliceforge::familyForModel (processorRef.getParameterCache().readPad (i).modelIndex);
        juce::Random random;
        random.setSeed ((juce::int64) juce::Time::currentTimeMillis());
        sliceforge::SeqPattern temp;
        sliceforge::randomizePattern (temp, copy->length, families, density, random);
        pattern.rows[(size_t) pad] = temp.rows[(size_t) pad];
        pattern.laneMute[(size_t) pad] = false;
        pattern.laneSolo[(size_t) pad] = false;
        selectedLane = pad;
        commit (std::move (copy), true);
    }

    void SequencerPanel::euclidLane (int pulses)
    {
        if (data == nullptr)
            return;
        pushUndo();
        auto copy = sliceforge::cloneSequencerData (*data);
        auto& pattern = copy->patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern)];
        sliceforge::euclideanLane (pattern, selectedLane, copy->length, pulses, 0, 0.82f);
        commit (std::move (copy), true);
    }

    void SequencerPanel::copyCurrentPattern()
    {
        if (data == nullptr)
            return;
        patternClipboard = data->patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern)];
        pasteButton.setEnabled (true);
        repaint();
    }

    void SequencerPanel::pasteCurrentPattern()
    {
        if (data == nullptr || ! patternClipboard.has_value())
            return;
        pushUndo();
        auto copy = sliceforge::cloneSequencerData (*data);
        copy->patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern)] = *patternClipboard;
        commit (std::move (copy), true);
    }

    void SequencerPanel::showPatternCopyMenu (int sourcePattern, juce::Component* targetComponent)
    {
        if (data == nullptr)
            return;

        sourcePattern = juce::jlimit (0, sliceforge::kSeqPatterns - 1, sourcePattern);
        const auto letter = [] (int index)
                            {
                                return juce::String::charToString ((juce::juce_wchar) ('A' + index));
                            };

        const auto& source = data->patterns[(size_t) sourcePattern];
        const juce::String sourceEmpty = patternHasNotes (source)
                                             ? juce::String()
                                             : juce::String ("  (empty)");

        juce::PopupMenu menu;
        menu.addItem (1, "Copy pattern " + letter (sourcePattern) + sourceEmpty, false, false);
        menu.addSeparator();

        for (int target = 0; target < sliceforge::kSeqPatterns; ++target)
        {
            if (target == sourcePattern)
                continue;

            const auto replaces = patternHasNotes (data->patterns[(size_t) target])
                                      ? "  (replaces " + letter (target) + ")"
                                      : juce::String();
            // +1 keeps every id above zero, which showMenuAsync uses for "nothing chosen".
            menu.addItem (sourcePattern * sliceforge::kSeqPatterns + target + 1,
                          "Copy " + letter (sourcePattern) + "  -  to " + letter (target) + replaces);
        }

        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (targetComponent),
                            [safe] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                // The menu is closed, so no chip click is pending suppression any
                                // more - this runs for a choice and for a dismissal alike.
                                safe->chipClickToIgnore = -1;
                                if (result <= 0)
                                    return;
                                const auto chosen = result - 1;
                                safe->applyPatternCopy (chosen / sliceforge::kSeqPatterns,
                                                        chosen % sliceforge::kSeqPatterns);
                            });
    }

    void SequencerPanel::applyPatternCopy (int source, int target)
    {
        if (data == nullptr)
            return;

        // Not edit(): the undo snapshot has to be conditional. A rejected pair would otherwise
        // spend an undo step on nothing, and copyPatternTo is the only place that knows whether a
        // pair is legal. The processor still holds the pre-edit state here, so taking the snapshot
        // after mutating the local copy is still taking it before the edit.
        auto copy = sliceforge::cloneSequencerData (*data);
        if (! sliceforge::copyPatternTo (*copy, source, target))
            return;

        pushUndo();
        commit (std::move (copy), true);
    }

    void SequencerPanel::clearCurrentPattern()
    {
        edit ([] (sliceforge::SequencerData& d)
              {
                  auto& pattern = d.patterns[(size_t) juce::jlimit (0, sliceforge::kSeqPatterns - 1, d.activePattern)];
                  sliceforge::clearPattern (pattern);
              }, true);
    }

    void SequencerPanel::openStepInspector (int pad, int step, juce::Point<int> screenPosition)
    {
        if (data == nullptr)
            return;
        pad = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
        step = juce::jlimit (0, sliceforge::kSeqSteps - 1, step);
        pushUndo();
        const auto patternIndex = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
        const auto initialStep = data->patterns[(size_t) patternIndex].rows[(size_t) pad][(size_t) step];
        const auto baseSettings = processorRef.getParameterCache().readPad (pad);
        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        auto inspector = std::make_unique<StepInspector> (
            pad, step, initialStep, baseSettings,
            [safe, pad, step] (const sliceforge::SeqStep& newStep)
            {
                if (safe == nullptr || safe->data == nullptr)
                    return;
                auto copy = sliceforge::cloneSequencerData (*safe->data);
                const auto active = juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern);
                copy->patterns[(size_t) active].rows[(size_t) pad][(size_t) step] = newStep;
                safe->commit (std::move (copy), true);
            });
        inspector->setLookAndFeel (&getLookAndFeel());
        inspector->onGestureStart = [safe] { if (safe != nullptr) safe->pushUndo(); };
        auto* parent = getTopLevelComponent();
        const auto screenArea = juce::Rectangle<int> (screenPosition.x, screenPosition.y, 1, 1);
        const auto parentArea = parent != nullptr ? parent->getLocalArea (nullptr, screenArea) : screenArea;
        juce::CallOutBox::launchAsynchronously (std::move (inspector), parentArea,
                                                parent != nullptr ? parent : nullptr);
    }

    void SequencerPanel::showPatternPresetMenu()
    {
        juce::PopupMenu menu;
        for (int i = 0; i < 6; ++i)
            menu.addItem (i + 1, sliceforge::patternPresetName ((sliceforge::PatternPreset) i));
        menu.addSeparator();
        menu.addItem (20, "Save pattern preset...");
        menu.addItem (21, "Load pattern preset...");
        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&patternPresetButton),
                            [safe] (int result)
                            {
                                if (safe == nullptr || result <= 0)
                                    return;
                                if (result <= 6)
                                    safe->applyPatternPreset (result - 1);
                                else if (result == 20)
                                    safe->savePatternPresetFile();
                                else if (result == 21)
                                    safe->loadPatternPresetFile();
                            });
    }

    void SequencerPanel::applyPatternPreset (int presetIndex)
    {
        if (data == nullptr)
            return;
        presetIndex = juce::jlimit (0, 5, presetIndex);
        pushUndo();
        auto copy = sliceforge::cloneSequencerData (*data);
        const auto active = juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern);
        copy->patterns[(size_t) active] = sliceforge::makePatternPreset ((sliceforge::PatternPreset) presetIndex);
        commit (std::move (copy), true);
    }

    void SequencerPanel::savePatternPresetFile()
    {
        if (data == nullptr)
            return;
        fileChooser = std::make_unique<juce::FileChooser> (
            "Save SliceForge pattern preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("SliceForge Pattern.vracpattern"),
            "*.vracpattern");
        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safe] (const juce::FileChooser& chooser)
                                  {
                                      if (safe != nullptr)
                                          safe->savePatternToFile (chooser.getResult());
                                  });
    }

    void SequencerPanel::loadPatternPresetFile()
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Load SliceForge pattern preset",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.vracpattern");
        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [safe] (const juce::FileChooser& chooser)
                                  {
                                      if (safe != nullptr)
                                          safe->loadPatternFromFile (chooser.getResult());
                                  });
    }

    void SequencerPanel::savePatternToFile (const juce::File& file)
    {
        if (data == nullptr || ! file.getFullPathName().isNotEmpty())
            return;
        auto copy = sliceforge::cloneSequencerData (*data);
        const auto active = juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern);
        const auto block = sliceforge::serialisePattern (copy->patterns[(size_t) active]);
        auto target = file.hasFileExtension ("vracpattern") ? file : file.withFileExtension ("vracpattern");
        if (! target.replaceWithData (block.getData(), block.getSize()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "SliceForge pattern preset",
                                                    "Could not write " + target.getFullPathName());
    }

    void SequencerPanel::loadPatternFromFile (const juce::File& file)
    {
        if (data == nullptr || ! file.existsAsFile())
            return;
        juce::MemoryBlock block;
        if (! file.loadFileAsData (block))
            return;
        const auto loaded = sliceforge::deserialisePattern (block);
        if (loaded == nullptr)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "SliceForge pattern preset",
                                                    "That file is not a valid SliceForge pattern preset.");
            return;
        }
        pushUndo();
        auto copy = sliceforge::cloneSequencerData (*data);
        const auto active = juce::jlimit (0, sliceforge::kSeqPatterns - 1, copy->activePattern);
        copy->patterns[(size_t) active] = *loaded;
        commit (std::move (copy), true);
    }

    void SequencerPanel::showMidiMenu()
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Export MIDI file...");
        menu.addItem (2, "Drag MIDI pattern to DAW");
        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&midiButton),
                            [safe] (int result)
                            {
                                if (safe == nullptr)
                                    return;
                                if (result == 1)
                                    safe->exportMidiFile();
                                else if (result == 2)
                                    safe->dragMidiFile();
                            });
    }

    void SequencerPanel::exportMidiFile()
    {
        if (data == nullptr)
            return;
        fileChooser = std::make_unique<juce::FileChooser> (
            "Export sequencer pattern as MIDI",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                .getChildFile ("SliceForge Pattern.mid"),
            "*.mid");
        auto safe = juce::Component::SafePointer<SequencerPanel> (this);
        fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [safe] (const juce::FileChooser& chooser)
                                  {
                                      if (safe != nullptr)
                                          safe->writeMidiToFile (chooser.getResult());
                                  });
    }

    void SequencerPanel::dragMidiFile()
    {
        if (data == nullptr)
            return;
        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("SKEER SliceForge Pattern.mid");
        writeMidiToFile (file);
        if (! juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() },
                                                                           false, this))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "SliceForge MIDI",
                                                    "The MIDI file was written to "
                                                        + file.getFullPathName()
                                                        + "\n\nThis editor cannot start an external drag.");
        }
    }

    void SequencerPanel::writeMidiToFile (const juce::File& file)
    {
        if (data == nullptr || ! file.getFullPathName().isNotEmpty())
            return;
        auto target = file.hasFileExtension ("mid") ? file : file.withFileExtension ("mid");
        auto exportData = *data;
        if (exportData.hostSync)
            exportData.bpm = processorRef.getSequencerHostBpm();
        const auto block = sliceforge::makePatternMidiFile (exportData);
        if (! target.replaceWithData (block.getData(), block.getSize()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "SliceForge MIDI",
                                                    "Could not write " + target.getFullPathName());
    }

    void SequencerPanel::setSongMode (bool enabled)
    {
        if (data == nullptr)
            return;
        edit ([enabled] (sliceforge::SequencerData& d) { d.songMode = enabled; }, true);
    }

    void SequencerPanel::cycleChainSlot (int slot)
    {
        if (data == nullptr || slot < 0 || slot >= sliceforge::kSeqChainLength)
            return;
        const int next = (data->chainPatterns[(size_t) slot] + 1) % sliceforge::kSeqPatterns;
        edit ([slot, next] (sliceforge::SequencerData& d) { d.chainPatterns[(size_t) slot] = next; }, true);
    }

    int SequencerPanel::laneLengthForSelected() const
    {
        if (data == nullptr)
            return 0;
        const auto active = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
        return data->patterns[(size_t) active].laneLength[(size_t) selectedLane];
    }

    void SequencerPanel::setLaneLength (int length)
    {
        if (data == nullptr)
            return;
        length = juce::jlimit (0, sliceforge::kSeqSteps, length);
        edit ([this, length] (sliceforge::SequencerData& d)
              {
                  const auto active = juce::jlimit (0, sliceforge::kSeqPatterns - 1, d.activePattern);
                  d.patterns[(size_t) active].laneLength[(size_t) selectedLane] = length;
              }, true);
    }

    void SequencerPanel::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        drawPanelBackground (g, getLocalBounds().toFloat(), true);
        drawSectionLabel (g, { 13, 8 }, "SEQUENCER");

        if (data != nullptr)
        {
            const auto playing = processorRef.isSequencerTransportPlaying();
            const auto mode = data->hostSync ? "HOST SYNC" : "INTERNAL";
            g.setColour (c.muted);
            g.setFont (font (6.8f, true));
            g.drawText (juce::String (mode) + "  /  " + juce::String (processorRef.getSequencerHostBpm(), 1) + " BPM",
                        948, 8, 274, 12, juce::Justification::centredRight, false);
            g.setColour (playing ? c.green : c.muted);
            g.drawText (playing ? "PLAYING" : "STOPPED", 948, 21, 274, 12,
                        juce::Justification::centredRight, false);

            g.setColour (c.muted);
            g.setFont (font (6.8f, true));
            g.drawText (juce::String (data->length) + " STEPS  /  " + sliceforge::divisionName (data->division)
                            + "  /  SWING " + juce::String ((int) std::round (data->swing * 100.0f)) + "%"
                            + (data->songMode ? "  /  SONG" : ""),
                        948, 34, 274, 12, juce::Justification::centredRight, false);

            const auto active = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
            g.setColour (c.muted);
            g.setFont (font (6.8f, true));
            g.drawText ("PATTERN " + juce::String::charToString ((juce::juce_wchar) ('A' + active))
                            + "  /  LANE " + sliceforge::padNotes()[(size_t) selectedLane],
                        948, 708, 274, 12, juce::Justification::centredRight, false);
        }

        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("BPM", 320, 24, 30, 12, juce::Justification::centredLeft, false);
        g.drawText ("SWING", 492, 24, 38, 12, juce::Justification::centredLeft, false);
        g.drawText ("STEPS", 664, 24, 38, 12, juce::Justification::centredLeft, false);
        g.drawText ("DIV", 824, 24, 30, 12, juce::Justification::centredLeft, false);
        g.drawText ("LANE LOOP", 926, 64, 58, 12, juce::Justification::centredLeft, false);

        g.setColour (c.line);
        g.fillRect (12.0f, 652.0f, (float) getWidth() - 24.0f, 1.0f);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("CHAIN", 14, 658, 44, 12, juce::Justification::centredLeft, false);
        g.drawText ("DRAW / DRAG TO PAINT  /  RIGHT-CLICK A STEP FOR NUDGE, FLAM & PARAMETER LOCKS",
                    14, 680, 720, 12, juce::Justification::centredLeft, false);
        g.drawText ("VELOCITY / PROBABILITY / RATCHET / NUDGE / FLAM  /  DRAG VERTICALLY"
                        "  /  COPY TO OR RIGHT-CLICK A PATTERN LETTER TO DUPLICATE IT",
                    14, 694, 1100, 12, juce::Justification::centredLeft, false);
        g.drawText ("SONG MODE PLAYS THE CHAIN SLOTS  /  CLICK SLOTS TO CYCLE  /  SPACE PLAY-STOP  /  G RANDOM",
                    14, 708, 760, 12, juce::Justification::centredLeft, false);
    }

    void SequencerPanel::resized()
    {
        for (int i = 0; i < sliceforge::kSeqPatterns; ++i)
            patternButtons[(size_t) i].setBounds (14 + i * 36, 26, 32, 24);

        playButton.setBounds (170, 24, 68, 26);
        hostButton.setBounds (244, 24, 62, 26);

        bpmSlider.setBounds (352, 20, 128, 30);
        swingSlider.setBounds (534, 20, 118, 30);
        lengthSlider.setBounds (704, 20, 104, 30);
        divisionBox.setBounds (854, 21, 86, 26);

        modeButtons[0].setBounds (14, 60, 52, 24);
        modeButtons[1].setBounds (72, 60, 52, 24);
        modeButtons[2].setBounds (130, 60, 54, 24);
        modeButtons[3].setBounds (190, 60, 60, 24);
        modeButtons[4].setBounds (256, 60, 54, 24);
        modeButtons[5].setBounds (316, 60, 44, 24);

        randomButton.setBounds (370, 60, 66, 24);
        clearButton.setBounds (442, 60, 52, 24);
        copyButton.setBounds (500, 60, 48, 24);
        pasteButton.setBounds (554, 60, 48, 24);
        laneRandomButton.setBounds (608, 60, 66, 24);
        euclidButton.setBounds (680, 60, 54, 24);
        songButton.setBounds (740, 60, 50, 24);
        patternPresetButton.setBounds (796, 60, 76, 24);
        midiButton.setBounds (878, 60, 42, 24);
        laneLengthBox.setBounds (986, 60, 120, 24);
        copyToButton.setBounds (getWidth() - 116, 60, 104, 24);

        for (int i = 0; i < sliceforge::kSeqChainLength; ++i)
            chainButtons[(size_t) i].setBounds (62 + i * 33, 654, 30, 20);

        grid.setBounds (12, 94, getWidth() - 24, getHeight() - 94 - 76);
    }
} // namespace sliceforge::ui
