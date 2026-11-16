#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int kMaxRecordBytes = 48 * 1024 * 1024;
}

SliceForgeAudioProcessor::SliceForgeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output 1", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output 2", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output 3", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output 4", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output 5", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output 6", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output 7", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output 8", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VRACPlus", sliceforge::createParameterLayout())
{
    parameterCache.connect (apvts);
    for (auto& start : padStarts)
        start.store (0.0f, std::memory_order_relaxed);
    for (auto& stamp : padHitStamps)
        stamp.store (0, std::memory_order_relaxed);
    for (auto& mapping : ccMappings)
        mapping.clear();

    sequencerData.store (sliceforge::makeDefaultSequencerData(), std::memory_order_release);
    seqRandom.setSeed (0x51E3A9C7u);

    currentProgramIndex.store (0);
    applyFactoryPresetInternal (0, true);
}

SliceForgeAudioProcessor::~SliceForgeAudioProcessor() = default;

juce::AudioProcessorEditor* SliceForgeAudioProcessor::createEditor()
{
    auto* editor = new SliceForgeAudioProcessorEditor (*this);
    return editor;
}

bool SliceForgeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.outputBuses.size() != sliceforge::kNumBuses)
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int i = 0; i < sliceforge::kNumBuses; ++i)
    {
        const auto set = layouts.getChannelSet (false, i);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo())
            return false;
    }
    if (layouts.inputBuses.size() > 1)
        return false;
    if (layouts.inputBuses.size() == 1)
    {
        const auto input = layouts.getChannelSet (true, 0);
        if (! input.isDisabled() && input != juce::AudioChannelSet::mono()
            && input != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void SliceForgeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax (32, samplesPerBlock);
    engine.prepare (sampleRate, maxBlockSize);
    engine.setSample (engine.getSample());
    preview.prepare (sampleRate);
    outputStage.prepare();
    // Worst case per block: every queued UI trigger, every sequencer trigger and one event per
    // MIDI sample position, with a margin. Fixed so the event list never allocates live.
    playbackEventCapacity = maxBlockSize + sliceforge::kNumPads * 4 + 384;
    playbackEvents.reserve ((size_t) playbackEventCapacity);
    const auto newRecordRate = (int) std::round (sampleRate);
    if (newRecordRate != recordSampleRate
        && (recordingActive.load (std::memory_order_relaxed) || recordingTakeReady.load (std::memory_order_relaxed)))
    {
        // A take is stamped with the rate it was captured at. If the host changes rate with a take
        // still in flight or unclaimed, that label no longer describes the buffer, and adopting it
        // would hand the engine a sample that plays at the wrong speed. Drop the take rather than
        // mislabel it, and say so in the same place a failed reload does.
        recordingActive.store (false, std::memory_order_relaxed);
        recordingTakeReady.store (false, std::memory_order_relaxed);
        recordedSamples.store (0, std::memory_order_relaxed);
        recordBuffer.clear();
        audioWasRecording = false;
        stateNotice = "RECORDING ABANDONED - HOST CHANGED SAMPLE RATE";
        juce::Logger::writeToLog ("SliceForge: " + stateNotice);
        triggerAsyncUpdate();
    }
    recordSampleRate = newRecordRate;
    sidechainActive.store (getBus (true, 0) != nullptr && getBus (true, 0)->isEnabled(), std::memory_order_relaxed);
    seqInternalPlaying.store (false, std::memory_order_relaxed);
    seqResetRequested.store (false, std::memory_order_relaxed);
    resetSequencerPlayback();
    wasPrepared = true;
}

void SliceForgeAudioProcessor::releaseResources()
{
    wasPrepared = false;
}

int SliceForgeAudioProcessor::getNumPrograms()
{
    return (int) sliceforge::factoryPresets().size();
}

int SliceForgeAudioProcessor::getCurrentProgram()
{
    return juce::jlimit (0, getNumPrograms() - 1, currentProgramIndex.load (std::memory_order_relaxed));
}

void SliceForgeAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;
    if (! restoringState && index == currentProgramIndex.load (std::memory_order_relaxed))
        return;
    applyFactoryPresetInternal (index, true);
}

const juce::String SliceForgeAudioProcessor::getProgramName (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return {};
    return sliceforge::factoryPresets()[(size_t) index].name;
}

void SliceForgeAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void SliceForgeAudioProcessor::applyFactoryPresetInternal (int index, bool clearHistory)
{
    const auto& presets = sliceforge::factoryPresets();
    if (index < 0 || index >= (int) presets.size())
        return;
    const auto& preset = presets[(size_t) index];

    auto sample = sliceforge::loadFactorySource (preset.sourceId);
    if (sample != nullptr)
    {
        engine.setSample (sample);
        sourceId = preset.sourceId;
        sourceName = sample->name;
        sourceIsFactory = true;
        customSamplePcm.reset();
        const auto sensitivity = juce::jlimit (0.0f, 1.0f, preset.sensitivity);
        setParameterValue ("source.sensitivity", sensitivity);
        slices = sliceforge::detectSlices (*sample, sensitivity);
        const auto starts = sliceforge::padStartsFromSlices (slices);
        for (int i = 0; i < sliceforge::kNumPads; ++i)
            padStarts[(size_t) i].store ((float) starts[(size_t) i], std::memory_order_relaxed);
    }

    auto pads = sliceforge::makeDefaultPads();
    for (int i = 0; i < sliceforge::kNumPads; ++i)
    {
        const auto& o = preset.pads[(size_t) i];
        auto& pad = pads[(size_t) i];
        if (o.modelIndex >= 0)
            pad.modelIndex = juce::jlimit (0, sliceforge::kNumModels - 1, o.modelIndex);
        if (std::isfinite (o.drumify)) pad.drumify = o.drumify;
        if (std::isfinite (o.length)) pad.length = o.length;
        if (std::isfinite (o.pitch)) pad.pitch = o.pitch;
        if (std::isfinite (o.pan)) pad.pan = o.pan;
        if (o.choke >= 0) pad.choke = o.choke;
        if (std::isfinite (o.drumifiedGain)) pad.drumifiedGain = o.drumifiedGain;
        if (std::isfinite (o.originalGain)) pad.originalGain = o.originalGain;
        parameterCache.writePad (i, pad);
    }
    parameterCache.writeGlobal (preset.global);
    selectedPad.store (juce::jlimit (0, sliceforge::kNumPads - 1, preset.selectedPad), std::memory_order_relaxed);
    currentPresetName = preset.name;
    currentProgramIndex.store (index, std::memory_order_relaxed);
    if (clearHistory)
    {
        undoStack.clear();
        redoStack.clear();
    }
    postUiChange();
}

