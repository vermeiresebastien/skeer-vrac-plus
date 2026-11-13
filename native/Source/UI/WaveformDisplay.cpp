#include "WaveformDisplay.h"
#include "Theme.h"

namespace vrac::ui
{
    WaveformDisplay::WaveformDisplay()
    {
        accent = palette().accent;
        setInterceptsMouseClicks (true, false);
    }

    void WaveformDisplay::setSample (SamplePtr newSample)
    {
        sample = std::move (newSample);
        repaint();
    }

    void WaveformDisplay::setSlices (const std::vector<Slice>& newSlices)
    {
        slices = newSlices;
        repaint();
    }

    void WaveformDisplay::setStart (double seconds)
    {
        if (std::abs (seconds - startSeconds) < 1.0e-9)
            return;
        startSeconds = seconds;
        repaint();
    }

    void WaveformDisplay::setFocusSeconds (double seconds)
    {
        focusSeconds = juce::jmax (0.0, seconds);
        repaint();
    }

    void WaveformDisplay::setInteractive (bool shouldBeInteractive)
    {
        interactive = shouldBeInteractive;
        setMouseCursor (interactive ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    }

    void WaveformDisplay::setCompact (bool shouldBeCompact)
    {
        compact = shouldBeCompact;
        repaint();
    }

    void WaveformDisplay::setAccent (juce::Colour colour)
    {
        accent = colour;
        repaint();
    }

    juce::Rectangle<float> WaveformDisplay::boundsForWave() const
    {
        return getLocalBounds().toFloat().reduced (6.0f, compact ? 4.0f : 8.0f);
    }

    double WaveformDisplay::timeAt (float x) const
    {
        if (sample == nullptr)
            return 0.0;
        const auto area = boundsForWave();
        const auto fraction = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / juce::jmax (1.0f, area.getWidth()));
        const auto duration = sample->getDurationSeconds();
        if (focusSeconds > 0.0)
        {
            const auto viewStart = juce::jmax (0.0, startSeconds - juce::jmin (0.22, focusSeconds * 0.12));
            const auto viewEnd = juce::jmin (duration, startSeconds + focusSeconds);
            return juce::jlimit (0.0, duration, viewStart + fraction * juce::jmax (0.01, viewEnd - viewStart));
        }
        return juce::jlimit (0.0, duration, fraction * duration);
    }

    float WaveformDisplay::xForTime (double time) const
    {
        if (sample == nullptr)
            return 0.0f;
        const auto area = boundsForWave();
        const auto duration = sample->getDurationSeconds();
        if (focusSeconds > 0.0)
        {
            const auto viewStart = juce::jmax (0.0, startSeconds - juce::jmin (0.22, focusSeconds * 0.12));
            const auto viewEnd = juce::jmin (duration, startSeconds + focusSeconds);
            const auto span = juce::jmax (0.01, viewEnd - viewStart);
            return area.getX() + (float) ((time - viewStart) / span * area.getWidth());
        }
        return area.getX() + (float) (time / juce::jmax (0.001, duration) * area.getWidth());
    }

