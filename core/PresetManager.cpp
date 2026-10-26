#include "PresetManager.h"
#include "Product.h"
#include "Diagnostics.h"

using namespace juce;

String PresetManager::getExtension() { return nob::product().presetExt; }

PresetManager::PresetManager (AudioProcessorValueTreeState& s) : apvts (s)
{
    loadLibrary();
    rescan();
    if (! apvts.state.hasProperty (nameKey))
        apvts.state.setProperty (nameKey, "Init", nullptr);
    loadedSnapshot = captureCurrent();
}

File PresetManager::getUserDirectory() const
{
   #if JUCE_MAC
    auto dir = File::getSpecialLocation (File::userHomeDirectory)
                   .getChildFile ("Library/Audio/Presets/Nobilis").getChildFile (nob::product().name);
   #else
    auto dir = File::getSpecialLocation (File::userApplicationDataDirectory)
                   .getChildFile ("Nobilis").getChildFile (nob::product().name).getChildFile ("Presets");
   #endif
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

bool PresetManager::readPresetFile (const File& file, Preset& p) const
{
    auto xml = parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("NobilisPreset"))
        return false;

    p.name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
    p.author = xml->getStringAttribute ("author", "User");
    p.tags = StringArray::fromTokens (xml->getStringAttribute ("tags"), ",", "");
    p.tags.trim();
    p.tags.removeEmptyStrings();
    p.isFactory = false;
    p.file = file;
    for (auto* e : xml->getChildWithTagNameIterator ("Param"))
        p.values[e->getStringAttribute ("id")] = (float) e->getDoubleAttribute ("value");
    if (auto* ex = xml->getChildByName ("EXTRA"))
        p.extra = ValueTree::fromXml (*ex);
    return true;
}

void PresetManager::rescan()
{
    presets.clear();

    for (const auto& f : nob::factoryPresets())
    {
        Preset p;
        p.name = String::fromUTF8 (f.name);
        p.category = f.category;
        p.tags = StringArray::fromTokens (f.tags, ",", "");
        p.isFactory = true;
        for (const auto& [id, v] : f.values)
            p.values[id] = v;
        p.extra = ValueTree ("EXTRA");
        presets.push_back (std::move (p));
    }

    // user presets; a subfolder (e.g. an installed pack) becomes its category
    const auto root = getUserDirectory();
    auto files = root.findChildFiles (File::findFiles, true, String ("*") + getExtension());
    files.sort();
    for (const auto& file : files)
    {
        Preset p;
        if (! readPresetFile (file, p))
            continue;
        p.category = file.getParentDirectory() == root ? String ("User")
                                                         : file.getParentDirectory().getFileName();
        presets.push_back (std::move (p));
    }
}

bool PresetManager::sameValues (const Snapshot& a, const Snapshot& b) const
{
    if (a.size() != b.size()) return false;
    for (const auto& [id, v] : a)
    {
        const auto it = b.find (id);
        if (it == b.end()) return false;
        auto* rp = apvts.getParameter (id);
        const float x = rp ? rp->convertTo0to1 (v) : v;
        const float y = rp ? rp->convertTo0to1 (it->second) : it->second;
        if (std::abs (x - y) > 1.0e-4f) return false;
    }
    return true;
}

std::map<String, float> PresetManager::captureCurrent() const
{
    std::map<String, float> m;
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            m[rp->getParameterID()] = rp->convertFrom0to1 (rp->getValue());
    return m;
}

void PresetManager::applyValues (const std::map<String, float>& values)
{
    for (auto* p : apvts.processor.getParameters())
    {
        auto* rp = dynamic_cast<RangedAudioParameter*> (p);
        if (rp == nullptr) continue;

        const auto it = values.find (rp->getParameterID());
        const float norm = it != values.end() ? rp->convertTo0to1 (it->second) : rp->getDefaultValue();

        rp->beginChangeGesture();
        rp->setValueNotifyingHost (norm);
        rp->endChangeGesture();
    }
}

void PresetManager::setCurrent (const String& name, const std::map<String, float>& snapshot)
{
    apvts.state.setProperty (nameKey, name, nullptr);
    const ScopedLock sl (lock);
    loadedSnapshot = snapshot;
}