void SliceForgeAudioProcessor::setParameterValue (const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter (id))
    {
        const auto& range = parameter->getNormalisableRange();
        parameter->setValueNotifyingHost (range.convertTo0to1 (value));
    }
}

void SliceForgeAudioProcessor::setParameterNormalised (const juce::String& id, float value)
{
    if (auto* parameter = apvts.getParameter (id))
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
}

void SliceForgeAudioProcessor::buildRuntime()
{
    sliceforge::EngineRuntime runtime;
    bool anySolo = false;
    for (int i = 0; i < sliceforge::kNumPads; ++i)
    {
        runtime.pads[(size_t) i] = parameterCache.readPad (i);
        anySolo = anySolo || runtime.pads[(size_t) i].solo;
    }
    for (int i = 0; i < sliceforge::kNumPads; ++i)
    {
        const auto& pad = runtime.pads[(size_t) i];
        runtime.audible[(size_t) i] = ! pad.mute && (! anySolo || pad.solo);
        runtime.padStarts[(size_t) i] = padStarts[(size_t) i].load (std::memory_order_relaxed);
    }
    runtime.global = parameterCache.readGlobal();
    engine.setRuntime (runtime);
}

void SliceForgeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (! wasPrepared)
    {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    auto* sidechainBus = getBus (true, 0);
    sidechainActive.store (sidechainBus != nullptr && sidechainBus->isEnabled(), std::memory_order_relaxed);

    // Record before the process block buffer is cleared (input and output share it).
    juce::AudioBuffer<float> inputBuffer;
    bool hasInput = false;
    if (sidechainBus != nullptr && sidechainBus->isEnabled())
    {
        inputBuffer = getBusBuffer (buffer, true, 0);
        hasInput = inputBuffer.getNumChannels() > 0;
    }
    serviceRecording (hasInput ? &inputBuffer : nullptr, numSamples);

    buildRuntime();
    if (chokeRequested.exchange (false, std::memory_order_relaxed))
        engine.chokeAll();
    if (seqResetRequested.exchange (false, std::memory_order_relaxed))
        resetSequencerPlayback();

    buffer.clear();
    engine.clearBusBuffers (numSamples);

    playbackEvents.clear();
    auto addPlaybackEvent = [this] (int offset, int pad, float velocity,
                                    bool followSelection,
                                    const sliceforge::StepLocks* locks = nullptr)
    {
        if ((int) playbackEvents.size() >= playbackEventCapacity)
        {
            droppedHits.fetch_add (1, std::memory_order_relaxed);
            return;
        }
        auto& event = playbackEvents.emplace_back();
        event.sampleOffset = juce::jmax (0, offset);
        event.pad = pad;
        event.velocity = juce::jlimit (0.0f, 1.0f, velocity);
        event.followAutoSelection = followSelection;
        event.hasLocks = locks != nullptr && locks->enabled;
        if (event.hasLocks)
            event.locks = *locks;
    };

    // UI triggers are scheduled at the start of the block.
    {
        auto readHandle = uiTriggerFifo.read (uiTriggerFifo.getNumReady());
        auto fire = [&addPlaybackEvent] (const UiTrigger& trigger)
        {
            addPlaybackEvent (0, trigger.pad, trigger.velocity, true);
        };
        for (int i = 0; i < readHandle.blockSize1; ++i)
            fire (uiTriggerData[(size_t) (readHandle.startIndex1 + i)]);
        for (int i = 0; i < readHandle.blockSize2; ++i)
            fire (uiTriggerData[(size_t) (readHandle.startIndex2 + i)]);
    }

    // Host transport information for the step sequencer.
    sliceforge::SeqHostInfo hostInfo;
    if (auto* playhead = getPlayHead())
    {
        if (const auto position = playhead->getPosition())
        {
            if (const auto ppq = position->getPpqPosition())
            {
                hostInfo.valid = true;
                hostInfo.ppq = *ppq;
                hostInfo.playing = position->getIsPlaying();
                if (const auto bpm = position->getBpm())
                    hostInfo.bpm = juce::jmax (20.0, *bpm);
            }
        }
    }

    const auto sequence = sequencerData.load (std::memory_order_acquire);
    sliceforge::SeqBlockOutput sequenceOutput;
    if (sequence != nullptr)
        sliceforge::processSequencerBlock (*sequence, hostInfo, seqInternalPlaying.load (std::memory_order_relaxed),
                                     currentSampleRate, numSamples, seqRandom, seqPlaybackState, sequenceOutput,
                                     // Read once per block, so a host that automates the pattern
                                     // mid-block cannot hand one block two different answers.
                                     parameterCache.readActivePattern());

    if (sequenceOutput.droppedTriggers > 0)
        droppedHits.fetch_add (sequenceOutput.droppedTriggers, std::memory_order_relaxed);

    seqHostPlaying.store (sequenceOutput.hostPlaying, std::memory_order_relaxed);
    seqTransportPlaying.store (sequenceOutput.transportPlaying, std::memory_order_relaxed);
    seqPlayheadStep.store (sequenceOutput.playheadStep, std::memory_order_relaxed);
    seqHostBpm.store (sequenceOutput.hostBpm, std::memory_order_relaxed);

    for (int i = 0; i < sequenceOutput.triggerCount; ++i)
    {
        const auto& trigger = sequenceOutput.triggers[(size_t) i];
        addPlaybackEvent (trigger.sampleOffset, trigger.pad, trigger.velocity, false,
                          trigger.hasLocks ? &trigger.locks : nullptr);
    }

    // MIDI CC is handled asynchronously on the message thread; note events join the
    // sample-accurate playback event list.
    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const int eventPos = juce::jlimit (0, juce::jmax (0, numSamples - 1), (int) metadata.samplePosition);

        if (message.isController())
        {
            const auto writeHandle = ccFifo.write (1);
            const CcEvent event { message.getControllerNumber(), message.getControllerValue() / 127.0f };
            if (writeHandle.blockSize1 > 0)
                ccData[(size_t) writeHandle.startIndex1] = event;
            else if (writeHandle.blockSize2 > 0)
                ccData[(size_t) writeHandle.startIndex2] = event;
            triggerAsyncUpdate();
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            engine.chokeAll();
        }

        if (message.isNoteOn() && message.getVelocity() > 0)
        {
            const auto note = message.getNoteNumber();
            const auto pad = note - sliceforge::kPadMidiBase;
            if (pad >= 0 && pad < sliceforge::kNumPads)
                addPlaybackEvent (eventPos, pad, message.getVelocity() / 127.0f, true);
        }
    }

    const int playbackEventCount = (int) playbackEvents.size();
    std::sort (playbackEvents.begin(), playbackEvents.begin() + playbackEventCount,
               [] (const PlaybackEvent& a, const PlaybackEvent& b)
               {
                   return a.sampleOffset < b.sampleOffset;
               });

    int cursor = 0;
    for (int i = 0; i < playbackEventCount; ++i)
    {
        const auto& event = playbackEvents[(size_t) i];
        const auto offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), event.sampleOffset);
        if (offset > cursor)
        {
            engine.processSegment (cursor, offset - cursor);
            cursor = offset;
        }
        if (event.hasLocks)
        {
            auto settings = engine.getRuntime().pads[(size_t) event.pad];
            settings.pitch = event.locks.pitch;
            settings.volume = event.locks.volume;
            settings.pan = event.locks.pan;
            settings.toneShift = event.locks.tone;
            settings.length = event.locks.length;
            engine.trigger (event.pad, event.velocity, &settings);
        }
        else
        {
            engine.trigger (event.pad, event.velocity);
        }
        updatePadHit (event.pad, event.followAutoSelection);
    }
    if (cursor < numSamples)
        engine.processSegment (cursor, numSamples - cursor);

    engine.processMaster (numSamples);

    // Vault auditions ride on bus 0 — the one every host routes by default — after the
    // master chain, so clicking through the library never touches the kit's own sound,
    // and the player's limiter keeps preview-plus-kit inside full scale.
    preview.render (engine.getBusBuffer (0), numSamples);

    copyEngineBusesToHost (buffer, numSamples);
    midiMessages.clear();
}