    void WaveformDisplay::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        const auto area = getLocalBounds().toFloat();
        g.setColour (c.panel);
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (c.line);
        g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, 1.0f);

        if (sample == nullptr || sample->peakMin.empty())
        {
            g.setColour (c.muted);
            g.setFont (mono (9.0f));
            g.drawText ("NO SAMPLE", area, juce::Justification::centred, false);
            return;
        }

        const auto waveArea = boundsForWave();
        const auto centreY = waveArea.getCentreY();
        const auto amplitude = waveArea.getHeight() * 0.46f;
        const auto duration = sample->getDurationSeconds();
        double viewStart = 0.0;
        double viewEnd = duration;
        if (focusSeconds > 0.0)
        {
            viewStart = juce::jmax (0.0, startSeconds - juce::jmin (0.22, focusSeconds * 0.12));
            viewEnd = juce::jmin (duration, startSeconds + focusSeconds);
        }
        const auto span = juce::jmax (0.001, viewEnd - viewStart);
        const auto startX = xForTime (startSeconds);
        const auto nextSlice = [this]() -> double
        {
            double next = 1.0e9;
            for (const auto& slice : slices)
                if (slice.time > startSeconds + 0.002)
                    next = juce::jmin (next, slice.time);
            return next;
        }();
        const auto endX = juce::jlimit (waveArea.getX(), waveArea.getRight(),
                                        xForTime (juce::jmin (nextSlice, viewEnd)));

        if (endX > startX)
        {
            g.setColour (accent.withAlpha (0.07f));
            g.fillRect (juce::Rectangle<float> (startX, waveArea.getY(), endX - startX, waveArea.getHeight()));
        }

        const auto peakCount = (int) sample->peakMin.size();
        const auto width = juce::jmax (1, (int) std::round (waveArea.getWidth()));
        for (int px = 0; px < width; ++px)
        {
            const auto t0 = viewStart + span * ((double) px / width);
            const auto t1 = viewStart + span * ((double) (px + 1) / width);
            const auto b0 = juce::jlimit (0, peakCount - 1, (int) (t0 / duration * peakCount));
            const auto b1 = juce::jmax (b0 + 1, juce::jlimit (0, peakCount, (int) std::ceil (t1 / duration * peakCount)));
            float lo = 0.0f, hi = 0.0f;
            for (int b = b0; b < b1; ++b)
            {
                lo = juce::jmin (lo, sample->peakMin[(size_t) b]);
                hi = juce::jmax (hi, sample->peakMax[(size_t) b]);
            }
            const auto top = centreY - hi * amplitude;
            const auto bottom = centreY - lo * amplitude;
            const auto x = waveArea.getX() + (float) px;
            const auto inSelection = x >= startX && x <= endX;
            g.setColour (inSelection ? accent.withAlpha (0.9f) : c.ink.withAlpha (0.62f));
            g.fillRect (juce::Rectangle<float> (x, top, 1.0f, juce::jmax (1.0f, bottom - top)));
        }

        if (! compact)
        {
            g.setColour (accent.withAlpha (0.4f));
            for (const auto& slice : slices)
            {
                const auto x = xForTime (slice.time);
                if (x >= waveArea.getX() && x <= waveArea.getRight())
                    g.fillRect (juce::Rectangle<float> (x, waveArea.getBottom() - 6.0f, 1.0f, 6.0f));
            }
        }

        if (startX >= waveArea.getX() - 4.0f && startX <= waveArea.getRight() + 4.0f)
        {
            g.setColour (accent);
            g.fillRect (juce::Rectangle<float> (startX - 0.75f, waveArea.getY(), 1.5f, waveArea.getHeight()));
            juce::Path handle;
            handle.startNewSubPath (startX - 5.0f, waveArea.getY());
            handle.lineTo (startX + 5.0f, waveArea.getY());
            handle.lineTo (startX, waveArea.getY() + 7.0f);
            handle.closeSubPath();
            g.fillPath (handle);
            g.fillEllipse (startX - 2.0f, waveArea.getY() + 10.0f, 4.0f, 4.0f);
        }

        if (! compact)
        {
            g.setColour (c.muted);
            g.setFont (mono (7.0f));
            auto footerArea = area;
            g.drawText (juce::String (slices.size()) + " SLICES", footerArea.removeFromBottom (13.0f).removeFromRight (70.0f),
                        juce::Justification::centredRight, false);
        }
    }

    void WaveformDisplay::mouseDown (const juce::MouseEvent& event)
    {
        if (! interactive || sample == nullptr || event.mods.isPopupMenu())
            return;
        dragging = true;
        if (onGestureStart)
            onGestureStart();
        if (onStartChanged)
            onStartChanged (timeAt ((float) event.position.x));
    }

    void WaveformDisplay::mouseDrag (const juce::MouseEvent& event)
    {
        if (! dragging || ! onStartChanged)
            return;
        onStartChanged (timeAt ((float) event.position.x));
    }

    void WaveformDisplay::mouseUp (const juce::MouseEvent&)
    {
        dragging = false;
    }
} // namespace vrac::ui

