#include "VaultPanel.h"
#include "Theme.h"
#include "Controls.h"

#include <cmath>
#include <iterator>

namespace sliceforge::ui
{
    namespace
    {
        constexpr int kRowHeight = 34;
        constexpr int kDetailWidth = 394;
        constexpr int kDetailGap = 12;
        constexpr int kTop = 78;
        constexpr int kBottomMargin = 12;
        constexpr int kButtonHeight = 30;
        constexpr int kPreviewTop = 300;
        constexpr int kPreviewChipHeight = 24;
        constexpr int kPreviewChipGap = 8;
        constexpr int kStallLimitPolls = 24;

        // One source of truth for the audition row so paint() and resized() cannot drift.
        struct PreviewRow
        {
            juce::Rectangle<int> label, replay, loop, stop, status;
        };

        PreviewRow previewRowFor (juce::Rectangle<int> detail)
        {
            const auto x = detail.getX() + 14;
            const auto y = detail.getY() + kPreviewTop;
            PreviewRow row;
            row.label = { x, y, detail.getWidth() - 28, 11 };
            row.replay = { x, y + 15, 92, kPreviewChipHeight };
            row.loop = { x + 92 + kPreviewChipGap, y + 15, 78, kPreviewChipHeight };
            row.stop = { x + 92 + 78 + kPreviewChipGap * 2, y + 15, 66, kPreviewChipHeight };
            row.status = { x, y + 15 + kPreviewChipHeight + 6, detail.getWidth() - 28, 12 };
            return row;
        }

        const char* familyValues[] { "drums", "tonal", "fx", "texture", "vocal", "recording", "unknown" };
        const char* familyLabels[] { "DRUMS", "TONAL", "FX", "TEXTURE", "VOCAL", "RECORDING", "UNKNOWN" };
        const char* typeValues[] { "one_shot", "loop", "long", "unknown" };
        const char* typeLabels[] { "ONE SHOT", "LOOP", "LONG", "UNKNOWN" };

        void styleVaultCombo (juce::ComboBox&) {}

        void styleVaultEditor (juce::TextEditor& editor)
        {
            editor.setColour (juce::TextEditor::backgroundColourId, palette().panel);
            editor.setColour (juce::TextEditor::textColourId, palette().ink);
            editor.setColour (juce::TextEditor::outlineColourId, palette().line);
            editor.setColour (juce::TextEditor::focusedOutlineColourId, palette().accent);
            editor.setColour (juce::TextEditor::highlightColourId, palette().accent.withAlpha (0.22f));
            editor.setColour (juce::TextEditor::highlightedTextColourId, palette().ink);
            editor.setFont (font (11.0f));
            editor.setIndents (10, 7);
            editor.setJustification (juce::Justification::centredLeft);
        }
    } // namespace

    void VaultPanel::WaveformView::setPeaks (std::vector<float> newMin, std::vector<float> newMax)
    {
        minimum = std::move (newMin);
        maximum = std::move (newMax);
        repaint();
    }

    void VaultPanel::WaveformView::setProgress (double playhead)
    {
        const auto next = juce::jlimit (-1.0, 1.0, playhead);
        if (std::abs (next - head) < 0.0015)
            return;
        head = next;
        repaint();
    }