void SliceForgeAudioProcessor::copyEngineBusesToHost (juce::AudioBuffer<float>& buffer, int numSamples)
{
    auto* mainBus = getBus (false, 0);
    if (mainBus == nullptr || ! mainBus->isEnabled())
        return;

    bool folded = false;
    auto addBusTo = [] (const juce::AudioBuffer<float>& source, juce::AudioBuffer<float>& destination)
    {
        const auto n = juce::jmin (source.getNumSamples(), destination.getNumSamples());
        if (destination.getNumChannels() >= 2)
        {
            destination.addFrom (0, 0, source, 0, 0, n);
            destination.addFrom (1, 0, source, 1, 0, n);
        }
        else if (destination.getNumChannels() == 1)
        {
            for (int i = 0; i < n; ++i)
                destination.addSample (0, i, (source.getSample (0, i) + source.getSample (1, i)) * 0.5f);
        }
    };

    for (int bus = 0; bus < sliceforge::kNumBuses; ++bus)
    {
        auto* hostBus = getBus (false, bus);
        auto& source = engine.getBusBuffer (bus);
        if (hostBus != nullptr && hostBus->isEnabled())
        {
            auto destination = getBusBuffer (buffer, false, bus);
            addBusTo (source, destination);
        }
        else
        {
            auto destination = getBusBuffer (buffer, false, 0);
            addBusTo (source, destination);
            folded = true;
        }
    }

    if (folded)
    {
        auto main = getBusBuffer (buffer, false, 0);
        for (int channel = 0; channel < main.getNumChannels(); ++channel)
        {
            auto* data = main.getWritePointer (channel);
            for (int i = 0; i < numSamples; ++i)
                data[i] = std::tanh (data[i]);
        }
    }

    // Everything above is the kit; from here it is the output stage. It runs per host output
    // bus so a multi-out session gets the same clipper and the same trim on every stem, and the
    // ceiling clamp is the last thing that touches the signal before the host sees it. The
    // vault audition was mixed into bus 0 above, so it is protected by the stage too.
    const auto& globals = engine.getRuntime().global;
    outputStage.setParameters (globals.clip, globals.outGain);
    for (int bus = 0; bus < getBusCount (false); ++bus)
    {
        auto* hostBus = getBus (false, bus);
        if (hostBus != nullptr && hostBus->isEnabled())
        {
            auto destination = getBusBuffer (buffer, false, bus);
            outputStage.process (destination, numSamples);
        }
    }
}

