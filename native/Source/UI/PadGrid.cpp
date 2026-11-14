#include "PadGrid.h"
#include "Theme.h"

namespace sliceforge::ui
{
    PadGrid::PadGrid (SliceForgeAudioProcessor& processor) : processorRef (processor)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void PadGrid::setSelectedPad (int pad)
    {
        pad = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
        if (pad != selected)
        {
            selected = pad;
            repaint();
        }
    }

    void PadGrid::resized()
    {
        updateRects();
    }

    void PadGrid::updateRects()
    {
        const auto area = getLocalBounds().toFloat();
        constexpr float gap = 8.0f;
        constexpr float toggleWidth = 18.0f;
        constexpr float toggleGap = 4.0f;
        const auto cellW = (area.getWidth() - gap * 3.0f) * 0.25f;
        const auto rowH = (area.getHeight() - gap * 3.0f) * 0.25f;
        for (int row = 0; row < 4; ++row)
        {
            for (int col = 0; col < 4; ++col)
            {
                const auto pad = (3 - row) * 4 + col;
                const auto x = area.getX() + (float) col * (cellW + gap);
                const auto y = area.getY() + (float) row * (rowH + gap);
                padRects[(size_t) pad] = { x, y, cellW - toggleWidth - toggleGap, rowH };
                muteRects[(size_t) pad] = { x + cellW - toggleWidth, y, toggleWidth, (rowH - 3.0f) * 0.5f };
                soloRects[(size_t) pad] = { x + cellW - toggleWidth, y + rowH * 0.5f + 1.5f, toggleWidth,
                                            (rowH - 3.0f) * 0.5f };
            }
        }
    }

    void PadGrid::mouseDown (const juce::MouseEvent& event)
    {
        const auto position = event.position;
        for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
        {
            if (muteRects[(size_t) pad].contains (position))
            {
                if (onMuteClicked)
                    onMuteClicked (pad);
                return;
            }
            if (soloRects[(size_t) pad].contains (position))
            {
                if (onSoloClicked)
                    onSoloClicked (pad);
                return;
            }
        }
        for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
        {
            if (! padRects[(size_t) pad].contains (position))
                continue;
            const auto& r = padRects[(size_t) pad];
            auto velocity = 0.35f + juce::jlimit (0.0f, 1.0f, 1.0f - (position.y - r.getY()) / r.getHeight()) * 0.65f;
            if (event.mods.isShiftDown())
                velocity = 1.0f;
            if (event.mods.isAltDown())
                velocity = 0.3f;
            selected = pad;
            processorRef.triggerPad (pad, velocity);
            processorRef.setSelectedPad (pad);
            if (onPadTriggered)
                onPadTriggered (pad, velocity);
            if (onPadSelected)
                onPadSelected (pad);
            repaint();
            return;
        }
    }

    void PadGrid::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        const auto now = juce::Time::getMillisecondCounter();
        for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
        {
            const auto& r = padRects[(size_t) pad];
            const auto settings = processorRef.getParameterCache().readPad (pad);
            const auto family = sliceforge::familyForModel (settings.modelIndex);
            const auto model = sliceforge::localModelForIndex (settings.modelIndex);
            const auto accent = sliceforge::ui::familyColour (family);
            const auto hitAge = now - processorRef.getPadHitStamp (pad);
            const bool active = hitAge < 180;
            const bool muted = settings.mute;

            // Pad key.
            auto padBounds = r;
            g.setColour (c.pad);
            g.fillRoundedRectangle (padBounds, 4.0f);
            if (pad == selected)
            {
                g.setColour (c.accent.withAlpha (0.12f));
                g.fillRoundedRectangle (padBounds, 4.0f);
            }
            if (active)
            {
                g.setColour (c.green.withAlpha (0.20f));
                g.fillRoundedRectangle (padBounds, 4.0f);
            }
            if (muted)
            {
                g.setColour (c.bg.withAlpha (0.45f));
                g.fillRoundedRectangle (padBounds, 4.0f);
            }

            g.setColour (c.line);
            g.drawRoundedRectangle (padBounds.reduced (0.5f), 4.0f, 1.0f);
            if (pad == selected)
            {
                g.setColour (c.selection);
                g.drawRoundedRectangle (padBounds.reduced (0.5f), 4.0f, 1.8f);
            }
            if (active)
            {
                g.setColour (c.green);
                g.drawRoundedRectangle (padBounds.reduced (2.5f), 2.5f, 1.0f);
            }

            // Pad number and family dot.
            g.setColour (c.muted);
            g.setFont (mono (7.0f));
            g.drawText (juce::String (pad + 1).paddedLeft ('0', 2), (int) padBounds.getX() + 8,
                        (int) padBounds.getY() + 7, 22, 10, juce::Justification::centredLeft, false);
            g.setColour (accent);
            g.fillEllipse (padBounds.getX() + 32.0f, padBounds.getY() + 10.0f, 4.0f, 4.0f);

            // Model name and note.
            const auto name = sliceforge::modelBank()[(size_t) family][(size_t) model].name;
            g.setColour (muted ? c.muted : c.ink);
            g.setFont (font (13.0f, true));
            g.drawFittedText (name, padBounds.toNearestInt().reduced (12, 12), juce::Justification::centred, 1);

            g.setColour (c.muted);
            g.setFont (mono (7.0f));
            g.drawText (sliceforge::padNotes()[(size_t) pad], (int) padBounds.getX() + 8,
                        (int) padBounds.getBottom() - 17, 48, 10, juce::Justification::centredLeft, false);

            const auto code = sliceforge::modelBank()[(size_t) family][(size_t) model].code;
            g.drawText (juce::String (code) + " " + juce::String ((int) std::round (settings.drumify * 100.0f)),
                        (int) padBounds.getRight() - 58, (int) padBounds.getBottom() - 17,
                        50, 10, juce::Justification::centredRight, false);

            // M / S keys.
            auto drawToggle = [&] (juce::Rectangle<float> bounds, const juce::String& text, bool on)
            {
                g.setColour (on ? c.accent : c.panel);
                g.fillRoundedRectangle (bounds, 2.5f);
                g.setColour (on ? juce::Colour (0xfffff9f2) : c.ink);
                g.setFont (font (6.8f, true));
                g.drawText (text, bounds.toNearestInt(), juce::Justification::centred, false);
                g.setColour (c.line);
                g.drawRoundedRectangle (bounds.reduced (0.5f), 2.5f, 1.0f);
            };
            drawToggle (muteRects[(size_t) pad], "M", settings.mute);
            drawToggle (soloRects[(size_t) pad], "S", settings.solo);
        }
    }
} // namespace sliceforge::ui

