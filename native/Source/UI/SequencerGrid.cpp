#include "SequencerGrid.h"
#include "Theme.h"

namespace sliceforge::ui
{
    namespace
    {
        // Sequencer family palette: vivid but still readable on the light SKEER
        // surfaces and on the dark skin.
        juce::Colour stepColourForFamily (int family)
        {
            static const juce::Colour colours[]
            {
                juce::Colour (0xffff6b35), // kick  - coral orange
                juce::Colour (0xff2f7bff), // snare - electric blue
                juce::Colour (0xff00c9a7), // hat   - mint teal
                juce::Colour (0xff7b4dff)  // clap  - violet
            };
            return colours[(size_t) juce::jlimit (0, 3, family)];
        }
    } // namespace

    SequencerGrid::SequencerGrid (SliceForgeAudioProcessor& processor) : processorRef (processor)
    {
        setInterceptsMouseClicks (true, false);
    }

    void SequencerGrid::setData (std::shared_ptr<const sliceforge::SequencerData> newData)
    {
        data = std::move (newData);
        repaint();
    }

    void SequencerGrid::setPlayheadStep (int step)
    {
        if (playheadStep == step)
            return;
        playheadStep = step;
        repaint();
    }

    void SequencerGrid::setEditMode (EditMode newMode)
    {
        mode = newMode;
        setMouseCursor (mode == EditMode::Draw ? juce::MouseCursor::PointingHandCursor
                                               : juce::MouseCursor::UpDownResizeCursor);
        repaint();
    }

    void SequencerGrid::setSelectedLane (int lane)
    {
        lane = juce::jlimit (0, sliceforge::kNumPads - 1, lane);
        if (selectedLane == lane)
            return;
        selectedLane = lane;
        repaint();
    }

    int SequencerGrid::laneLengthForPad (int pad) const
    {
        if (data == nullptr)
            return sliceforge::kSeqSteps;
        const auto patternIndex = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
        const auto& pattern = data->patterns[(size_t) patternIndex];
        const auto lane = pattern.laneLength[(size_t) juce::jlimit (0, sliceforge::kNumPads - 1, pad)];
        return lane > 0 ? juce::jlimit (1, sliceforge::kSeqSteps, lane)
                        : juce::jlimit (1, sliceforge::kSeqSteps, data->length);
    }

    int SequencerGrid::dataStepForLane (int pad, int displayStep) const
    {
        if (data == nullptr)
            return displayStep;
        const auto laneLength = laneLengthForPad (pad);
        return ((displayStep % laneLength) + laneLength) % laneLength;
    }

    void SequencerGrid::resized()
    {
        updateLayout();
    }

    void SequencerGrid::updateLayout()
    {
        cellWidth = juce::jmax (1.0f, (float) (getWidth() - labelWidth) / (float) sliceforge::kSeqSteps);
        rowHeight = juce::jmax (1.0f, (float) (getHeight() - headerHeight) / (float) sliceforge::kNumPads);
        for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
        {
            const auto y = (float) headerHeight + (float) pad * rowHeight;
            rowRects[(size_t) pad] = { 0.0f, y, (float) getWidth(), rowHeight };
            muteRects[(size_t) pad] = { (float) labelWidth - 47.0f, y + rowHeight * 0.5f - 6.0f, 19.0f, 12.0f };
            soloRects[(size_t) pad] = { (float) labelWidth - 24.0f, y + rowHeight * 0.5f - 6.0f, 19.0f, 12.0f };
        }
    }

    int SequencerGrid::padAtY (float y) const
    {
        if (y < headerHeight)
            return -1;
        return juce::jlimit (0, sliceforge::kNumPads - 1, (int) ((y - headerHeight) / juce::jmax (1.0f, rowHeight)));
    }

    int SequencerGrid::stepAtX (float x) const
    {
        if (x < labelWidth)
            return -1;
        return juce::jlimit (0, sliceforge::kSeqSteps - 1, (int) ((x - labelWidth) / juce::jmax (1.0f, cellWidth)));
    }

    juce::Rectangle<float> SequencerGrid::cellBounds (int pad, int step) const
    {
        const auto y = (float) headerHeight + (float) pad * rowHeight + 1.0f;
        const auto x = (float) labelWidth + (float) step * cellWidth + 1.0f;
        return { x, y, juce::jmax (1.0f, cellWidth - 2.0f), juce::jmax (1.0f, rowHeight - 2.0f) };
    }

    void SequencerGrid::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillAll();

        if (data == nullptr)
            return;

        const int patternIndex = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
        const auto& pattern = data->patterns[(size_t) patternIndex];
        const int length = juce::jlimit (1, sliceforge::kSeqSteps, data->length);