void SliceForgeAudioProcessor::serviceRecording (const juce::AudioBuffer<float>* input, int numSamples)
{
    if (recordingActive.load (std::memory_order_relaxed))
    {
        audioWasRecording = true;
        if (input != nullptr && input->getNumSamples() > 0 && recordBuffer.getNumSamples() > 0)
        {
            const auto writePos = recordedSamples.load (std::memory_order_relaxed);
            const auto available = recordBuffer.getNumSamples() - writePos;
            const auto toWrite = juce::jmin (numSamples, juce::jmin (input->getNumSamples(), available));
            if (toWrite > 0)
            {
                const auto inputChannels = input->getNumChannels();
                for (int i = 0; i < toWrite; ++i)
                {
                    const auto left = input->getSample (0, i);
                    const auto right = inputChannels > 1 ? input->getSample (1, i) : left;
                    recordBuffer.setSample (0, writePos + i, left);
                    recordBuffer.setSample (1, writePos + i, right);
                }
                recordedSamples.store (writePos + toWrite, std::memory_order_relaxed);
            }
            if (recordedSamples.load (std::memory_order_relaxed) >= recordBuffer.getNumSamples())
                recordingActive.store (false, std::memory_order_relaxed);
        }
    }
    else if (audioWasRecording)
    {
        audioWasRecording = false;
        if (recordedSamples.load (std::memory_order_relaxed) > 0)
            recordingTakeReady.store (true, std::memory_order_relaxed);
    }
}

void SliceForgeAudioProcessor::updatePadHit (int pad, bool followAutoSelection)
{
    if (pad < 0 || pad >= sliceforge::kNumPads)
        return;
    padHitStamps[(size_t) pad].store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
    if (followAutoSelection)
    {
        lastTriggeredPad.store (pad, std::memory_order_relaxed);
        triggerStamp.fetch_add (1, std::memory_order_relaxed);
    }
}

void SliceForgeAudioProcessor::resetSequencerPlayback()
{
    seqPlaybackState = {};
    seqHostPlaying.store (false, std::memory_order_relaxed);
    seqTransportPlaying.store (false, std::memory_order_relaxed);
    seqPlayheadStep.store (-1, std::memory_order_relaxed);
}

std::shared_ptr<const sliceforge::SequencerData> SliceForgeAudioProcessor::getSequencerData() const
{
    return sequencerData.load (std::memory_order_acquire);
}

void SliceForgeAudioProcessor::setSequencerData (std::shared_ptr<const sliceforge::SequencerData> data)
{
    if (data == nullptr)
        return;
    const auto previous = sequencerData.load (std::memory_order_acquire);
    auto next = std::move (data);
    sequencerData.store (next, std::memory_order_release);
    if (previous != nullptr && next != nullptr
        && (previous->division != next->division || previous->hostSync != next->hostSync))
        seqResetRequested.store (true, std::memory_order_relaxed);
    markHostStateDirty();
}

void SliceForgeAudioProcessor::setPatternParameter (int index)
{
    // setValueNotifyingHost, not the parameter's operator=: a chip click has to reach the host as
    // well, or Live keeps showing - and automating - the pattern the plugin stopped playing.
    setParameterValue ("seq.pattern", (float) juce::jlimit (0, sliceforge::kSeqPatterns - 1, index));
}

void SliceForgeAudioProcessor::setSequencerInternalPlaying (bool shouldPlay)
{
    seqInternalPlaying.store (shouldPlay, std::memory_order_relaxed);
    if (! shouldPlay)
    {
        seqTransportPlaying.store (false, std::memory_order_relaxed);
        seqPlayheadStep.store (-1, std::memory_order_relaxed);
    }
}

void SliceForgeAudioProcessor::markHostStateDirty()
{
    hostStateChanged.store (true, std::memory_order_relaxed);
    triggerAsyncUpdate();
}

void SliceForgeAudioProcessor::handleAsyncUpdate()
{
    auto readHandle = ccFifo.read (ccFifo.getNumReady());
    auto process = [this] (const CcEvent& event)
    {
        applyCcOnMessageThread (event.cc, event.value);
    };
    for (int i = 0; i < readHandle.blockSize1; ++i)
        process (ccData[(size_t) (readHandle.startIndex1 + i)]);
    for (int i = 0; i < readHandle.blockSize2; ++i)
        process (ccData[(size_t) (readHandle.startIndex2 + i)]);

    if (hostStateChanged.exchange (false, std::memory_order_relaxed))
        updateHostDisplay (juce::AudioProcessorListener::ChangeDetails()
                               .withNonParameterStateChanged (true));
}

void SliceForgeAudioProcessor::applyCcOnMessageThread (int cc, float value)
{
    lastCcSeen.store (juce::Time::getMillisecondCounter(), std::memory_order_relaxed);
    if (midiLearnTarget.isNotEmpty())
    {
        if (cc >= 0 && cc < 128)
        {
            ccMappings[(size_t) cc] = midiLearnTarget;
            midiLearnTarget.clear();
            postUiChange();
        }
        return;
    }
    if (cc >= 0 && cc < 128 && ccMappings[(size_t) cc].isNotEmpty())
        setParameterNormalised (ccMappings[(size_t) cc], value);
}

void SliceForgeAudioProcessor::setSelectedPad (int pad)
{
    pad = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
    if (selectedPad.load (std::memory_order_relaxed) == pad)
        return;
    selectedPad.store (pad, std::memory_order_relaxed);
    postUiChange();
}

void SliceForgeAudioProcessor::triggerPad (int pad, float velocity)
{
    if (pad < 0 || pad >= sliceforge::kNumPads)
        return;
    auto writeHandle = uiTriggerFifo.write (1);
    if (writeHandle.blockSize1 > 0)
        uiTriggerData[(size_t) writeHandle.startIndex1] = { pad, juce::jlimit (0.0f, 1.0f, velocity) };
    else if (writeHandle.blockSize2 > 0)
        uiTriggerData[(size_t) writeHandle.startIndex2] = { pad, juce::jlimit (0.0f, 1.0f, velocity) };
    updatePadHit (pad);
}

void SliceForgeAudioProcessor::chokeAll()
{
    chokeRequested.store (true, std::memory_order_relaxed);
}

