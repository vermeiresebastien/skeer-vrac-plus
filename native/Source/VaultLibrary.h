#pragma once

#include <JuceHeader.h>

#include <cstdint>
#include <vector>

namespace vrac
{
    struct VaultTrack
    {
        int64_t id = 0;
        juce::String path;
        juce::String filename;
        juce::String folder;
        juce::String format;
        juce::String family;
        juce::String soundType;
        juce::String keyRoot;
        juce::String keyMode;
        juce::String tags;
        juce::String notes;
        double bpm = 0.0;
        double durationMs = 0.0;
        int sampleRate = 0;
        int channels = 0;
        bool favorite = false;
        bool analyzed = false;

        // Filled only by queryTrackDetails(); sourced from the vault's indexed
        // waveform peaks so VRAC can preview a sample without decoding it.
        std::vector<float> waveformMin;
        std::vector<float> waveformMax;
    };

    struct VaultCollection
    {
        int id = 0;
        juce::String name;
        juce::String colour;
    };

    struct VaultQuery
    {
        juce::String search;
        juce::String family;
        juce::String soundType;
        int collectionId = 0;
        bool favoritesOnly = false;
        int sortMode = 0; // 0 name, 1 recent, 2 duration, 3 bpm
        int limit = 500;
    };

    struct VaultStatus
    {
        bool available = false;
        juce::String error;
        juce::File database;
        int totalTracks = 0;
        int analyzedTracks = 0;
        int favoriteTracks = 0;
        int collectionCount = 0;
        juce::String schemaVersion;
        juce::String appVersion;
        juce::Time lastWrite;
    };

    class VaultLibrary
    {
    public:
        VaultLibrary();

        bool refreshStatus();
        bool isAvailable() const noexcept { return status.available; }
        const VaultStatus& getStatus() const noexcept { return status; }
        juce::File getDatabaseFile() const noexcept { return status.database; }

        bool setDatabaseFile (const juce::File& file, juce::String& error);
        void clearSavedDatabase();

        std::vector<VaultTrack> queryTracks (const VaultQuery& query, juce::String* error = nullptr);
        std::vector<VaultCollection> queryCollections();
        bool queryTrackDetails (int64_t trackId, VaultTrack& result);

        static juce::File findDatabaseFile();

    private:
        VaultStatus status;
    };
} // namespace vrac