        // Header.
        g.setColour (c.bg);
        g.fillRect (0, 0, getWidth(), headerHeight);
        g.setColour (c.muted);
        g.setFont (font (6.8f, true));
        g.drawText ("PAD / STEP", 10, 0, labelWidth - 18, headerHeight, juce::Justification::centredLeft, false);
        for (int step = 0; step < sliceforge::kSeqSteps; ++step)
        {
            const auto x = (float) labelWidth + (float) step * cellWidth;
            const bool inLength = step < length;
            g.setColour (inLength ? (step % 4 == 0 ? c.inkSoft : c.muted) : c.lineStrong);
            g.setFont (mono (step % 4 == 0 ? 7.5f : 7.0f));
            g.drawText (juce::String (step + 1), (int) x, 0, (int) juce::jmax (1.0f, cellWidth), headerHeight,
                        juce::Justification::centred, false);
            if (step % 4 == 0)
            {
                g.setColour (c.lineStrong);
                g.fillRect (x, 0.0f, 1.0f, (float) getHeight());
            }
        }

        for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
        {
            const auto& row = rowRects[(size_t) pad];
            const auto settings = processorRef.getParameterCache().readPad (pad);
            const auto family = sliceforge::familyForModel (settings.modelIndex);
            const auto model = sliceforge::localModelForIndex (settings.modelIndex);
            const auto stepColour = stepColourForFamily (family);
            const bool laneSelected = pad == selectedLane;
            const bool muted = pattern.laneMute[(size_t) pad];

            g.setColour ((pad % 2) == 0 ? c.panel : c.bg);
            g.fillRect (row);
            if (laneSelected)
            {
                g.setColour (c.accent.withAlpha (0.07f));
                g.fillRect (row);
                g.setColour (c.accent);
                g.fillRect (0.0f, row.getY(), 2.0f, row.getHeight());
            }
            if (muted)
            {
                g.setColour (c.bgDeep.withAlpha (0.45f));
                g.fillRect (row);
            }

            g.setColour (c.line);
            g.fillRect (0.0f, row.getBottom() - 0.5f, (float) getWidth(), 1.0f);

            g.setColour (stepColour);
            g.fillEllipse (10.0f, row.getCentreY() - 2.0f, 4.0f, 4.0f);
            g.setColour (muted ? c.muted : c.ink);
            g.setFont (font (9.2f, true));
            g.drawText (sliceforge::padNotes()[(size_t) pad], 20, (int) row.getY() + 4, 36, 13,
                        juce::Justification::centredLeft, false);
            g.setColour (c.muted);
            g.setFont (font (6.8f, true));
            g.drawText (sliceforge::modelBank()[(size_t) family][(size_t) model].name, 58, (int) row.getY() + 5, 92, 12,
                        juce::Justification::centredLeft, false);
            g.setFont (mono (6.5f));
            g.drawText (sliceforge::modelBank()[(size_t) family][(size_t) model].code, 58, (int) row.getY() + 17, 50, 10,
                        juce::Justification::centredLeft, false);

            const auto laneLength = laneLengthForPad (pad);
            const int globalLength = juce::jlimit (1, sliceforge::kSeqSteps, data->length);
            if (laneLength != globalLength)
            {
                g.setColour (c.accent.withAlpha (0.7f));
                g.setFont (mono (6.5f));
                g.drawText ("L" + juce::String (laneLength), 112, (int) row.getY() + 5, 34, 11,
                            juce::Justification::centredLeft, false);
            }

            auto drawLaneToggle = [&] (juce::Rectangle<float> bounds, const juce::String& text, bool on)
            {
                g.setColour (on ? c.accent : c.panel);
                g.fillRoundedRectangle (bounds, 2.5f);
                g.setColour (on ? juce::Colour (0xfffff9f2) : c.ink);
                g.setFont (font (6.5f, true));
                g.drawText (text, bounds.toNearestInt(), juce::Justification::centred, false);
                g.setColour (c.line);
                g.drawRoundedRectangle (bounds.reduced (0.5f), 2.5f, 1.0f);
            };
            drawLaneToggle (muteRects[(size_t) pad], "M", pattern.laneMute[(size_t) pad]);
            drawLaneToggle (soloRects[(size_t) pad], "S", pattern.laneSolo[(size_t) pad]);

            const bool repeatedLane = laneLength < length;
            for (int step = 0; step < sliceforge::kSeqSteps; ++step)
            {
                const auto cell = cellBounds (pad, step);
                const bool inLength = step < length;
                const int dataStep = dataStepForLane (pad, step);
                const auto& s = pattern.rows[(size_t) pad][(size_t) dataStep];
                const bool repeatGhost = repeatedLane && step >= laneLength;

                if (! inLength)
                {
                    g.setColour (c.bgDeep.withAlpha (0.28f));
                    g.fillRoundedRectangle (cell, 2.0f);
                    continue;
                }

                if (step == playheadStep && step < length)
                {
                    g.setColour (c.green.withAlpha (0.10f));
                    g.fillRoundedRectangle (cell.expanded (0.5f, 0.0f), 2.0f);
                }

                if (s.on)
                {
                    // Solid family colours: velocity stays readable through the
                    // white fill-height bar rather than by fading the chip out.
                    const float ghostAlpha = repeatGhost ? 0.52f : 1.0f;
                    g.setColour (stepColour.withAlpha (ghostAlpha));
                    g.fillRoundedRectangle (cell, 2.0f);
                    g.setColour (stepColour.withAlpha (ghostAlpha));
                    g.drawRoundedRectangle (cell.reduced (0.5f), 2.0f, 1.0f);
                    // Keep velocity as a slim bottom accent so the family colour
                    // remains the dominant, fully opaque chip colour.
                    const auto barHeight = juce::jlimit (1.0f, 3.0f,
                                                         cell.getHeight() * 0.08f
                                                             * juce::jlimit (0.08f, 1.0f, s.velocity));
                    g.setColour (juce::Colour (0xfffff9f2).withAlpha (0.82f * ghostAlpha));
                    g.fillRect (cell.getX() + 3.0f, cell.getBottom() - barHeight - 2.0f,
                                juce::jmax (1.0f, cell.getWidth() - 6.0f), barHeight);
                }
                else
                {
                    g.setColour (c.panel);
                    g.fillRoundedRectangle (cell, 2.0f);
                    g.setColour (c.line.withAlpha (0.72f));
                    g.drawRoundedRectangle (cell.reduced (0.5f), 2.0f, 1.0f);
                }

                if (s.on && s.probability < 0.995f)
                {
                    g.setColour (c.ink.withAlpha (0.7f * (repeatGhost ? 0.45f : 1.0f)));
                    g.fillEllipse (cell.getRight() - 6.0f, cell.getY() + 3.0f, 3.0f, 3.0f);
                }
                if (s.on && (s.ratchet > 1 || s.flam > 0))
                {
                    g.setColour (juce::Colour (0xfffff9f2));
                    g.setFont (mono (5.8f));
                    juce::String badge;
                    if (s.flam > 0)
                        badge += "F" + juce::String (s.flam);
                    if (s.ratchet > 1)
                        badge += (badge.isNotEmpty() ? " " : "") + juce::String ("x") + juce::String (s.ratchet);
                    g.drawText (badge, cell.toNearestInt().reduced (2),
                                juce::Justification::bottomRight, false);
                }
                if (s.on && std::abs (s.nudge) > 0.02f)
                {
                    g.setColour (c.ink.withAlpha (s.nudge < 0.0f ? 0.48f : 0.74f));
                    g.fillRect (cell.getX() + 3.0f, cell.getY() + 2.0f, 3.0f, 1.5f);
                }
                if (s.on && s.locks.enabled)
                {
                    g.setColour (c.ink);
                    g.fillEllipse (cell.getX() + 3.0f, cell.getY() + 3.0f, 2.6f, 2.6f);
                }
            }

            if (repeatedLane)
            {
                const auto loopX = (float) labelWidth + (float) laneLength * cellWidth;
                g.setColour (stepColour.withAlpha (0.55f));
                g.drawLine (loopX, row.getY() + 1.0f, loopX, row.getBottom() - 1.0f, 1.0f);
            }
        }