int PresetManager::getCurrentIndex() const
{
    const auto name = getCurrentName();
    // prefer user preset with that name, then factory
    for (int i = (int) presets.size(); --i >= 0;)
        if (presets[(size_t) i].name == name)
            return i;
    return -1;
}

String PresetManager::getCurrentName() const
{
    return apvts.state.getProperty (nameKey, "Init").toString();
}

bool PresetManager::isModified() const
{
    const auto now = captureCurrent();
    const ScopedLock sl (lock);
    if (extraModified) return true;
    for (const auto& [id, v] : now)
    {
        const auto it = loadedSnapshot.find (id);
        if (it == loadedSnapshot.end()) return true;
        auto* rp = apvts.getParameter (id);
        const float a = rp ? rp->convertTo0to1 (v) : v;
        const float b = rp ? rp->convertTo0to1 (it->second) : it->second;
        if (std::abs (a - b) > 1.0e-3f)
            return true;
    }
    return false;
}

void PresetManager::load (int index)
{
    if (! isPositiveAndBelow (index, (int) presets.size()))
        return;
    const auto& p = presets[(size_t) index];
    nob::diag::step ("Preset load: " + p.name);
    applyValues (p.values);
    if (applyExtra) applyExtra (p.extra.isValid() ? p.extra : ValueTree ("EXTRA"));
    extraModified = false;
    setCurrent (p.name, captureCurrent());
}

void PresetManager::loadNext()
{
    if (presets.empty()) return;
    const int i = getCurrentIndex();
    load (i < 0 ? 0 : (i + 1) % (int) presets.size());
}

void PresetManager::loadPrevious()
{
    if (presets.empty()) return;
    const int i = getCurrentIndex();
    const int n = (int) presets.size();
    load (i < 0 ? n - 1 : (i - 1 + n) % n);
}

String PresetManager::sanitise (const String& name)
{
    auto s = name.trim().retainCharacters (
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_()");
    return s.isEmpty() ? String ("Preset") : s.substring (0, 48);
}

bool PresetManager::userPresetExists (const String& name) const
{
    return getUserDirectory().getChildFile (sanitise (name) + getExtension()).existsAsFile();
}

bool PresetManager::saveUserPreset (const String& rawName)
{
    const auto name = rawName.trim().substring (0, 48);
    if (name.isEmpty())
        return false;

    const auto values = captureCurrent();
    XmlElement root ("NobilisPreset");
    root.setAttribute ("plugin", nob::product().name);
    root.setAttribute ("name", name);
    root.setAttribute ("version", 1);
    root.setAttribute ("author", "User");
    // keep the tags of the preset this one started from
    if (const int cur = getCurrentIndex(); cur >= 0)
        root.setAttribute ("tags", presets[(size_t) cur].tags.joinIntoString (","));
    for (const auto& [id, v] : values)
    {
        auto* e = root.createNewChildElement ("Param");
        e->setAttribute ("id", id);
        e->setAttribute ("value", (double) v);
    }
    if (getExtra)
        if (auto ex = getExtra().createXml())
            root.addChildElement (ex.release());

    const auto file = getUserDirectory().getChildFile (sanitise (name) + getExtension());
    nob::diag::step ("Preset save: " + file.getFullPathName());
    if (! root.writeTo (file))
        return false;

    rescan();
    extraModified = false;
    setCurrent (name, values);
    return true;
}

bool PresetManager::importPresetFile (const File& file)
{
    Preset p;
    if (! readPresetFile (file, p))
        return false;

    const auto dest = getUserDirectory().getChildFile (file.getFileName());
    if (dest != file && ! file.copyFileTo (dest))
        return false;

    rescan();
    for (int i = 0; i < (int) presets.size(); ++i)
        if (presets[(size_t) i].file == dest) { load (i); break; }
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! isPositiveAndBelow (index, (int) presets.size()) || presets[(size_t) index].isFactory)
        return false;

    const bool ok = presets[(size_t) index].file.deleteFile();
    rescan();
    if (ok && getCurrentIndex() < 0)
        setCurrent (getCurrentName(), captureCurrent()); // keep settings, name stays until changed
    return ok;
}

void PresetManager::syncAfterStateRestore()
{
    const auto snap = captureCurrent();
    const ScopedLock sl (lock);
    loadedSnapshot = snap;
    extraModified = false;
}

//==============================================================================
static File libraryFile (const File& userDir) { return userDir.getChildFile ("library.xml"); }

