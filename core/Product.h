#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

//==============================================================================
// What makes each plugin of the pack different, as far as the shared core is
// concerned. Every plugin defines nob::product() and nob::factoryPresets() once
// (in its own Product.cpp); the core never names a plugin directly.
//==============================================================================
namespace nob
{

struct ProductInfo
{
    const char* name;           // "PULSUS"  (logo, folders, dialogs)
    const char* tagline;        // "TEMPO PUMP  /  LITE"  (under the logo)
    const char* about;          // one sentence for the ABOUT box (English, goes through TRANS)
    const char* presetExt;      // ".pulsus"
    // extra TRANS() mappings for this plugin, per language code (may return nullptr)
    const char* (*translations) (const juce::String& code);
};

const ProductInfo& product();

// Factory bank: only the values that differ from the parameter defaults.
struct FactoryPreset
{
    const char* category;       // "NN Name": the number only sets the order
    const char* name;
    const char* tags;
    std::vector<std::pair<const char*, float>> values;
};

const std::vector<FactoryPreset>& factoryPresets();

// The whole pack shares one licence (one sign-in unlocks the four plugins)
inline constexpr const char* licenceProduct = "lite";
inline constexpr const char* packName = "NOBILIS LITE";
inline constexpr const char* packFolder = "Lite";      // Nobilis/Lite (licence, settings, update cache)

// "PULSUS" -> "Pulsus"
inline juce::String productFolder() { auto n = juce::String (product().name); return n.substring (0, 1) + n.substring (1).toLowerCase(); }

} // namespace nob
