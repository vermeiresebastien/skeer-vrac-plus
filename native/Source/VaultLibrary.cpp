#include "VaultLibrary.h"

#include <algorithm>
#include <cstring>
#include <type_traits>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

namespace sliceforge
{
    namespace
    {
        constexpr int kSqliteOpenReadOnly = 0x00000001;
        constexpr int kSqliteOpenNoMutex = 0x00008000;
        constexpr int kSqliteOk = 0;
        constexpr int kSqliteRow = 100;
        constexpr int kSqliteDone = 101;

#if JUCE_WINDOWS
        struct SqliteApi
        {
            using OpenV2 = int (*) (const char*, void**, int, const char*);
            using Close = int (*) (void*);
            using Prepare = int (*) (void*, const char*, int, void**, const char**);
            using Step = int (*) (void*);
            using Finalize = int (*) (void*);
            using ColumnInt64 = long long (*) (void*, int);
            using ColumnInt = int (*) (void*, int);
            using ColumnDouble = double (*) (void*, int);
            using ColumnText = const unsigned char* (*) (void*, int);
            using ColumnBytes = int (*) (void*, int);
            using ColumnType = int (*) (void*, int);
            using BindText = int (*) (void*, int, const char*, int, void (*) (void*));
            using BindInt = int (*) (void*, int, int);
            using BindInt64 = int (*) (void*, int, long long);
            using ErrMsg = const char* (*) (void*);
            using BusyTimeout = int (*) (void*, int);

            HMODULE module = nullptr;
            OpenV2 openV2 = nullptr;
            Close close = nullptr;
            Prepare prepare = nullptr;
            Step step = nullptr;
            Finalize finalize = nullptr;
            ColumnInt64 columnInt64 = nullptr;
            ColumnInt columnInt = nullptr;
            ColumnDouble columnDouble = nullptr;
            ColumnText columnText = nullptr;
            ColumnBytes columnBytes = nullptr;
            ColumnType columnType = nullptr;
            BindText bindText = nullptr;
            BindInt bindInt = nullptr;
            BindInt64 bindInt64 = nullptr;
            ErrMsg errMsg = nullptr;
            BusyTimeout busyTimeout = nullptr;
            bool ready = false;

            bool load()
            {
                module = LoadLibraryW (L"winsqlite3.dll");
                if (module == nullptr)
                    module = LoadLibraryW (L"sqlite3.dll");
                if (module == nullptr)
                    return false;

                auto resolve = [this] (auto& target, const char* name)
                {
                    target = reinterpret_cast<std::decay_t<decltype (target)>> (
                        GetProcAddress (module, name));
                    return target != nullptr;
                };

                ready = resolve (openV2, "sqlite3_open_v2")
                     && resolve (close, "sqlite3_close")
                     && resolve (prepare, "sqlite3_prepare_v2")
                     && resolve (step, "sqlite3_step")
                     && resolve (finalize, "sqlite3_finalize")
                     && resolve (columnInt64, "sqlite3_column_int64")
                     && resolve (columnInt, "sqlite3_column_int")
                     && resolve (columnDouble, "sqlite3_column_double")
                     && resolve (columnText, "sqlite3_column_text")
                     && resolve (columnBytes, "sqlite3_column_bytes")
                     && resolve (columnType, "sqlite3_column_type")
                     && resolve (bindText, "sqlite3_bind_text")
                     && resolve (bindInt, "sqlite3_bind_int")
                     && resolve (bindInt64, "sqlite3_bind_int64")
                     && resolve (errMsg, "sqlite3_errmsg")
                     && resolve (busyTimeout, "sqlite3_busy_timeout");
                return ready;
            }
        };

        SqliteApi& sqliteApi()
        {
            static SqliteApi api;
            static const bool loaded = api.load();
            juce::ignoreUnused (loaded);
            return api;
        }
#else
        struct SqliteApi
        {
            bool ready = false;
        };
        SqliteApi& sqliteApi()
        {
            static SqliteApi api;
            return api;
        }
#endif

        struct SqliteDb
        {
            void* handle = nullptr;
            ~SqliteDb()
            {
                if (handle != nullptr && sqliteApi().ready)
                    sqliteApi().close (handle);
            }
        };

