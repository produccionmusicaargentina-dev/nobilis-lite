#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
// Factory + user presets (same as GRAVIS). User presets are small XML
// files (*.pulsus, *.adeps, ...) stored in:
//   Windows: %APPDATA%\Nobilis\<PLUGIN>\Presets
//   macOS:   ~/Library/Audio/Presets/Nobilis/<PLUGIN>
// Subfolders are scanned too, so a preset pack is simply a folder dropped
// in there (its folder name becomes the category).
// All methods must be called from the message thread, except
// syncAfterStateRestore() which is lock-protected.
//==============================================================================
class PresetManager
{
public:
    struct Preset
    {
        juce::String name, category;     // category = source (vocals, drums...) for factory presets
        juce::StringArray tags;          // TONAL, DYNAMIC, SURGICAL, ...
        juce::String author { "Nobilis" };
        bool isFactory = true;
        juce::File file;
        std::map<juce::String, float> values; // real (denormalised) values
        juce::ValueTree extra;                // non-parameter data ("EXTRA"), empty in v1.0
        juce::String key() const { return (isFactory ? "F:" : "U:") + name; }
    };

    // Non-parameter sound data, provided by the processor
    std::function<juce::ValueTree()> getExtra;
    std::function<void (const juce::ValueTree&)> applyExtra;
    void markExtraModified() { extraModified = true; }

    // Library: favourites and the user's own ratings (0..5), kept in library.xml
    bool isFavourite (int index) const;
    void setFavourite (int index, bool);
    int  getRating (int index) const;
    void setRating (int index, int stars);
    juce::StringArray getAllTags() const;

    static constexpr const char* nameKey = "presetName";
    static juce::String getExtension();

    explicit PresetManager (juce::AudioProcessorValueTreeState&);

    void rescan();
    const std::vector<Preset>& getPresets() const noexcept { return presets; }

    int  getCurrentIndex() const;
    juce::String getCurrentName() const;
    bool isModified() const;

    void load (int index);
    void loadNext();
    void loadPrevious();

    // Saves the current settings under `name` (overwrites a user preset with the same name).
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    bool userPresetExists (const juce::String& name) const;

    // Loads any preset file (e.g. from a file chooser) and copies it into the user folder.
    bool importPresetFile (const juce::File&);

    juce::File getUserDirectory() const;
    juce::File getUserPresetFile (const juce::String& name) const { return getUserDirectory().getChildFile (sanitise (name) + getExtension()); }

    // Called after the host restores a session.
    void syncAfterStateRestore();

    using Snapshot = std::map<juce::String, float>;
    Snapshot captureCurrent() const;
    void applyValues (const Snapshot&);
    bool sameValues (const Snapshot& a, const Snapshot& b) const;

private:
    void setCurrent (const juce::String& name, const std::map<juce::String, float>& snapshot);
    static juce::String sanitise (const juce::String&);
    bool readPresetFile (const juce::File&, Preset&) const;

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> presets;

    juce::CriticalSection lock;
    std::map<juce::String, float> loadedSnapshot;
    bool extraModified = false;

    void loadLibrary();
    void saveLibrary() const;
    std::map<juce::String, std::pair<bool, int>> library;   // key -> (favourite, rating)
};

//==============================================================================
// Undo / redo of parameter changes + A/B comparison. Message thread only;
// poll() is called by the editor timer and groups a drag into one step.
class ParamHistory
{
public:
    explicit ParamHistory (PresetManager& m) : pm (m) { stable = pm.captureCurrent(); }

    void poll();

    bool canUndo() const noexcept { return ! undoStack.empty() || pending; }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    void undo();
    void redo();

    bool isOnB() const noexcept { return onB; }
    void toggleAB();
    void copyToOther();   // copies the active slot into the other one

private:
    void commitPending();
    void applyAndSettle (const PresetManager::Snapshot&);

    PresetManager& pm;
    PresetManager::Snapshot stable, pendingBase;
    bool pending = false;
    juce::uint32 lastChangeMs = 0;
    std::vector<PresetManager::Snapshot> undoStack, redoStack;

    bool onB = false;
    PresetManager::Snapshot slotA, slotB;
    static constexpr size_t maxSteps = 100;
};
