#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "Presets.h"
#include "Engine/VoiceEngine.h"
#include "Engine/PreviewPlayer.h"
#include "Engine/SampleData.h"
#include "Engine/Slicer.h"
#include "Sequencer.h"

#include <array>
#include <atomic>
#include <memory>
#include <vector>

class SliceForgeAudioProcessor final : public juce::AudioProcessor,
                                     private juce::AsyncUpdater
{
public:
    SliceForgeAudioProcessor();
    ~SliceForgeAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SliceForge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    sliceforge::ParameterCache& getParameterCache() noexcept { return parameterCache; }

    // ---- UI-facing API -------------------------------------------------
    juce::ChangeBroadcaster& getUiBroadcaster() noexcept { return uiBroadcaster; }

    sliceforge::SamplePtr getSamplePtr() const { return engine.getSample(); }
    const std::vector<sliceforge::Slice>& getSlices() const noexcept { return slices; }
    double getPadStart (int pad) const;
    void setPadStart (int pad, double seconds);
    int getSelectedPad() const noexcept { return selectedPad.load (std::memory_order_relaxed); }
    void setSelectedPad (int pad);
    juce::String getSourceName() const;
    juce::String getSourceId() const;
    bool sourceIsFactorySample() const noexcept { return sourceIsFactory; }
    juce::String getPresetName() const;
    /** Why the last session reload could not restore everything it was given, empty when it
        could. A rejected blob or an undecodable recording is otherwise invisible. */
    const juce::String& getStateNotice() const noexcept { return stateNotice; }
    juce::String getMidiLearnTarget() const;
    juce::String getMidiMappingForCc (int cc) const;
    int getLastTriggeredPad() const noexcept { return lastTriggeredPad.load (std::memory_order_relaxed); }
    uint32_t getTriggerStamp() const noexcept { return triggerStamp.load (std::memory_order_relaxed); }
    uint32_t getPadHitStamp (int pad) const;
    float getBusPeak (int bus) const;
    int getActiveVoiceCount() const { return engine.getActiveVoiceCount(); }
    /** Cumulative hits a block could not fit, from either the sequencer's trigger list or the
        processor's own event cap. A dropped hit is audible as a missing note, so it is counted
        instead of swallowed. */
    int getDroppedHitCount() const noexcept { return droppedHits.load (std::memory_order_relaxed); }
    bool isSidechainInputActive() const noexcept { return sidechainActive.load (std::memory_order_relaxed); }
    double getEngineSampleRate() const noexcept { return currentSampleRate; }

    // Audition transport the Sample Vault clicks through. Mixes into output bus 0.
    sliceforge::PreviewPlayer& getPreviewPlayer() noexcept { return preview; }
    const sliceforge::PreviewPlayer& getPreviewPlayer() const noexcept { return preview; }

    void triggerPad (int pad, float velocity);
    void chokeAll();
    void loadFactorySource (const juce::String& sourceId);
    bool loadUserSampleFromFile (const juce::File& file);
    void onSensitivityChanged();
    void addSliceAt (double seconds);
    void deleteNearestSlice (double seconds);
    void applyFactoryPresetFromUi (int index);
    void randomizePads();
    void pushUndoSnapshot();
    void undo();
    void redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    void armMidiLearn (const juce::String& parameterId);
    void cancelMidiLearn();
    void toggleRecording();
    bool isRecording() const noexcept { return recordingActive.load (std::memory_order_relaxed); }
    bool isRecordingTakeReady() const noexcept { return recordingTakeReady.load (std::memory_order_relaxed); }
    double getRecordingSeconds() const;
    void adoptRecordingTake();
    void discardRecordingTake();

    std::shared_ptr<const sliceforge::SequencerData> getSequencerData() const;
    void setSequencerData (std::shared_ptr<const sliceforge::SequencerData> data);
    void setSequencerInternalPlaying (bool shouldPlay);
    bool isSequencerInternalPlaying() const noexcept { return seqInternalPlaying.load (std::memory_order_relaxed); }
    bool isSequencerHostPlaying() const noexcept { return seqHostPlaying.load (std::memory_order_relaxed); }
    bool isSequencerTransportPlaying() const noexcept { return seqTransportPlaying.load (std::memory_order_relaxed); }
    int getSequencerPlayheadStep() const noexcept { return seqPlayheadStep.load (std::memory_order_relaxed); }
    double getSequencerHostBpm() const noexcept { return seqHostBpm.load (std::memory_order_relaxed); }

    // ---- internal helpers ----------------------------------------------
    void postUiChange() { uiBroadcaster.sendChangeMessage(); }

private:
    struct PlaybackEvent
    {
        int sampleOffset = 0;
        int pad = 0;
        float velocity = 0.8f;
        bool followAutoSelection = true;
        bool hasLocks = false;
        sliceforge::StepLocks locks {};
    };

    std::vector<PlaybackEvent> playbackEvents;   // reused every block: a fresh 1024-element array
                                                  // would run its default members over per block
    int playbackEventCapacity = 512;              // bounded so a pathological block cannot grow here
    std::atomic<int> droppedHits { 0 };           // sequencer + playback events that did not fit

    struct Snapshot
    {
        juce::ValueTree parameterState;
        std::vector<sliceforge::Slice> slices;
        std::array<float, sliceforge::kNumPads> starts {};
        int selectedPad = 0;
        juce::String presetName;
        int programIndex = 0;
        std::shared_ptr<const sliceforge::SequencerData> sequencer;
    };

    void handleAsyncUpdate() override;
    void applyFactoryPresetInternal (int index, bool clearHistory);
    void setParameterValue (const juce::String& id, float value);
    void setParameterNormalised (const juce::String& id, float value);
    void buildRuntime();
    void resliceAndAssignStarts (bool remapExisting);
    void applyCcOnMessageThread (int cc, float value);
    void markHostStateDirty();
    void restoreSnapshot (Snapshot snapshot);
    void copyEngineBusesToHost (juce::AudioBuffer<float>& buffer, int numSamples);
    void updatePadHit (int pad, bool followAutoSelection = true);
    void serviceRecording (const juce::AudioBuffer<float>* input, int numSamples);
    void resetSequencerPlayback();

    juce::AudioProcessorValueTreeState apvts;
    sliceforge::ParameterCache parameterCache;
    sliceforge::DrumEngine engine;
    sliceforge::PreviewPlayer preview;
    sliceforge::OutputStage outputStage;   // soft clipper -> OUT GAIN -> safety ceiling

    std::vector<sliceforge::Slice> slices;
    std::array<std::atomic<float>, sliceforge::kNumPads> padStarts {};
    std::atomic<int> selectedPad { 3 };
    std::atomic<int> lastTriggeredPad { -1 };
    std::atomic<uint32_t> triggerStamp { 0 };
    std::array<std::atomic<uint32_t>, sliceforge::kNumPads> padHitStamps {};

    std::atomic<int> currentProgramIndex { 0 };
    juce::String currentPresetName { "BIG CAT" };
    juce::String sourceId { "foley" };
    juce::String sourceName { "FOLEY TRASH" };
    bool sourceIsFactory = true;
    juce::MemoryBlock customSamplePcm;
    juce::String stateNotice;

    std::atomic<bool> sidechainActive { false };
    std::atomic<bool> recordingActive { false };
    std::atomic<bool> recordingTakeReady { false };
    std::atomic<int> recordedSamples { 0 };
    juce::AudioBuffer<float> recordBuffer;
    bool audioWasRecording = false;
    int recordSampleRate = 48000;

    std::array<juce::String, 128> ccMappings;
    juce::String midiLearnTarget;
    std::atomic<uint32_t> lastCcSeen { 0 };
    struct CcEvent { int cc = 0; float value = 0.0f; };
    juce::AbstractFifo ccFifo { 256 };
    std::array<CcEvent, 256> ccData {};

    struct UiTrigger { int pad = 0; float velocity = 0.0f; };
    juce::AbstractFifo uiTriggerFifo { 128 };
    std::array<UiTrigger, 128> uiTriggerData {};

    std::atomic<std::shared_ptr<const sliceforge::SequencerData>> sequencerData;
    std::atomic<bool> seqInternalPlaying { false };
    std::atomic<bool> seqHostPlaying { false };
    std::atomic<bool> seqTransportPlaying { false };
    std::atomic<bool> seqResetRequested { false };
    std::atomic<bool> hostStateChanged { false };
    std::atomic<int> seqPlayheadStep { -1 };
    std::atomic<double> seqHostBpm { 128.0 };
    sliceforge::SeqPlaybackState seqPlaybackState;
    juce::Random seqRandom;

    std::vector<Snapshot> undoStack;
    std::vector<Snapshot> redoStack;
    juce::ChangeBroadcaster uiBroadcaster;

    double currentSampleRate = 48000.0;
    int maxBlockSize = 512;
    bool wasPrepared = false;
    bool restoringState = false;
    std::atomic<bool> chokeRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SliceForgeAudioProcessor)
};