        struct SqliteStatement
        {
            void* handle = nullptr;
            ~SqliteStatement()
            {
                if (handle != nullptr && sqliteApi().ready)
                    sqliteApi().finalize (handle);
            }
        };

        juce::String configFilePath()
        {
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                .getChildFile ("SliceForge")
                .getChildFile ("vault-path.txt")
                .getFullPathName();
        }

        juce::String savedDatabasePath()
        {
            const juce::File config (configFilePath());
            if (! config.existsAsFile())
                return {};
            return config.loadFileAsString().trim();
        }

        bool isVaultDatabase (const juce::File& file);

        bool openDatabase (const juce::File& file, SqliteDb& db, juce::String& error)
        {
            const auto& api = sqliteApi();
            if (! api.ready)
            {
                error = "Windows SQLite (winsqlite3.dll) is unavailable.";
                return false;
            }
            if (! file.existsAsFile())
            {
                error = "SKEER Sample Vault database not found: " + file.getFullPathName();
                return false;
            }

            const auto path = file.getFullPathName().toRawUTF8();
            const auto result = api.openV2 (path, &db.handle, kSqliteOpenReadOnly | kSqliteOpenNoMutex, nullptr);
            if (result != kSqliteOk || db.handle == nullptr)
            {
                error = "Could not open the SKEER Sample Vault database";
                if (db.handle != nullptr)
                {
                    const auto* msg = api.errMsg (db.handle);
                    if (msg != nullptr)
                        error += juce::String (": ") + juce::String::fromUTF8 (msg);
                }
                return false;
            }
            api.busyTimeout (db.handle, 400);
            return true;
        }

        bool prepare (void* db, const juce::String& sql, SqliteStatement& statement, juce::String& error)
        {
            const auto& api = sqliteApi();
            const auto utf8 = sql.toRawUTF8();
            const auto result = api.prepare (db, utf8, -1, &statement.handle, nullptr);
            if (result != kSqliteOk || statement.handle == nullptr)
            {
                error = "SKEER Sample Vault query failed";
                if (db != nullptr)
                {
                    const auto* msg = api.errMsg (db);
                    if (msg != nullptr)
                        error += juce::String (": ") + juce::String::fromUTF8 (msg);
                }
                return false;
            }
            return true;
        }

        void bindText (void* statement, int index, const juce::String& value)
        {
            const auto& api = sqliteApi();
            const auto utf8 = value.toRawUTF8();
            api.bindText (statement, index, utf8, (int) value.getNumBytesAsUTF8(),
                          reinterpret_cast<void (*) (void*)> (-1));
        }

        void bindInt (void* statement, int index, int value)
        {
            sqliteApi().bindInt (statement, index, value);
        }

        void bindInt64 (void* statement, int index, int64_t value)
        {
            sqliteApi().bindInt64 (statement, index, (long long) value);
        }

        juce::String columnText (void* statement, int index)
        {
            const auto& api = sqliteApi();
            if (api.columnType (statement, index) == 5) // SQLITE_NULL
                return {};
            const auto* text = api.columnText (statement, index);
            if (text == nullptr)
                return {};
            return juce::String::fromUTF8 (reinterpret_cast<const char*> (text),
                                           api.columnBytes (statement, index));
        }

        int64_t columnInt64 (void* statement, int index)
        {
            return (int64_t) sqliteApi().columnInt64 (statement, index);
        }

        int columnInt (void* statement, int index)
        {
            return sqliteApi().columnInt (statement, index);
        }

        double columnDouble (void* statement, int index)
        {
            return sqliteApi().columnDouble (statement, index);
        }

        bool querySingleInt (void* db, const juce::String& sql, int& value, juce::String& error)
        {
            SqliteStatement statement;
            if (! prepare (db, sql, statement, error))
                return false;
            const auto& api = sqliteApi();
            if (api.step (statement.handle) != kSqliteRow)
                return false;
            value = columnInt (statement.handle, 0);
            return true;
        }

        juce::String querySingleString (void* db, const juce::String& sql)
        {
            SqliteStatement statement;
            juce::String ignored;
            if (! prepare (db, sql, statement, ignored))
                return {};
            if (sqliteApi().step (statement.handle) != kSqliteRow)
                return {};
            return columnText (statement.handle, 0);
        }

