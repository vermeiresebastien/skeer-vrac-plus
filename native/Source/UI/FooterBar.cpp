#include "FooterBar.h"
#include "Theme.h"

namespace sliceforge::ui
{
    FooterBar::FooterBar (SliceForgeAudioProcessor& processor) : processorRef (processor)
    {
    }

    void FooterBar::paint (juce::Graphics& g)
    {
        const auto& c = palette();
        g.setColour (c.panel);
        g.fillAll();
        g.setColour (c.line);
        g.fillRect (0, 0, getWidth(), 1);
        // A reload that could not restore everything it was handed, and any hit a block could not
        // fit, say so here in the slot the static hint normally uses, so it is visible in the
        // window the session opened in. Long strings go here rather than in the narrow meter.
        const auto notice = processorRef.getStateNotice();
        const auto dropped = processorRef.getDroppedHitCount();
        const auto warning = notice.isNotEmpty()
                                 ? notice
                                 : (dropped > 0
                                        ? juce::String (dropped) + " HITS DROPPED - TOO MANY TRIGGERS FOR THIS BLOCK SIZE"
                                        : juce::String());
        g.setColour (warning.isNotEmpty() ? c.red : c.muted);
        g.setFont (font (7.0f, true));
        g.drawText (warning.isNotEmpty() ? warning
                                         : juce::String ("RIGHT-CLICK A KNOB TO MIDI LEARN  /  LOAD ANY SOUND AND AUTO-SLICE IT  /  8 STEREO OUTPUTS"),
                    14, 0, getWidth() - 280, getHeight(), juce::Justification::centredLeft, false);
        g.setFont (mono (7.5f));
        g.setColour (c.muted);
        g.drawText (juce::String (processorRef.getActiveVoiceCount()) + " VOICES  /  8 STEREO OUTS",
                    getWidth() - 300, 0, 150, getHeight(), juce::Justification::centredRight, false);
        g.setColour (c.ink);
        g.setFont (display (14.0f));
        g.drawText ("SLICEFORGE", getWidth() - 140, 0, 126, getHeight(), juce::Justification::centredRight, false);
    }
} // namespace sliceforge::ui