double SliceForgeAudioProcessor::getPadStart (int pad) const
{
    pad = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
    return padStarts[(size_t) pad].load (std::memory_order_relaxed);
}

void SliceForgeAudioProcessor::setPadStart (int pad, double seconds)
{
    pad = juce::jlimit (0, sliceforge::kNumPads - 1, pad);
    const auto sample = engine.getSample();
    const auto duration = sample != nullptr ? sample->getDurationSeconds() : 0.0;
    padStarts[(size_t) pad].store ((float) juce::jlimit (0.0, juce::jmax (0.0, duration - 0.001), seconds),
                                   std::memory_order_relaxed);
    postUiChange();
}

juce::String SliceForgeAudioProcessor::getSourceName() const { return sourceName; }
juce::String SliceForgeAudioProcessor::getSourceId() const { return sourceId; }
juce::String SliceForgeAudioProcessor::getPresetName() const { return currentPresetName; }
juce::String SliceForgeAudioProcessor::getMidiLearnTarget() const { return midiLearnTarget; }

juce::String SliceForgeAudioProcessor::getMidiMappingForCc (int cc) const
{
    if (cc < 0 || cc >= 128)
        return {};
    return ccMappings[(size_t) cc];
}

uint32_t SliceForgeAudioProcessor::getPadHitStamp (int pad) const
{
    if (pad < 0 || pad >= sliceforge::kNumPads)
        return 0;
    return padHitStamps[(size_t) pad].load (std::memory_order_relaxed);
}

float SliceForgeAudioProcessor::getBusPeak (int bus) const
{
    return engine.getBusPeak (bus);
}

void SliceForgeAudioProcessor::loadFactorySource (const juce::String& newSourceId)
{
    auto sample = sliceforge::loadFactorySource (newSourceId);
    if (sample == nullptr)
        return;
    engine.setSample (sample);
    sourceId = newSourceId;
    sourceName = sample->name;
    sourceIsFactory = true;
    customSamplePcm.reset();
    resliceAndAssignStarts (false);
    undoStack.clear();
    redoStack.clear();
    postUiChange();
}

bool SliceForgeAudioProcessor::loadUserSampleFromFile (const juce::File& file)
{
    auto sample = sliceforge::loadAudioFile (file);
    if (sample == nullptr)
        return false;
    engine.setSample (sample);
    sourceId = "custom-" + juce::String (juce::Time::currentTimeMillis());
    sourceName = sample->name;
    sourceIsFactory = false;
    customSamplePcm = sliceforge::encodePcm16 (*sample);
    resliceAndAssignStarts (false);
    undoStack.clear();
    redoStack.clear();
    postUiChange();
    return true;
}

void SliceForgeAudioProcessor::onSensitivityChanged()
{
    resliceAndAssignStarts (true);
}

void SliceForgeAudioProcessor::resliceAndAssignStarts (bool remapExisting)
{
    const auto sample = engine.getSample();
    if (sample == nullptr)
        return;
    const auto oldSlices = slices;
    const auto sensitivity = juce::jlimit (0.0f, 1.0f, parameterCache.readSensitivity());
    auto newSlices = sliceforge::detectSlices (*sample, sensitivity);
    std::array<double, sliceforge::kNumPads> newStarts {};
    if (remapExisting && ! oldSlices.empty() && ! newSlices.empty())
    {
        for (int pad = 0; pad < sliceforge::kNumPads; ++pad)
        {
            const auto current = (double) padStarts[(size_t) pad].load (std::memory_order_relaxed);
            int nearest = 0;
            double best = std::numeric_limits<double>::max();
            for (int i = 0; i < (int) oldSlices.size(); ++i)
            {
                const auto d = std::abs (oldSlices[(size_t) i].time - current);
                if (d < best)
                {
                    best = d;
                    nearest = i;
                }
            }
            const auto ratio = oldSlices.size() > 1 ? (double) nearest / (double) (oldSlices.size() - 1) : 0.0;
            const auto target = juce::jlimit (0, (int) newSlices.size() - 1, (int) std::round (ratio * (double) (newSlices.size() - 1)));
            newStarts[(size_t) pad] = newSlices[(size_t) target].time;
        }
    }
    else
    {
        const auto starts = sliceforge::padStartsFromSlices (newSlices);
        for (int i = 0; i < sliceforge::kNumPads; ++i)
            newStarts[(size_t) i] = starts[(size_t) i];
    }
    slices = std::move (newSlices);
    for (int i = 0; i < sliceforge::kNumPads; ++i)
        padStarts[(size_t) i].store ((float) newStarts[(size_t) i], std::memory_order_relaxed);
    postUiChange();
}

void SliceForgeAudioProcessor::addSliceAt (double seconds)
{
    if (seconds < 0.0)
        return;
    for (const auto& slice : slices)
        if (std::abs (slice.time - seconds) < 0.004)
            return;
    slices.push_back ({ seconds, 0.6f });
    std::sort (slices.begin(), slices.end(), [] (const sliceforge::Slice& a, const sliceforge::Slice& b) { return a.time < b.time; });
    postUiChange();
}

void SliceForgeAudioProcessor::deleteNearestSlice (double seconds)
{
    if (slices.empty())
        return;
    int nearest = 0;
    double best = std::numeric_limits<double>::max();
    for (int i = 0; i < (int) slices.size(); ++i)
    {
        const auto d = std::abs (slices[(size_t) i].time - seconds);
        if (d < best)
        {
            best = d;
            nearest = i;
        }
    }
    slices.erase (slices.begin() + nearest);
    postUiChange();
}

void SliceForgeAudioProcessor::applyFactoryPresetFromUi (int index)
{
    applyFactoryPresetInternal (index, true);
}