        void appendJsonTags (const juce::String& json, juce::StringArray& tags)
        {
            if (json.isEmpty())
                return;
            const auto parsed = juce::JSON::parse (json);
            if (auto* array = parsed.getArray())
            {
                for (const auto& value : *array)
                {
                    const auto tag = value.toString().trim();
                    if (tag.isNotEmpty() && ! tags.contains (tag, true))
                        tags.add (tag);
                }
            }
        }

        void parseWaveform (const juce::String& json, VaultTrack& track)
        {
            if (json.isEmpty())
                return;
            const auto parsed = juce::JSON::parse (json);
            auto* object = parsed.getDynamicObject();
            if (object == nullptr)
                return;

            auto readArray = [] (const juce::var& value, std::vector<float>& destination)
            {
                if (auto* array = value.getArray())
                {
                    destination.reserve ((size_t) array->size());
                    for (const auto& entry : *array)
                        destination.push_back ((float) entry.toString().getDoubleValue());
                }
            };
            readArray (object->getProperty ("min"), track.waveformMin);
            readArray (object->getProperty ("max"), track.waveformMax);
        }

        juce::File normalizeDatabaseCandidate (const juce::String& path)
        {
            if (path.isEmpty())
                return {};
            auto file = juce::File (path.trim());
            if (file.isDirectory())
                file = file.getChildFile ("data").getChildFile ("vault.sqlite3");
            return file.existsAsFile() ? file : juce::File();
        }

        void addCandidate (juce::Array<juce::File>& candidates, const juce::String& path)
        {
            const auto file = normalizeDatabaseCandidate (path);
            if (file.existsAsFile() && ! candidates.contains (file))
                candidates.add (file);
        }

        void addDirectoryCandidates (juce::Array<juce::File>& candidates, juce::File directory)
        {
            for (int i = 0; i < 5 && directory.isDirectory(); ++i)
            {
                addCandidate (candidates, directory.getChildFile ("data").getChildFile ("vault.sqlite3").getFullPathName());
                addCandidate (candidates, directory.getChildFile ("vault.sqlite3").getFullPathName());
                directory = directory.getParentDirectory();
            }
        }

        void addSiblingProjectCandidates (juce::Array<juce::File>& candidates, juce::File start)
        {
            // Source checkouts commonly keep the two tools side by side. This is a
            // development convenience; installed Vault copies are found via local
            // app-data and the installer registry instead.
            auto directory = start.isDirectory() ? start : start.getParentDirectory();
            static const char* siblingNames[] { "SKEER AUDIO - AUDIOVAULT",
                                                "SKEER AUDIO - AUDIO VAULT",
                                                "SKEER SAMPLE VAULT",
                                                "SKEER Sample Vault" };
            for (int i = 0; i < 5 && directory.isDirectory(); ++i)
            {
                for (const auto* name : siblingNames)
                {
                    const auto project = directory.getChildFile (name);
                    if (project.isDirectory())
                        addDirectoryCandidates (candidates, project);
                }
                const auto parent = directory.getParentDirectory();
                if (parent == directory)
                    break;
                directory = parent;
            }
        }

#if JUCE_WINDOWS
        juce::String readVaultRegistry (const wchar_t* name)
        {
            wchar_t buffer[2048] {};
            DWORD size = (DWORD) sizeof (buffer);
            DWORD type = 0;
            const auto result = RegGetValueW (HKEY_CURRENT_USER,
                                              L"Software\\SKEER\\SampleVault",
                                              name,
                                              RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                                              &type, buffer, &size);
            if (result != ERROR_SUCCESS)
                return {};
            return juce::String (buffer);
        }
#endif

        bool isVaultDatabase (const juce::File& file)
        {
            if (! file.existsAsFile())
                return false;
            SqliteDb db;
            juce::String error;
            if (! openDatabase (file, db, error))
                return false;
            int count = 0;
            return querySingleInt (db.handle, "SELECT count(*) FROM tracks", count, error);
        }
    } // namespace