        if (playheadStep >= 0 && playheadStep < length)
        {
            const auto x = (float) labelWidth + (float) playheadStep * cellWidth;
            g.setColour (c.bg.withAlpha (0.72f));
            g.fillRect (x - 1.0f, (float) headerHeight, 3.5f, (float) getHeight() - headerHeight);
            g.setColour (c.green);
            g.fillRect (x, (float) headerHeight, 1.5f, (float) getHeight() - headerHeight);
            juce::Path marker;
            marker.startNewSubPath (x - 4.0f, 0.0f);
            marker.lineTo (x + 5.5f, 0.0f);
            marker.lineTo (x + 0.75f, 6.0f);
            marker.closeSubPath();
            g.setColour (c.green);
            g.fillPath (marker);
        }

        // Length boundary.
        const auto endX = (float) labelWidth + (float) length * cellWidth;
        if (endX < (float) getWidth())
        {
            g.setColour (c.lineStrong.withAlpha (0.6f));
            g.fillRect (endX, (float) headerHeight, 1.0f, (float) getHeight() - headerHeight);
        }
    }

    void SequencerGrid::applyCell (int pad, int displayStep, float relativeY)
    {
        if (data == nullptr || pad < 0 || pad >= sliceforge::kNumPads
            || displayStep < 0 || displayStep >= sliceforge::kSeqSteps)
            return;
        const int step = dataStepForLane (pad, displayStep);
        const auto patternIndex = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
        auto s = data->patterns[(size_t) patternIndex].rows[(size_t) pad][(size_t) step];
        const auto vertical = juce::jlimit (0.0f, 1.0f, 1.0f - relativeY);

        switch (mode)
        {
            case EditMode::Draw:
                s.on = dragTarget;
                if (s.on)
                {
                    s.velocity = 0.82f;
                    s.probability = 1.0f;
                    s.ratchet = 1;
                }
                break;
            case EditMode::Velocity:
                s.on = true;
                s.velocity = juce::jlimit (0.08f, 1.0f, vertical);
                break;
            case EditMode::Probability:
                s.on = true;
                s.probability = juce::jlimit (0.05f, 1.0f, vertical);
                break;
            case EditMode::Ratchet:
                s.on = true;
                s.ratchet = juce::jlimit (1, 4, 1 + (int) std::round ((1.0f - vertical) * 3.0f));
                break;
            case EditMode::Nudge:
                s.on = true;
                s.nudge = juce::jlimit (-0.5f, 0.5f, (vertical - 0.5f));
                break;
            case EditMode::Flam:
                s.on = true;
                s.flam = juce::jlimit (0, 2, (int) std::round ((1.0f - vertical) * 2.0f));
                break;
        }
        if (onStepChanged)
            onStepChanged (pad, step, s);
    }

    void SequencerGrid::mouseDown (const juce::MouseEvent& event)
    {
        if (data == nullptr)
            return;
        const auto pos = event.position;
        const int pad = padAtY (pos.y);

        if (pad < 0)
            return;

        if (pos.x < labelWidth)
        {
            if (muteRects[(size_t) pad].contains (pos))
            {
                if (onLaneMuteClicked)
                    onLaneMuteClicked (pad);
                return;
            }
            if (soloRects[(size_t) pad].contains (pos))
            {
                if (onLaneSoloClicked)
                    onLaneSoloClicked (pad);
                return;
            }
            selectedLane = pad;
            if (onLaneSelected)
                onLaneSelected (pad);
            repaint();
            return;
        }

        const int step = stepAtX (pos.x);
        if (step < 0 || step >= juce::jlimit (1, sliceforge::kSeqSteps, data->length))
            return;

        if (event.mods.isPopupMenu())
        {
            if (onStepInspectorRequested)
                onStepInspectorRequested (pad, dataStepForLane (pad, step),
                                          event.getScreenPosition());
            return;
        }

        if (onGestureStart)
            onGestureStart();

        dragPad = pad;
        dragStep = step;
        const auto patternIndex = juce::jlimit (0, sliceforge::kSeqPatterns - 1, data->activePattern);
        if (mode == EditMode::Draw)
            dragTarget = ! data->patterns[(size_t) patternIndex]
                                .rows[(size_t) pad][(size_t) dataStepForLane (pad, step)].on;
        else
            dragTarget = true;

        const auto cell = cellBounds (pad, step);
        applyCell (pad, step, (pos.y - cell.getY()) / juce::jmax (1.0f, cell.getHeight()));
    }

    void SequencerGrid::mouseDrag (const juce::MouseEvent& event)
    {
        if (data == nullptr || dragPad < 0)
            return;
        const auto pos = event.position;
        const int pad = padAtY (pos.y);
        const int step = stepAtX (pos.x);
        if (pad < 0 || step < 0 || step >= juce::jlimit (1, sliceforge::kSeqSteps, data->length))
            return;

        if (mode == EditMode::Draw || mode == EditMode::Ratchet)
        {
            if (pad == dragPad && step == dragStep)
                return;
        }
        if (mode == EditMode::Flam)
        {
            if (pad == dragPad && step == dragStep)
                return;
        }

        dragPad = pad;
        dragStep = step;
        const auto cell = cellBounds (pad, step);
        applyCell (pad, step, (pos.y - cell.getY()) / juce::jmax (1.0f, cell.getHeight()));
    }

    void SequencerGrid::mouseUp (const juce::MouseEvent&)
    {
        dragPad = -1;
        dragStep = -1;
    }
} // namespace sliceforge::ui
