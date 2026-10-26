#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Translations.h"
#include "Product.h"

//==============================================================================
// Interface language (same system as GRAVIS). English is the source text
// (TRANS("...") everywhere); the other languages are juce::LocalisedStrings
// mappings from Translations.h. The choice is global (all Nobilis Lite instances,
// all sessions) and is stored in Nobilis/Lite.settings next to the presets.
//
// Parameter names (what the DAW shows for automation) and section titles stay
// in English, like on most hardware and software processors.
//==============================================================================
namespace nob::i18n
{

struct Language { const char* code; const char* nativeName; const char* mappings; };

inline const std::vector<Language>& languages()
{
    static const std::vector<Language> l =
    {
        { "en",     "English",                  nullptr },
        { "es",     "Español (España)",         translations::es },
        { "es-419", "Español (Latinoamérica)",  translations::es419 },
        { "fr",     "Français",                 translations::fr },
        { "zh",     "中文（简体）",               translations::zh },
        { "ja",     "日本語",                     translations::ja },
    };
    return l;
}

inline juce::PropertiesFile& settings()
{
    static juce::PropertiesFile file ([]
    {
        juce::PropertiesFile::Options o;
        o.applicationName = nob::packFolder;
        o.folderName = "Nobilis";
        o.filenameSuffix = "settings";
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return o;
    }());
    return file;
}

inline juce::String& currentCode()
{
    static juce::String code { "en" };
    return code;
}

inline void setLanguage (const juce::String& code, bool save = true)
{
    for (const auto& l : languages())
        if (code == l.code)
        {
            currentCode() = l.code;
            if (l.mappings == nullptr)
                juce::LocalisedStrings::setCurrentMappings (nullptr);
            else
            {
                // shared strings of the pack + this plugin's own (each plugin binary has its own JUCE statics)
                auto text = juce::String::fromUTF8 (l.mappings);
                if (auto* extra = nob::product().translations != nullptr ? nob::product().translations (l.code) : nullptr)
                    text << "\n" << juce::String::fromUTF8 (extra);
                juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (text, false));
            }
            if (save)
            {
                settings().setValue ("language", code);
                settings().saveIfNeeded();
            }
            return;
        }
}

inline void applySavedLanguage()
{
    static bool done = false;          // once per process: every instance shares it
    if (done) return;
    done = true;
    setLanguage (settings().getValue ("language", "en"), false);
}

} // namespace nob::i18n