    juce::File VaultLibrary::findDatabaseFile()
    {
        juce::Array<juce::File> candidates;

        const auto envDb = juce::SystemStats::getEnvironmentVariable ("SKEER_VAULT_DB", {});
        addCandidate (candidates, envDb);

        const auto envData = juce::SystemStats::getEnvironmentVariable ("SKEER_VAULT_DATA", {});
        if (envData.isNotEmpty())
            addCandidate (candidates, juce::File (envData).getChildFile ("vault.sqlite3").getFullPathName());

        const auto envRoot = juce::SystemStats::getEnvironmentVariable ("SKEER_VAULT_ROOT", {});
        if (envRoot.isNotEmpty())
            addCandidate (candidates, juce::File (envRoot).getChildFile ("data").getChildFile ("vault.sqlite3").getFullPathName());

        addCandidate (candidates, savedDatabasePath());

        const auto localAppData = juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {});
        if (localAppData.isNotEmpty())
        {
            addCandidate (candidates, juce::File (localAppData).getChildFile ("SKEER Sample Vault")
                                                  .getChildFile ("data").getChildFile ("vault.sqlite3").getFullPathName());
            addCandidate (candidates, juce::File (localAppData).getChildFile ("Programs")
                                                  .getChildFile ("SKEER Sample Vault").getChildFile ("data")
                                                  .getChildFile ("vault.sqlite3").getFullPathName());
        }
        const auto appData = juce::SystemStats::getEnvironmentVariable ("APPDATA", {});
        if (appData.isNotEmpty())
        {
            addCandidate (candidates, juce::File (appData).getChildFile ("SKEER Sample Vault")
                                             .getChildFile ("data").getChildFile ("vault.sqlite3").getFullPathName());
        }

        addSiblingProjectCandidates (candidates, juce::File::getCurrentWorkingDirectory());
        addSiblingProjectCandidates (candidates,
                                     juce::File::getSpecialLocation (juce::File::currentExecutableFile));

#if JUCE_WINDOWS
        const auto installDir = readVaultRegistry (L"InstallDir");
        if (installDir.isNotEmpty())
            addDirectoryCandidates (candidates, juce::File (installDir));
        const auto servicePath = readVaultRegistry (L"ServicePath");
        if (servicePath.isNotEmpty())
        {
            auto serviceDir = juce::File (servicePath);
            if (serviceDir.isDirectory())
                addDirectoryCandidates (candidates, serviceDir);
            else
                addDirectoryCandidates (candidates, serviceDir.getParentDirectory());
        }
#endif