    void VaultPanel::WaveformView::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);

        if (minimum.empty() || maximum.empty())
        {
            g.setColour (c.muted);
            g.setFont (font (7.2f, true));
            g.drawText ("NO INDEXED WAVEFORM YET", getLocalBounds(), juce::Justification::centred, false);
            return;
        }

        const auto centre = (float) getHeight() * 0.5f;
        const auto amplitude = (float) getHeight() * 0.42f;
        g.setColour (c.line);
        g.fillRect (0.0f, centre, (float) getWidth(), 1.0f);

        const auto count = juce::jmin (minimum.size(), maximum.size());
        const auto columns = juce::jmax (1, juce::jmin (getWidth(), (int) count));
        g.setColour (c.accent.withAlpha (0.85f));
        for (int i = 0; i < columns; ++i)
        {
            const auto source = (size_t) ((double) i * (double) count / (double) columns);
            const auto lo = juce::jlimit (-1.0f, 1.0f, minimum[juce::jmin (source, count - 1)]);
            const auto hi = juce::jlimit (-1.0f, 1.0f, maximum[juce::jmin (source, count - 1)]);
            const auto x = (float) i;
            g.drawLine (x, centre - hi * amplitude, x, centre - lo * amplitude, 1.0f);
        }

        if (head >= 0.0)
        {
            const auto x = juce::jlimit (0.0f, (float) getWidth() - 1.0f,
                                         (float) head * (float) (getWidth() - 1));
            g.setColour (c.ink.withAlpha (0.85f));
            g.fillRect (x, 1.0f, 1.0f, (float) getHeight() - 2.0f);
            g.setColour (c.ink);
            g.fillRect (x - 1.0f, 0.0f, 3.0f, 3.0f);
        }
    }

    VaultPanel::VaultPanel (SliceForgeAudioProcessor& processor)
        : processorRef (processor),
          searchTimer ([this] { rebuildResults(); })
    {
        addAndMakeVisible (search);
        search.setTextToShowWhenEmpty ("SEARCH SAMPLES, FOLDERS OR TAGS...", palette().muted);
        search.setSelectAllWhenFocused (true);
        styleVaultEditor (search);
        search.onTextChange = [this] { startSearchTimer(); };

        familyBox.addItem ("ALL FAMILIES", 1);
        for (int i = 0; i < (int) std::size (familyValues); ++i)
            familyBox.addItem (familyLabels[i], i + 2);
        familyBox.setSelectedId (1, juce::dontSendNotification);
        styleVaultCombo (familyBox);
        addAndMakeVisible (familyBox);

        typeBox.addItem ("ALL TYPES", 1);
        for (int i = 0; i < (int) std::size (typeValues); ++i)
            typeBox.addItem (typeLabels[i], i + 2);
        typeBox.setSelectedId (1, juce::dontSendNotification);
        styleVaultCombo (typeBox);
        addAndMakeVisible (typeBox);

        collectionBox.addItem ("ALL COLLECTIONS", 1);
        collectionBox.setSelectedId (1, juce::dontSendNotification);
        styleVaultCombo (collectionBox);
        addAndMakeVisible (collectionBox);

        sortBox.addItem ("SORT: NAME", 1);
        sortBox.addItem ("SORT: RECENT", 2);
        sortBox.addItem ("SORT: DURATION", 3);
        sortBox.addItem ("SORT: BPM", 4);
        sortBox.setSelectedId (1, juce::dontSendNotification);
        styleVaultCombo (sortBox);
        addAndMakeVisible (sortBox);

        for (auto* button : { &favoritesButton, &refreshButton, &locateButton, &folderButton,
                              &previewButton, &stopButton })
            styleChip (*button, false);
        stylePrimaryButton (loadButton);
        addAndMakeVisible (favoritesButton);
        addAndMakeVisible (refreshButton);
        addAndMakeVisible (locateButton);
        addAndMakeVisible (loadButton);
        addAndMakeVisible (folderButton);
        favoritesButton.setClickingTogglesState (true);
        favoritesButton.onClick = [this]
        {
            if (! updating)
                rebuildResults();
        };
        refreshButton.onClick = [this] { refreshStatus(); };
        locateButton.onClick = [this] { locateDatabase(); };
        loadButton.onClick = [this] { loadSelected(); };
        folderButton.onClick = [this] { revealSelectedFolder(); };

        loopButton.setClickingTogglesState (true);
        addAndMakeVisible (previewButton);
        addAndMakeVisible (loopButton);
        addAndMakeVisible (stopButton);
        previewButton.onClick = [this] { restartPreview(); };
        loopButton.onClick = [this]
        {
            auto& player = processorRef.getPreviewPlayer();
            player.setLoopEnabled (loopButton.getToggleState());
            if (loopButton.getToggleState() && ! player.isPlaying() && previewSample != nullptr)
                previewButton.triggerClick();
        };
        stopButton.onClick = [this] { stopPreview ("STOPPED  •  CLICK A SAMPLE TO AUDITION IT AGAIN"); };
        for (auto* button : { &previewButton, &loopButton, &stopButton })
        {
            button->setEnabled (false);
            button->setVisible (false);
        }

        addAndMakeVisible (list);
        list.setRowHeight (kRowHeight);
        list.setOutlineThickness (1);
        list.setMultipleSelectionEnabled (false);
        addAndMakeVisible (waveform);

        familyBox.onChange = [this] { if (! updating) rebuildResults(); };
        typeBox.onChange = [this] { if (! updating) rebuildResults(); };
        collectionBox.onChange = [this] { if (! updating) rebuildResults(); };
        sortBox.onChange = [this] { if (! updating) rebuildResults(); };

        loadButton.setEnabled (false);
        folderButton.setEnabled (false);
        search.setEnabled (false);
        familyBox.setEnabled (false);
        typeBox.setEnabled (false);
        collectionBox.setEnabled (false);
        sortBox.setEnabled (false);
        favoritesButton.setEnabled (false);
        statusLine = "SKEER SAMPLE VAULT";
        statusDetail = "OPEN THE VAULT TAB TO CONNECT";
        setActive (false);
    }

    VaultPanel::~VaultPanel()
    {
        stopTimer();
        searchTimer.stopTimer();
        processorRef.getPreviewPlayer().stop();
        previewPool.removeAllJobs (true, 0);
    }

    void VaultPanel::setActive (bool shouldBeActive)
    {
        active = shouldBeActive;
        if (active)
        {
            refreshStatus();
        }
        else
        {
            abandonPreview();
        }
    }

    void VaultPanel::refreshTheme()
    {
        const auto& c = palette();
        search.setColour (juce::TextEditor::backgroundColourId, c.panel);
        search.setColour (juce::TextEditor::textColourId, c.ink);
        search.setColour (juce::TextEditor::outlineColourId, c.line);
        search.setColour (juce::TextEditor::focusedOutlineColourId, c.accent);
        search.setColour (juce::TextEditor::highlightColourId, c.accent.withAlpha (0.22f));
        search.setColour (juce::TextEditor::highlightedTextColourId, c.ink);
        search.setTextToShowWhenEmpty ("SEARCH SAMPLES, FOLDERS OR TAGS...", c.muted);
        search.applyColourToAllText (c.ink, true);
        search.sendLookAndFeelChange();
        search.repaint();
        list.repaint();
        repaint();
    }

    void VaultPanel::selectFirstRowForTesting()
    {
        if (tracks.empty())
            return;
        // ListBox::selectRow always calls back into selectedRowsChanged, and there is no way
        // to ask it not to, so the audition is switched off around the selection instead.
        previewSuppressed = true;
        selectedRow = 0;
        list.selectRow (0);
        updateDetails();
        previewSuppressed = false;
    }

    void VaultPanel::selectFirstAnalysedRowForTesting()
    {
        previewSuppressed = true;
        for (int i = 0; i < (int) tracks.size(); ++i)
        {
            if (! tracks[(size_t) i].analyzed)
                continue;
            selectedRow = i;
            list.selectRow (i);
            updateDetails();
            previewSuppressed = false;
            return;
        }
        previewSuppressed = false;
        selectFirstRowForTesting();
    }

    void VaultPanel::searchForTesting (const juce::String& text)
    {
        search.setText (text, juce::dontSendNotification);
        rebuildResults();
    }

    void VaultPanel::startSearchTimer()
    {
        // A juce::Timer of its own: the audition playhead polls on the component timer at
        // 20 Hz, and the two must not cancel each other out.
        searchTimer.startTimer (220);
    }

    void VaultPanel::timerCallback()
    {
        pollPreview();
    }

    juce::Rectangle<int> VaultPanel::detailRect() const
    {
        return { getWidth() - 14 - kDetailWidth, kTop, kDetailWidth,
                 getHeight() - kTop - kBottomMargin };
    }

    void VaultPanel::restartPreview()
    {
        if (previewSample == nullptr)
        {
            beginPreview();
            return;
        }
        auto& player = processorRef.getPreviewPlayer();
        player.play (previewSample, loopButton.getToggleState());
        previewWasPlaying = true;
        previewStamp = player.getAdvanceStamp();
        previewStallPolls = 0;
        previewStatus = "AUDITIONING  •  " + details.filename;
        startTimerHz (20);
        repaint();
    }

    void VaultPanel::beginPreview()
    {
        auto& player = processorRef.getPreviewPlayer();
        auto* track = selectedTrack();

        if (! active || track == nullptr || track->path.isEmpty())
        {
            stopPreview();
            return;
        }

        // Re-clicking the row that is already auditioned replays the decode we already hold.
        if (previewSample != nullptr && previewedTrackId == track->id)
        {
            restartPreview();
            return;
        }

        const juce::File file (track->path);
        player.stop();
        stopTimer();
        previewWasPlaying = false;
        previewSample.reset();
        previewedTrackId = track->id;
        waveform.setProgress (-1.0);
        previewPool.removeAllJobs (true, 0);

        if (! file.existsAsFile())
        {
            previewedTrackId = 0;
            previewStatus = "FILE IS MISSING  •  " + file.getFileName();
            repaint();
            return;
        }

        previewStatus = "DECODING...";
        const auto generation = previewGeneration->fetch_add (1) + 1;
        auto counter = previewGeneration;
        auto safe = juce::Component::SafePointer<VaultPanel> (this);
        previewPool.addJob ([counter, generation, file, safe]
        {
            if (counter->load() != generation)
                return;
            auto decoded = sliceforge::loadAudioFile (file);
            juce::MessageManager::callAsync ([safe, generation, decoded]
            {
                if (safe != nullptr)
                    safe->finishPreview (generation, decoded);
            });
        });
        repaint();
    }

    void VaultPanel::finishPreview (unsigned generation, sliceforge::SamplePtr decoded)
    {
        if (! active || generation != previewGeneration->load())
            return;

        if (decoded == nullptr || decoded->getNumSamples() < 2)
        {
            previewSample.reset();
            previewedTrackId = 0;
            previewStatus = "SLICEFORGE COULD NOT DECODE THIS FILE";
            repaint();
            return;
        }

        previewSample = std::move (decoded);
        waveform.setPeaks (previewSample->peakMin, previewSample->peakMax);
        restartPreview();
    }

    void VaultPanel::pollPreview()
    {
        auto& player = processorRef.getPreviewPlayer();

        if (! player.isPlaying())
        {
            stopTimer();
            waveform.setProgress (-1.0);
            if (previewWasPlaying)
                previewStatus = "PLAYED ONCE  •  TURN ON LOOP TO HEAR IT REPEATEDLY";
            previewWasPlaying = false;
            repaint();
            return;
        }

        const auto stamp = player.getAdvanceStamp();
        if (stamp == previewStamp)
        {
            // A VST3 is only asked to render while the host wants sound. Clicking in the
            // vault with the transport stopped and monitoring off would otherwise leave a
            // playhead frozen mid-file and audio arriving late, so say what happened.
            if (++previewStallPolls <= kStallLimitPolls)
                return;
            player.stop();
            stopTimer();
            previewWasPlaying = false;
            waveform.setProgress (-1.0);
            previewStatus = "THE HOST IS NOT PROCESSING AUDIO  •  ARM OR PLAY THE TRACK";
            repaint();
            return;
        }

        previewStallPolls = 0;
        previewStamp = stamp;
        previewStatus = "AUDITION  " + juce::String (player.getElapsedSeconds(), 2) + " s  /  "
                            + juce::String (player.getDurationSeconds(), 2) + " s"
                            + (player.isLoopEnabled() ? "  •  LOOPING" : "");
        waveform.setProgress (player.getProgress());
        repaint (detailRect());
    }

    void VaultPanel::stopPreview (const juce::String& idleStatus)
    {
        processorRef.getPreviewPlayer().stop();
        previewWasPlaying = false;
        previewStallPolls = 0;
        stopTimer();
        waveform.setProgress (-1.0);
        if (! idleStatus.isEmpty())
            previewStatus = idleStatus;
        repaint();
    }

    void VaultPanel::abandonPreview()
    {
        stopPreview ("CLICK A SAMPLE TO AUDITION IT");
        // Bump the generation first so any decode still queued or running disowns itself
        // before the pool is drained, then drop the buffer: a filtered-out file should not
        // keep up to ten minutes of audio resident.
        previewGeneration->fetch_add (1);
        previewPool.removeAllJobs (true, 0);
        previewSample.reset();
        previewedTrackId = 0;
    }

    void VaultPanel::refreshStatus()
    {
        vault.refreshStatus();
        statusLine = vault.isAvailable()
            ? "SKEER SAMPLE VAULT  •  " + juce::String (vault.getStatus().totalTracks) + " INDEXED SAMPLES"
            : "SKEER SAMPLE VAULT NOT CONNECTED";
        statusDetail = vault.isAvailable()
            ? juce::String (vault.getStatus().analyzedTracks) + " ANALYSED  •  "
                  + juce::String (vault.getStatus().favoriteTracks) + " FAVOURITES  •  "
                  + vault.getStatus().database.getFullPathName()
            : vault.getStatus().error;

        search.setEnabled (vault.isAvailable());
        familyBox.setEnabled (vault.isAvailable());
        typeBox.setEnabled (vault.isAvailable());
        collectionBox.setEnabled (vault.isAvailable());
        sortBox.setEnabled (vault.isAvailable());
        favoritesButton.setEnabled (vault.isAvailable());
        refreshButton.setEnabled (true);
        locateButton.setEnabled (true);

        rebuildCollections();
        rebuildResults();
        repaint();
    }

    void VaultPanel::rebuildCollections()
    {
        const auto previous = collectionBox.getSelectedId();
        updating = true;
        collectionBox.clear (juce::dontSendNotification);
        collectionBox.addItem ("ALL COLLECTIONS", 1);
        if (vault.isAvailable())
        {
            for (const auto& collection : vault.queryCollections())
                collectionBox.addItem (collection.name, 1000 + collection.id);
        }
        collectionBox.setSelectedId (previous > 0 ? previous : 1, juce::dontSendNotification);
        if (collectionBox.getSelectedId() <= 0)
            collectionBox.setSelectedId (1, juce::dontSendNotification);
        updating = false;
    }

    void VaultPanel::rebuildResults()
    {
        // A new result set drops the selection, so the audition that belonged to it goes too.
        abandonPreview();
        if (! vault.isAvailable())
        {
            tracks.clear();
            details = {};
            selectedRow = -1;
            list.updateContent();
            list.repaint();
            loadButton.setEnabled (false);
            folderButton.setEnabled (false);
            for (auto* button : { &previewButton, &loopButton, &stopButton })
            {
                button->setEnabled (false);
                button->setVisible (false);
            }
            waveform.setPeaks ({}, {});
            repaint();
            return;
        }

        VaultQuery query;
        query.search = search.getText().trim();
        const auto familyId = familyBox.getSelectedId();
        if (familyId >= 2 && familyId - 2 < (int) std::size (familyValues))
            query.family = familyValues[familyId - 2];
        const auto typeId = typeBox.getSelectedId();
        if (typeId >= 2 && typeId - 2 < (int) std::size (typeValues))
            query.soundType = typeValues[typeId - 2];
        const auto collectionId = collectionBox.getSelectedId();
        query.collectionId = collectionId >= 1000 ? collectionId - 1000 : 0;
        query.favoritesOnly = favoritesButton.getToggleState();
        query.sortMode = juce::jmax (0, sortBox.getSelectedId() - 1);
        query.limit = 500;

        juce::String error;
        tracks = vault.queryTracks (query, &error);
        selectedRow = -1;
        details = {};
        list.deselectAllRows();
        list.updateContent();
        if (! tracks.empty())
            list.scrollToEnsureRowIsOnscreen (0);
        loadButton.setEnabled (false);
        folderButton.setEnabled (false);
        for (auto* button : { &previewButton, &loopButton, &stopButton })
        {
            button->setEnabled (false);
            button->setVisible (false);
        }
        waveform.setPeaks ({}, {});
        if (tracks.empty() && vault.isAvailable())
            statusDetail = "NO MATCHING SAMPLES  •  " + vault.getStatus().database.getFullPathName();
        repaint();
    }

    int VaultPanel::getNumRows()
    {
        return (int) tracks.size();
    }

    VaultTrack* VaultPanel::selectedTrack()
    {
        if (selectedRow < 0 || selectedRow >= (int) tracks.size())
            return nullptr;
        return &tracks[(size_t) selectedRow];
    }

    juce::String VaultPanel::formatDuration (double milliseconds) const
    {
        if (milliseconds <= 0.0)
            return "—";
        const auto seconds = milliseconds / 1000.0;
        if (seconds < 10.0)
            return juce::String (seconds, 2) + " s";
        const auto minutes = (int) (seconds / 60.0);
        const auto remaining = (int) std::round (seconds - minutes * 60.0);
        return juce::String (minutes) + ":" + juce::String (remaining).paddedLeft ('0', 2);
    }

    juce::String VaultPanel::formatKey (const VaultTrack& track) const
    {
        if (track.keyRoot.isEmpty())
            return {};
        auto key = track.keyRoot;
        if (track.keyMode.isNotEmpty())
            key += " " + track.keyMode;
        return key;
    }

    void VaultPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
    {
        if (rowNumber < 0 || rowNumber >= (int) tracks.size())
            return;
        const auto& c = palette();
        const auto& track = tracks[(size_t) rowNumber];
        const auto bounds = juce::Rectangle<int> (0, 0, width, height);
        g.setColour (rowIsSelected ? c.accent.withAlpha (0.14f)
                                   : (rowNumber % 2 == 0 ? c.panel : c.bg));
        g.fillRect (bounds);
        if (rowIsSelected)
        {
            g.setColour (c.accent);
            g.fillRect (0, 0, 3, height);
        }

        g.setColour (track.analyzed ? c.green : c.lineStrong);
        g.fillEllipse (10.0f, 9.0f, 4.0f, 4.0f);

        g.setColour (c.ink);
        g.setFont (font (10.0f, true));
        auto title = track.filename;
        g.drawText (title, 20, 2, width - 70, 14, juce::Justification::centredLeft, true);
        if (track.favorite)
        {
            g.setColour (c.accent);
            g.setFont (font (11.0f, true));
            g.drawText ("★", width - 24, 1, 16, 15, juce::Justification::centredRight, false);
        }

        juce::StringArray meta;
        if (track.family.isNotEmpty())
            meta.add (track.family.toUpperCase());
        if (track.soundType.isNotEmpty())
            meta.add (track.soundType.replaceCharacter ('_', ' ').toUpperCase());
        if (track.bpm > 0.0)
            meta.add (juce::String (track.bpm, 1) + " BPM");
        const auto key = formatKey (track);
        if (key.isNotEmpty())
            meta.add (key);
        meta.add (track.durationMs > 0.0 ? formatDuration (track.durationMs) : "NO ANALYSIS");
        g.setColour (c.muted);
        g.setFont (font (7.0f, true));
        g.drawText (meta.joinIntoString ("  /  "), 20, 18, width - 28, 12,
                    juce::Justification::centredLeft, true);
    }

    void VaultPanel::selectedRowsChanged (int lastRowSelected)
    {
        selectedRow = lastRowSelected;
        updateDetails();

        // Autoplay lands here rather than in updateDetails(): the detail refresh is also
        // driven by the layout test and by filter rebuilds, and neither should make sound.
        if (active && ! previewSuppressed && vault.isAvailable() && lastRowSelected >= 0)
            beginPreview();
    }

    void VaultPanel::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
    {
        selectedRow = row;
        list.selectRow (row);
        updateDetails();
        loadSelected();
    }

    void VaultPanel::updateDetails()
    {
        auto* track = selectedTrack();
        const bool canPreview = track != nullptr;
        loadButton.setEnabled (track != nullptr);
        folderButton.setEnabled (track != nullptr);
        for (auto* button : { &previewButton, &loopButton, &stopButton })
        {
            button->setVisible (canPreview);
            button->setEnabled (canPreview);
        }
        if (track == nullptr)
        {
            details = {};
            waveform.setPeaks ({}, {});
            repaint();
            return;
        }

        VaultTrack loaded;
        if (vault.queryTrackDetails (track->id, loaded))
            details = std::move (loaded);
        else
            details = *track;

        // The decoded audition is the only waveform that carries a truthful playhead, so the
        // indexed peaks stay on screen until that file has been read.
        if (previewedTrackId != details.id)
        {
            waveform.setPeaks (details.waveformMin, details.waveformMax);
            waveform.setProgress (-1.0);
        }
        repaint();
    }

    void VaultPanel::loadSelected()
    {
        auto* track = selectedTrack();
        if (track == nullptr)
            return;
        stopPreview ("LOADED INTO THE KIT");
        const juce::File file (track->path);
        if (! file.existsAsFile())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "SKEER Sample Vault",
                                                    "The indexed source file is no longer available:\n"
                                                        + track->path);
            return;
        }

        const auto name = track->filename;
        if (processorRef.loadUserSampleFromFile (file))
        {
            statusDetail = "LOADED  •  " + name;
            list.repaint();
            repaint();
            if (onSampleLoaded)
                onSampleLoaded();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "SKEER Sample Vault",
                                                    "SliceForge could not load " + name + ".");
        }
    }

    void VaultPanel::revealSelectedFolder()
    {
        auto* track = selectedTrack();
        if (track == nullptr)
            return;
        const juce::File file (track->path);
        if (file.existsAsFile())
            file.revealToUser();
    }

    void VaultPanel::locateDatabase()
    {
        auto start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        const auto local = juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});
        if (local.isNotEmpty())
            start = juce::File (local).getChildFile ("SKEER Sample Vault").getChildFile ("data");
        fileChooser = std::make_unique<juce::FileChooser> (
            "Locate SKEER Sample Vault database (vault.sqlite3)", start, "*.sqlite3");
        auto safe = juce::Component::SafePointer<VaultPanel> (this);
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [safe] (const juce::FileChooser& chooser)
                                  {
                                      if (safe == nullptr)
                                          return;
                                      auto file = chooser.getResult();
                                      if (! file.existsAsFile())
                                          return;
                                      juce::String error;
                                      if (! safe->vault.setDatabaseFile (file, error))
                                      {
                                          juce::AlertWindow::showMessageBoxAsync (
                                              juce::MessageBoxIconType::WarningIcon,
                                              "SKEER Sample Vault", error);
                                          return;
                                      }
                                      safe->refreshStatus();
                                  });
    }

    void VaultPanel::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        drawPanelBackground (g, getLocalBounds().toFloat(), true);
        drawSectionLabel (g, { 13, 8 }, "SKEER SAMPLE VAULT", "SAMPLE MANAGER LINK");

        g.setColour (vault.isAvailable() ? c.muted : c.red);
        g.setFont (font (6.8f, true));
        g.drawText (statusLine, getWidth() - 700, 7, 686, 12,
                    juce::Justification::centredRight, true);
        g.setColour (c.muted);
        g.setFont (mono (6.2f));
        g.drawText (statusDetail, getWidth() - 1000, 21, 986, 11,
                    juce::Justification::centredRight, true);

        auto listBounds = juce::Rectangle<int> (14, kTop, getWidth() - 28 - kDetailWidth - kDetailGap,
                                                getHeight() - kTop - kBottomMargin);
        auto detailBounds = juce::Rectangle<int> (getWidth() - 14 - kDetailWidth, kTop, kDetailWidth,
                                                  getHeight() - kTop - kBottomMargin);
        drawInsetBackground (g, listBounds.toFloat());
        drawInsetBackground (g, detailBounds.toFloat());

        if (details.id == 0)
        {
            g.setColour (c.muted);
            g.setFont (font (10.0f, true));
            g.drawText (vault.isAvailable() ? "SELECT A SAMPLE" : "SAMPLE VAULT NOT CONNECTED",
                        detailBounds.reduced (16), juce::Justification::centred, false);
            g.setFont (font (7.2f));
            g.drawFittedText (vault.isAvailable()
                                  ? "Browse the index on the left. Double-click or use LOAD SAMPLE to pull it into the current SliceForge kit."
                                  : "Install or open SKEER Sample Vault once, then use LOCATE DB if the index is in a custom location.",
                              detailBounds.reduced (18).withTrimmedTop (20),
                              juce::Justification::centredTop, 4);
            return;
        }

        const auto title = detailBounds.reduced (14, 10).removeFromTop (32);
        g.setColour (c.ink);
        g.setFont (font (13.0f, true));
        g.drawFittedText (details.filename, title, juce::Justification::topLeft, 2);

        juce::String sub;
        if (details.family.isNotEmpty())
            sub += details.family.toUpperCase();
        if (details.soundType.isNotEmpty())
            sub += (sub.isNotEmpty() ? "  /  " : "") + details.soundType.replaceCharacter ('_', ' ').toUpperCase();
        if (details.favorite)
            sub += "  /  ★ FAVOURITE";
        g.setColour (c.muted);
        g.setFont (font (7.2f, true));
        g.drawText (sub, detailBounds.getX() + 14, detailBounds.getY() + 44,
                    detailBounds.getWidth() - 28, 14, juce::Justification::centredLeft, true);

        g.setColour (c.line);
        g.fillRect (detailBounds.getX() + 14.0f, detailBounds.getY() + 62.0f,
                    (float) detailBounds.getWidth() - 28.0f, 1.0f);

        juce::String stats;
        stats << (details.bpm > 0.0 ? juce::String (details.bpm, 1) + " BPM  •  " : "")
              << (formatKey (details).isNotEmpty() ? formatKey (details) + "  •  " : "")
              << formatDuration (details.durationMs);
        g.setColour (c.inkSoft);
        g.setFont (mono (7.2f));
        g.drawText (stats, detailBounds.getX() + 14, detailBounds.getY() + 70,
                    detailBounds.getWidth() - 28, 14, juce::Justification::centredLeft, true);

        g.setColour (c.muted);
        g.setFont (font (6.8f));
        juce::String technical;
        technical << (details.format.isNotEmpty() ? details.format.toUpperCase() : "AUDIO")
                  << (details.sampleRate > 0 ? "  •  " + juce::String (details.sampleRate / 1000.0, 1) + " kHz" : "")
                  << (details.channels > 0 ? "  •  " + juce::String (details.channels) + " ch" : "")
                  << (details.analyzed ? "  •  INDEXED" : "  •  QUEUED FOR ANALYSIS");
        g.drawText (technical, detailBounds.getX() + 14, detailBounds.getY() + 86,
                    detailBounds.getWidth() - 28, 14, juce::Justification::centredLeft, true);

        if (details.tags.isNotEmpty())
        {
            g.setColour (c.accent);
            g.setFont (font (6.8f, true));
            g.drawFittedText ("TAGS  " + details.tags, detailBounds.getX() + 14, detailBounds.getY() + 106,
                              detailBounds.getWidth() - 28, 18, juce::Justification::topLeft, 2);
        }

        if (details.notes.isNotEmpty())
        {
            g.setColour (c.inkSoft);
            g.setFont (font (6.8f));
            g.drawFittedText ("NOTES  " + details.notes, detailBounds.getX() + 14, detailBounds.getY() + 126,
                              detailBounds.getWidth() - 28, 30, juce::Justification::topLeft, 2);
        }

        g.setColour (c.muted);
        g.setFont (font (6.2f, true));
        g.drawText ("SOURCE PATH", detailBounds.getX() + 14, detailBounds.getY() + 160,
                    detailBounds.getWidth() - 28, 10, juce::Justification::centredLeft, false);
        g.setColour (c.inkSoft);
        g.setFont (mono (6.4f));
        g.drawFittedText (details.path, detailBounds.getX() + 14, detailBounds.getY() + 171,
                          detailBounds.getWidth() - 28, 28, juce::Justification::topLeft, 2);

        const auto row = previewRowFor (detailBounds);
        g.setColour (c.line);
        g.fillRect ((float) detailBounds.getX() + 14.0f, (float) row.label.getY() - 8,
                    (float) detailBounds.getWidth() - 28.0f, 1.0f);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("AUDITION  •  PLAYS ONCE ON CLICK", row.label.getX(), row.label.getY(),
                    row.label.getWidth(), row.label.getHeight(), juce::Justification::centredLeft, false);
        g.setColour (previewSample != nullptr ? c.inkSoft : c.muted);
        g.setFont (mono (6.6f));
        g.drawText (previewStatus, row.status.getX(), row.status.getY(),
                    row.status.getWidth(), row.status.getHeight(),
                    juce::Justification::centredLeft, true);
    }

    void VaultPanel::resized()
    {
        search.setBounds (14, 42, 330, 28);
        familyBox.setBounds (352, 42, 118, 28);
        typeBox.setBounds (476, 42, 108, 28);
        collectionBox.setBounds (590, 42, 160, 28);
        favoritesButton.setBounds (756, 42, 70, 28);
        sortBox.setBounds (832, 42, 120, 28);
        refreshButton.setBounds (958, 42, 62, 28);
        locateButton.setBounds (1026, 42, 96, 28);

        const auto listWidth = getWidth() - 28 - kDetailWidth - kDetailGap;
        const auto panelHeight = getHeight() - kTop - kBottomMargin;
        list.setBounds (14, kTop, listWidth, panelHeight);

        auto detail = juce::Rectangle<int> (getWidth() - 14 - kDetailWidth, kTop, kDetailWidth, panelHeight);
        waveform.setBounds (detail.getX() + 14, detail.getY() + 204, kDetailWidth - 28, 90);

        const auto row = previewRowFor (detail);
        previewButton.setBounds (row.replay);
        loopButton.setBounds (row.loop);
        stopButton.setBounds (row.stop);

        loadButton.setBounds (detail.getX() + 14, detail.getBottom() - kButtonHeight - 12,
                              kDetailWidth - 112, kButtonHeight);
        folderButton.setBounds (detail.getRight() - 94, detail.getBottom() - kButtonHeight - 12,
                                80, kButtonHeight);
    }
} // namespace sliceforge::ui