void PresetManager::loadLibrary()
{
    library.clear();
    if (auto xml = parseXML (libraryFile (getUserDirectory())))
        for (auto* e : xml->getChildWithTagNameIterator ("Item"))
            library[e->getStringAttribute ("key")] = { e->getBoolAttribute ("fav"), e->getIntAttribute ("rating") };
}

void PresetManager::saveLibrary() const
{
    XmlElement root ("NobilisLibrary");
    for (const auto& [key, v] : library)
    {
        if (! v.first && v.second == 0) continue;
        auto* e = root.createNewChildElement ("Item");
        e->setAttribute ("key", key);
        e->setAttribute ("fav", v.first);
        e->setAttribute ("rating", v.second);
    }
    root.writeTo (libraryFile (getUserDirectory()));
}

bool PresetManager::isFavourite (int index) const
{
    if (! isPositiveAndBelow (index, (int) presets.size())) return false;
    const auto it = library.find (presets[(size_t) index].key());
    return it != library.end() && it->second.first;
}

void PresetManager::setFavourite (int index, bool fav)
{
    if (! isPositiveAndBelow (index, (int) presets.size())) return;
    library[presets[(size_t) index].key()].first = fav;
    saveLibrary();
}

int PresetManager::getRating (int index) const
{
    if (! isPositiveAndBelow (index, (int) presets.size())) return 0;
    const auto it = library.find (presets[(size_t) index].key());
    return it != library.end() ? it->second.second : 0;
}

void PresetManager::setRating (int index, int stars)
{
    if (! isPositiveAndBelow (index, (int) presets.size())) return;
    library[presets[(size_t) index].key()].second = jlimit (0, 5, stars);
    saveLibrary();
}

StringArray PresetManager::getAllTags() const
{
    // fixed, meaningful order first; anything else (user tags) after
    StringArray tags;
    for (const auto& p : presets)
        for (const auto& t : p.tags)
            tags.addIfNotAlreadyThere (t.toUpperCase());
    return tags;
}

//==============================================================================
void ParamHistory::poll()
{
    const auto now = pm.captureCurrent();
    const auto ms = Time::getMillisecondCounter();

    const bool mouseDown = ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown();

    if (! pm.sameValues (now, stable))
    {
        // Only changes made by the user (recent click/drag in the UI) become undo
        // steps; host automation just moves the reference point.
        const auto lastDown = Desktop::getInstance().getMainMouseSource().getLastMouseDownTime();
        const bool userDriven = mouseDown || (Time::getCurrentTime() - lastDown).inMilliseconds() < 1500;

        if (userDriven && ! pending)
        {
            pendingBase = stable;
            pending = true;
        }
        stable = now;
        lastChangeMs = ms;
    }

    // close the step once the user has let go and nothing moved for a moment
    if (pending && ! mouseDown && ms - lastChangeMs > 350)
        commitPending();
}

void ParamHistory::commitPending()
{
    if (! pending) return;
    pending = false;
    if (pm.sameValues (pendingBase, stable)) return;
    undoStack.push_back (pendingBase);
    if (undoStack.size() > maxSteps) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

void ParamHistory::applyAndSettle (const PresetManager::Snapshot& s)
{
    pm.applyValues (s);
    stable = pm.captureCurrent();
    pending = false;
}

void ParamHistory::undo()
{
    commitPending();
    if (undoStack.empty()) return;
    redoStack.push_back (pm.captureCurrent());
    const auto target = undoStack.back();
    undoStack.pop_back();
    applyAndSettle (target);
}

void ParamHistory::redo()
{
    commitPending();
    if (redoStack.empty()) return;
    undoStack.push_back (pm.captureCurrent());
    const auto target = redoStack.back();
    redoStack.pop_back();
    applyAndSettle (target);
}

void ParamHistory::toggleAB()
{
    commitPending();
    const auto current = pm.captureCurrent();
    (onB ? slotB : slotA) = current;
    onB = ! onB;
    auto& target = onB ? slotB : slotA;
    if (target.empty())
        target = current;               // first switch: B starts as a copy of A

    undoStack.push_back (current);
    redoStack.clear();
    applyAndSettle (target);
}

void ParamHistory::copyToOther()
{
    (onB ? slotA : slotB) = pm.captureCurrent();
}