void SliceForgeAudioProcessor::randomizePads()
{
    pushUndoSnapshot();
    sliceforge::Rng rng ((uint32_t) juce::Time::currentTimeMillis());
    auto pads = sliceforge::makeDefaultPads();
    const int roles[] { 0, 0, 1, 3, 1, 2, 2, 2, 2, 2, 3, 3, 2, 2, 1, 3 };
    const float lengths[] { 0.20f, 0.55f, 0.10f, 0.42f };
    for (int i = 0; i < sliceforge::kNumPads; ++i)
    {
        auto& pad = pads[(size_t) i];
        const auto family = roles[i];
        pad.modelIndex = family * sliceforge::kModelsPerFamily + rng.rangeInt (0, sliceforge::kModelsPerFamily - 1);
        pad.drumify = rng.range (0.34f, 0.96f);
        pad.length = lengths[(size_t) family] + rng.range (-0.05f, 0.18f);
        pad.pitch = rng.chance (0.35f) ? (float) rng.rangeInt (-4, 4) : 0.0f;
        pad.pan = family == sliceforge::hat || family == sliceforge::clap ? rng.range (-0.35f, 0.35f) : rng.range (-0.1f, 0.1f);
        pad.choke = family == sliceforge::hat ? 2 : 0;
        pad.warp = rng.range (0.0f, 0.65f);
        pad.impact = rng.range (0.1f, 0.8f);
        pad.instability = rng.range (0.0f, 0.3f);
        pad.super = rng.range (0.0f, 0.55f);
        pad.velocityAmount = rng.range (0.35f, 0.9f);
        parameterCache.writePad (i, pad);
    }
    auto global = parameterCache.readGlobal();
    global.tone = juce::jlimit (-12.0f, 12.0f, global.tone + rng.range (-3.0f, 3.0f));
    global.filter = juce::jlimit (-0.5f, 0.5f, global.filter + rng.range (-0.12f, 0.12f));
    global.width = juce::jlimit (0.0f, 1.0f, global.width + rng.range (-0.2f, 0.2f));
    parameterCache.writeGlobal (global);
    currentPresetName = "RANDOM KIT";
    postUiChange();
}