        for (const auto& candidate : candidates)
            if (isVaultDatabase (candidate))
                return candidate;
        return {};
    }

    VaultLibrary::VaultLibrary() = default;

    bool VaultLibrary::refreshStatus()
    {
        status = {};
        const auto database = findDatabaseFile();
        if (! database.existsAsFile())
        {
            status.error = "SKEER Sample Vault was not found. Install/open it once, or locate vault.sqlite3.";
            return false;
        }

        status.database = database;
        status.lastWrite = database.getLastModificationTime();

        SqliteDb db;
        juce::String error;
        if (! openDatabase (database, db, error))
        {
            status.error = error;
            return false;
        }

        if (! querySingleInt (db.handle, "SELECT count(*) FROM tracks WHERE missing=0",
                              status.totalTracks, error))
        {
            status.error = error.isNotEmpty() ? error : "The SKEER Sample Vault index has an unexpected schema.";
            return false;
        }
        querySingleInt (db.handle, "SELECT count(*) FROM tracks WHERE analysis_state='complete'",
                        status.analyzedTracks, error);
        querySingleInt (db.handle, "SELECT count(*) FROM tracks WHERE favorite=1", status.favoriteTracks, error);
        querySingleInt (db.handle, "SELECT count(*) FROM collections", status.collectionCount, error);
        status.schemaVersion = querySingleString (db.handle, "SELECT value FROM meta WHERE key='schema_version'");
        status.appVersion = querySingleString (db.handle, "SELECT value FROM meta WHERE key='app_version'");
        status.available = true;
        return true;
    }

    bool VaultLibrary::setDatabaseFile (const juce::File& file, juce::String& error)
    {
        if (! isVaultDatabase (file))
        {
            error = "That file is not a readable SKEER Sample Vault database.";
            return false;
        }
        const auto config = juce::File (configFilePath());
        config.getParentDirectory().createDirectory();
        config.replaceWithText (file.getFullPathName());
        refreshStatus();
        return status.available;
    }

    void VaultLibrary::clearSavedDatabase()
    {
        juce::File (configFilePath()).deleteFile();
        refreshStatus();
    }

    std::vector<VaultTrack> VaultLibrary::queryTracks (const VaultQuery& query, juce::String* error)
    {
        std::vector<VaultTrack> result;
        if (! status.available)
            refreshStatus();
        if (! status.available)
        {
            if (error != nullptr)
                *error = status.error;
            return result;
        }

        SqliteDb db;
        juce::String localError;
        if (! openDatabase (status.database, db, localError))
        {
            if (error != nullptr)
                *error = localError;
            return result;
        }

        const auto escapedSearch = query.search
            .replace ("\\", "\\\\")
            .replace ("%", "\\%")
            .replace ("_", "\\_");

        juce::String sql =
            "SELECT t.id,t.path,t.filename,t.folder_path,t.format,"
            "COALESCE(NULLIF(t.user_family,''),t.family,''),"
            "COALESCE(NULLIF(t.user_sound_type,''),t.sound_type,''),"
            "COALESCE(t.bpm,0),COALESCE(t.key_root,''),COALESCE(t.key_mode,''),"
            "COALESCE(t.duration_ms,0),COALESCE(t.sample_rate,0),COALESCE(t.channels,0),"
            "COALESCE(t.favorite,0),COALESCE(t.user_tags_json,''),COALESCE(t.auto_tags_json,''),"
            "COALESCE(t.notes,''),CASE WHEN t.analysis_state='complete' THEN 1 ELSE 0 END "
            "FROM tracks t WHERE t.missing=0";

        if (query.search.isNotEmpty())
            sql += " AND (t.filename LIKE ? ESCAPE '\\' OR t.folder_name LIKE ? ESCAPE '\\'"
                   " OR t.path LIKE ? ESCAPE '\\' OR t.user_tags_json LIKE ? ESCAPE '\\'"
                   " OR t.auto_tags_json LIKE ? ESCAPE '\\')";
        if (query.family.isNotEmpty())
            sql += " AND COALESCE(NULLIF(t.user_family,''),t.family,'') = ?";
        if (query.soundType.isNotEmpty())
            sql += " AND COALESCE(NULLIF(t.user_sound_type,''),t.sound_type,'') = ?";
        if (query.collectionId > 0)
            sql += " AND EXISTS (SELECT 1 FROM collection_tracks ct WHERE ct.track_id=t.id"
                   " AND ct.collection_id=?)";
        if (query.favoritesOnly)
            sql += " AND t.favorite=1";

        switch (query.sortMode)
        {
            case 1: sql += " ORDER BY t.updated_at DESC, t.filename COLLATE NOCASE ASC"; break;
            case 2: sql += " ORDER BY COALESCE(t.duration_ms,0) DESC, t.filename COLLATE NOCASE ASC"; break;
            case 3: sql += " ORDER BY COALESCE(t.bpm,0) DESC, t.filename COLLATE NOCASE ASC"; break;
            default: sql += " ORDER BY t.filename COLLATE NOCASE ASC"; break;
        }
        sql += " LIMIT ?";

        SqliteStatement statement;
        if (! prepare (db.handle, sql, statement, localError))
        {
            if (error != nullptr)
                *error = localError;
            return result;
        }

        int index = 1;
        if (query.search.isNotEmpty())
        {
            const auto pattern = "%" + escapedSearch + "%";
            for (int i = 0; i < 5; ++i)
                bindText (statement.handle, index++, pattern);
        }
        if (query.family.isNotEmpty())
            bindText (statement.handle, index++, query.family);
        if (query.soundType.isNotEmpty())
            bindText (statement.handle, index++, query.soundType);
        if (query.collectionId > 0)
            bindInt (statement.handle, index++, query.collectionId);
        bindInt (statement.handle, index++, juce::jlimit (1, 1000, query.limit));

        const auto& api = sqliteApi();
        int stepResult = api.step (statement.handle);
        while (stepResult == kSqliteRow)
        {
            VaultTrack track;
            track.id = columnInt64 (statement.handle, 0);
            track.path = columnText (statement.handle, 1);
            track.filename = columnText (statement.handle, 2);
            track.folder = columnText (statement.handle, 3);
            track.format = columnText (statement.handle, 4);
            track.family = columnText (statement.handle, 5);
            track.soundType = columnText (statement.handle, 6);
            track.bpm = columnDouble (statement.handle, 7);
            track.keyRoot = columnText (statement.handle, 8);
            track.keyMode = columnText (statement.handle, 9);
            track.durationMs = columnDouble (statement.handle, 10);
            track.sampleRate = columnInt (statement.handle, 11);
            track.channels = columnInt (statement.handle, 12);
            track.favorite = columnInt (statement.handle, 13) != 0;
            track.analyzed = columnInt (statement.handle, 17) != 0;

            juce::StringArray tags;
            appendJsonTags (columnText (statement.handle, 14), tags);
            appendJsonTags (columnText (statement.handle, 15), tags);
            track.tags = tags.joinIntoString (", ");
            track.notes = columnText (statement.handle, 16);
            result.push_back (std::move (track));

            stepResult = api.step (statement.handle);
        }

        if (error != nullptr)
            *error = localError;
        return result;
    }

    std::vector<VaultCollection> VaultLibrary::queryCollections()
    {
        std::vector<VaultCollection> result;
        if (! status.available)
            refreshStatus();
        if (! status.available)
            return result;

        SqliteDb db;
        juce::String error;
        if (! openDatabase (status.database, db, error))
            return result;

        SqliteStatement statement;
        if (! prepare (db.handle, "SELECT id,name,COALESCE(color,'') FROM collections ORDER BY name COLLATE NOCASE",
                       statement, error))
            return result;

        const auto& api = sqliteApi();
        int stepResult = api.step (statement.handle);
        while (stepResult == kSqliteRow)
        {
            VaultCollection collection;
            collection.id = columnInt (statement.handle, 0);
            collection.name = columnText (statement.handle, 1);
            collection.colour = columnText (statement.handle, 2);
            result.push_back (std::move (collection));
            stepResult = api.step (statement.handle);
        }
        return result;
    }

    bool VaultLibrary::queryTrackDetails (int64_t trackId, VaultTrack& result)
    {
        if (! status.available)
            refreshStatus();
        if (! status.available)
            return false;

        SqliteDb db;
        juce::String error;
        if (! openDatabase (status.database, db, error))
            return false;

        SqliteStatement statement;
        if (! prepare (db.handle,
                       "SELECT t.id,t.path,t.filename,t.folder_path,t.format,"
                       "COALESCE(NULLIF(t.user_family,''),t.family,''),"
                       "COALESCE(NULLIF(t.user_sound_type,''),t.sound_type,''),"
                       "COALESCE(t.bpm,0),COALESCE(t.key_root,''),COALESCE(t.key_mode,''),"
                       "COALESCE(t.duration_ms,0),COALESCE(t.sample_rate,0),COALESCE(t.channels,0),"
                       "COALESCE(t.favorite,0),COALESCE(t.user_tags_json,''),"
                       "COALESCE(t.auto_tags_json,''),COALESCE(t.notes,''),"
                       "CASE WHEN t.analysis_state='complete' THEN 1 ELSE 0 END,"
                       "COALESCE(t.peaks_json,'') FROM tracks t WHERE t.id=?",
                       statement, error))
            return false;
        bindInt64 (statement.handle, 1, trackId);

        if (sqliteApi().step (statement.handle) != kSqliteRow)
            return false;

        result.id = columnInt64 (statement.handle, 0);
        result.path = columnText (statement.handle, 1);
        result.filename = columnText (statement.handle, 2);
        result.folder = columnText (statement.handle, 3);
        result.format = columnText (statement.handle, 4);
        result.family = columnText (statement.handle, 5);
        result.soundType = columnText (statement.handle, 6);
        result.bpm = columnDouble (statement.handle, 7);
        result.keyRoot = columnText (statement.handle, 8);
        result.keyMode = columnText (statement.handle, 9);
        result.durationMs = columnDouble (statement.handle, 10);
        result.sampleRate = columnInt (statement.handle, 11);
        result.channels = columnInt (statement.handle, 12);
        result.favorite = columnInt (statement.handle, 13) != 0;
        result.analyzed = columnInt (statement.handle, 17) != 0;

        juce::StringArray tags;
        appendJsonTags (columnText (statement.handle, 14), tags);
        appendJsonTags (columnText (statement.handle, 15), tags);
        result.tags = tags.joinIntoString (", ");
        result.notes = columnText (statement.handle, 16);
        parseWaveform (columnText (statement.handle, 18), result);
        return true;
    }
} // namespace sliceforge