void SliceForgeAudioProcessor::pushUndoSnapshot()
{
    if (restoringState)
        return;
    Snapshot snapshot;
    snapshot.parameterState = apvts.copyState();
    snapshot.slices = slices;
    for (int i = 0; i < sliceforge::kNumPads; ++i)
        snapshot.starts[(size_t) i] = padStarts[(size_t) i].load (std::memory_order_relaxed);
    snapshot.selectedPad = selectedPad.load (std::memory_order_relaxed);
    snapshot.presetName = currentPresetName;
    snapshot.programIndex = currentProgramIndex.load (std::memory_order_relaxed);
    snapshot.sequencer = sequencerData.load (std::memory_order_acquire);
    undoStack.push_back (std::move (snapshot));
    if (undoStack.size() > 50)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void SliceForgeAudioProcessor::restoreSnapshot (Snapshot snapshot)
{
    restoringState = true;
    apvts.replaceState (snapshot.parameterState);
    slices = std::move (snapshot.slices);
    for (int i = 0; i < sliceforge::kNumPads; ++i)
        padStarts[(size_t) i].store (snapshot.starts[(size_t) i], std::memory_order_relaxed);
    selectedPad.store (juce::jlimit (0, sliceforge::kNumPads - 1, snapshot.selectedPad), std::memory_order_relaxed);
    currentPresetName = snapshot.presetName.isNotEmpty() ? snapshot.presetName : currentPresetName;
    currentProgramIndex.store (juce::jlimit (0, getNumPrograms() - 1, snapshot.programIndex),
                               std::memory_order_relaxed);
    if (snapshot.sequencer != nullptr)
    {
        sequencerData.store (snapshot.sequencer, std::memory_order_release);
        seqResetRequested.store (true, std::memory_order_relaxed);
    }
    markHostStateDirty();
    restoringState = false;
    postUiChange();
}

void SliceForgeAudioProcessor::undo()
{
    if (undoStack.empty())
        return;
    Snapshot current;
    current.parameterState = apvts.copyState();
    current.slices = slices;
    for (int i = 0; i < sliceforge::kNumPads; ++i)
        current.starts[(size_t) i] = padStarts[(size_t) i].load (std::memory_order_relaxed);
    current.selectedPad = selectedPad.load (std::memory_order_relaxed);
    current.presetName = currentPresetName;
    current.programIndex = currentProgramIndex.load (std::memory_order_relaxed);
    current.sequencer = sequencerData.load (std::memory_order_acquire);
    redoStack.push_back (std::move (current));
    auto snapshot = std::move (undoStack.back());
    undoStack.pop_back();
    restoreSnapshot (std::move (snapshot));
}

void SliceForgeAudioProcessor::redo()
{
    if (redoStack.empty())
        return;
    Snapshot current;
    current.parameterState = apvts.copyState();
    current.slices = slices;
    for (int i = 0; i < sliceforge::kNumPads; ++i)
        current.starts[(size_t) i] = padStarts[(size_t) i].load (std::memory_order_relaxed);
    current.selectedPad = selectedPad.load (std::memory_order_relaxed);
    current.presetName = currentPresetName;
    current.programIndex = currentProgramIndex.load (std::memory_order_relaxed);
    current.sequencer = sequencerData.load (std::memory_order_acquire);
    undoStack.push_back (std::move (current));
    auto snapshot = std::move (redoStack.back());
    redoStack.pop_back();
    restoreSnapshot (std::move (snapshot));
}

void SliceForgeAudioProcessor::armMidiLearn (const juce::String& parameterId)
{
    midiLearnTarget = parameterId;
    postUiChange();
}

void SliceForgeAudioProcessor::cancelMidiLearn()
{
    midiLearnTarget.clear();
    postUiChange();
}

void SliceForgeAudioProcessor::toggleRecording()
{
    if (recordingActive.load (std::memory_order_relaxed))
    {
        recordingActive.store (false, std::memory_order_relaxed);
        postUiChange();
        return;
    }

    const auto maxSamples = juce::jmax (1, kMaxRecordBytes / (2 * (int) sizeof (float)));
    const auto wanted = juce::jmax (1, (int) std::round (currentSampleRate * 120.0));
    recordBuffer.setSize (2, juce::jmin (maxSamples, wanted), false, true, false);
    recordBuffer.clear();
    recordedSamples.store (0, std::memory_order_relaxed);
    recordingTakeReady.store (false, std::memory_order_relaxed);
    audioWasRecording = false;
    recordSampleRate = (int) std::round (currentSampleRate);
    recordingActive.store (true, std::memory_order_relaxed);
    postUiChange();
}

double SliceForgeAudioProcessor::getRecordingSeconds() const
{
    return recordSampleRate > 0 ? (double) recordedSamples.load (std::memory_order_relaxed) / (double) recordSampleRate : 0.0;
}

void SliceForgeAudioProcessor::adoptRecordingTake()
{
    if (! recordingTakeReady.load (std::memory_order_relaxed))
        return;
    const auto numSamples = recordedSamples.load (std::memory_order_relaxed);
    if (numSamples <= 0)
    {
        discardRecordingTake();
        return;
    }
    auto sample = std::make_shared<sliceforge::SampleData>();
    sample->name = "RECORDED INPUT";
    sample->id = "custom-" + juce::String (juce::Time::currentTimeMillis());
    sample->sampleRate = (double) recordSampleRate;
    sample->buffer.setSize (2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        sample->buffer.setSample (0, i, recordBuffer.getSample (0, i));
        sample->buffer.setSample (1, i, recordBuffer.getSample (1, i));
    }
    sample->buildPeaks();
    engine.setSample (sample);
    sourceId = sample->id;
    sourceName = sample->name;
    sourceIsFactory = false;
    customSamplePcm = sliceforge::encodePcm16 (*sample);
    resliceAndAssignStarts (false);
    undoStack.clear();
    redoStack.clear();
    recordingTakeReady.store (false, std::memory_order_relaxed);
    postUiChange();
}

void SliceForgeAudioProcessor::discardRecordingTake()
{
    recordingTakeReady.store (false, std::memory_order_relaxed);
    recordedSamples.store (0, std::memory_order_relaxed);
    postUiChange();
}

void SliceForgeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto extra = state.getChildWithName ("VRAC_EXTRA");
    if (extra.isValid())
        state.removeChild (extra, nullptr);

    extra = juce::ValueTree ("VRAC_EXTRA");
    extra.setProperty ("sourceId", sourceId, nullptr);
    extra.setProperty ("sourceName", sourceName, nullptr);
    extra.setProperty ("sourceIsFactory", sourceIsFactory, nullptr);
    extra.setProperty ("program", currentProgramIndex.load (std::memory_order_relaxed), nullptr);
    extra.setProperty ("presetName", currentPresetName, nullptr);
    extra.setProperty ("selectedPad", selectedPad.load (std::memory_order_relaxed), nullptr);
    if (! sourceIsFactory && customSamplePcm.getSize() > 0)
    {
        extra.setProperty ("customPcm", customSamplePcm, nullptr);
        if (const auto sample = engine.getSample())
            extra.setProperty ("customSampleRate", sample->sampleRate, nullptr);
    }

    juce::ValueTree sliceTree ("SLICES");
    for (const auto& slice : slices)
    {
        juce::ValueTree s ("SLICE");
        s.setProperty ("time", slice.time, nullptr);
        s.setProperty ("strength", slice.strength, nullptr);
        sliceTree.addChild (s, -1, nullptr);
    }
    extra.addChild (sliceTree, -1, nullptr);

    juce::ValueTree starts ("STARTS");
    for (int i = 0; i < sliceforge::kNumPads; ++i)
        starts.setProperty ("s" + juce::String (i), (double) padStarts[(size_t) i].load (std::memory_order_relaxed), nullptr);
    extra.addChild (starts, -1, nullptr);

    juce::ValueTree mappings ("MAPPINGS");
    for (int cc = 0; cc < 128; ++cc)
    {
        if (ccMappings[(size_t) cc].isNotEmpty())
        {
            juce::ValueTree map ("MAP");
            map.setProperty ("cc", cc, nullptr);
            map.setProperty ("param", ccMappings[(size_t) cc], nullptr);
            mappings.addChild (map, -1, nullptr);
        }
    }
    extra.addChild (mappings, -1, nullptr);

    if (const auto sequence = sequencerData.load (std::memory_order_acquire))
    {
        // The saved field is a copy of the live selector, not of whatever the document happens to
        // carry, so a session round-trips the pattern the panel is showing. That is the field's
        // meaning: the pattern standalone mode plays and the next session resumes on. In song mode the
        // chain owns what you hear, and neither this field nor the parameter is what was audible, so
        // this stamp buys no "the host automated it and the save followed" benefit here - it just keeps
        // the standby selector honest. Same slot in the blob, same field, same version: the serialiser
        // is untouched.
        auto forSave = sliceforge::cloneSequencerData (*sequence);
        forSave->activePattern = getPatternParameter();
        extra.setProperty ("sequencer", sliceforge::serialiseSequencer (*forSave), nullptr);
    }

    state.addChild (extra, -1, nullptr);

    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void SliceForgeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData (data, (size_t) juce::jmax (0, sizeInBytes));
    if (! state.isValid())
        return;

    restoringState = true;
    auto extra = state.getChildWithName ("VRAC_EXTRA");
    auto parameterState = state.createCopy();
    if (extra.isValid())
        parameterState.removeChild (extra, nullptr);
    else
        extra = juce::ValueTree ("VRAC_EXTRA");
    // Presence, not value: a session written before seq.pattern existed carries no child for it,
    // and then the pattern stored inside the document blob is the only record of what was playing.
    const bool stateCarriesPattern
        = parameterState.getChildWithProperty ("id", "seq.pattern").isValid();
    apvts.replaceState (parameterState);

    sourceId = extra.getProperty ("sourceId", "foley").toString();
    sourceName = extra.getProperty ("sourceName", "FOLEY TRASH").toString();
    sourceIsFactory = (bool) extra.getProperty ("sourceIsFactory", true);
    currentProgramIndex.store (juce::jlimit (0, getNumPrograms() - 1, (int) extra.getProperty ("program", 0)),
                               std::memory_order_relaxed);
    currentPresetName = extra.getProperty ("presetName", getProgramName (currentProgramIndex.load())).toString();
    selectedPad.store (juce::jlimit (0, sliceforge::kNumPads - 1, (int) extra.getProperty ("selectedPad", 3)),
                       std::memory_order_relaxed);

    sliceforge::SamplePtr sample;
    customSamplePcm.reset();
    stateNotice.clear();
    if (sourceIsFactory)
    {
        sample = sliceforge::loadFactorySource (sourceId);
        if (sample == nullptr)
        {
            stateNotice = "SOURCE '" + sourceId + "' NOT IN FACTORY LIBRARY";
            sourceId = "foley";
            sample = sliceforge::loadFactorySource (sourceId);
            juce::Logger::writeToLog ("SliceForge: " + stateNotice);
        }
    }
    else
    {
        if (auto* pcm = extra.getProperty ("customPcm").getBinaryData())
        {
            customSamplePcm = *pcm;
            auto rate = (double) extra.getProperty ("customSampleRate", 48000.0);
            if (! std::isfinite (rate) || rate < 8000.0 || rate > 192000.0)
                rate = 48000.0;
            sample = sliceforge::decodePcm16 (customSamplePcm, 2, rate, sourceName);
        }
        if (sample == nullptr)
        {
            /* The embedded recording is unreadable, so there is nothing custom left to restore.
               Half-restoring was the bug: the engine kept the previous session's sample while the
               slice map below was cleared and the header showed this source's name, which plays
               audio the interface does not describe. The identity is rewritten to the source that
               is actually loaded, and the unreadable bytes go with it - keeping them would let the
               next save claim a live custom source that cannot be decoded twice. */
            customSamplePcm.reset();
            sourceId = "foley";
            sourceIsFactory = true;
            sample = sliceforge::loadFactorySource (sourceId);
            stateNotice = "RECORDING LOST - COULD NOT BE READ, LOADED FOLEY TRASH";
            juce::Logger::writeToLog ("SliceForge: " + stateNotice);
        }
    }
    if (sample != nullptr)
    {
        engine.setSample (sample);
        sourceName = sample->name;
    }

    slices.clear();
    if (auto sliceTree = extra.getChildWithName ("SLICES"); sliceTree.isValid())
        for (int i = 0; i < sliceTree.getNumChildren(); ++i)
        {
            const auto child = sliceTree.getChild (i);
            slices.push_back ({ (double) child.getProperty ("time", 0.0), (float) child.getProperty ("strength", 0.5f) });
        }
    if (slices.empty() && sample != nullptr)
        slices = sliceforge::detectSlices (*sample, juce::jlimit (0.0f, 1.0f, parameterCache.readSensitivity()));

    auto starts = extra.getChildWithName ("STARTS");
    for (int i = 0; i < sliceforge::kNumPads; ++i)
    {
        float start = 0.0f;
        if (starts.isValid())
            start = (float) (double) starts.getProperty ("s" + juce::String (i), 0.0);
        else if (! slices.empty())
            start = (float) slices[(size_t) juce::jmin ((int) slices.size() - 1, i)].time;
        padStarts[(size_t) i].store (start, std::memory_order_relaxed);
    }

    for (auto& mapping : ccMappings)
        mapping.clear();
    if (auto mappings = extra.getChildWithName ("MAPPINGS"); mappings.isValid())
        for (int i = 0; i < mappings.getNumChildren(); ++i)
        {
            const auto child = mappings.getChild (i);
            const int cc = (int) child.getProperty ("cc", -1);
            if (cc >= 0 && cc < 128)
                ccMappings[(size_t) cc] = child.getProperty ("param", "").toString();
        }

    std::shared_ptr<sliceforge::SequencerData> loadedSequence;
    const auto* sequenceBlob = extra.getProperty ("sequencer").getBinaryData();
    if (sequenceBlob != nullptr)
        loadedSequence = sliceforge::deserialiseSequencer (*sequenceBlob);
    if (loadedSequence == nullptr && sequenceBlob != nullptr)
    {
        stateNotice = stateNotice.isNotEmpty()
                          ? stateNotice + "  /  PATTERNS CORRUPT"
                          : juce::String ("PATTERNS COULD NOT BE READ - RESET TO DEFAULTS");
        juce::Logger::writeToLog ("SliceForge: " + stateNotice);
    }
    if (loadedSequence == nullptr)
        loadedSequence = sliceforge::cloneSequencerData (*sliceforge::makeDefaultSequencerData());
    /*  Two records of one choice, and they can disagree. seq.pattern is the automation lane; the
        field inside the pattern blob is what a document stored before the parameter existed.
        A state that carries the parameter wins, because that is the value the host will drive from
        here on, and the document's copy is restamped so the grid, the exporter and the next save
        describe the pattern that is actually playing. A state that does not carry it is an older
        session - the blob is the only witness, so it initialises the parameter instead. Either way
        the serialised field keeps its slot and its meaning: nothing here renumbers the format. */
    if (stateCarriesPattern)
        loadedSequence->activePattern = getPatternParameter();
    else
        setPatternParameter (loadedSequence->activePattern);
    sequencerData.store (loadedSequence, std::memory_order_release);
    seqInternalPlaying.store (false, std::memory_order_relaxed);
    seqResetRequested.store (true, std::memory_order_relaxed);

    midiLearnTarget.clear();
    restoringState = false;
    postUiChange();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SliceForgeAudioProcessor();
}




