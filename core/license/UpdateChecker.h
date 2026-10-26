#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "LicenseKeys.h"
#include "LicenseManager.h"
#include "Product.h"

namespace nobilis
{

/** Aviso de nueva versión.

    Una vez por día (como mucho) pregunta a la web cuál es la última versión publicada
    (GET /api/plugin/version) y la guarda junto a la licencia. Si es más nueva que la
    instalada, la interfaz muestra un aviso con el link de descarga.
    Corre en su propio hilo; sin internet simplemente no avisa.
    Una sola instancia para todas las copias abiertas (juce::SharedResourcePointer).
*/
class UpdateChecker : private juce::Thread,
                      public juce::ChangeBroadcaster
{
public:
    UpdateChecker() : juce::Thread ("Nobilis update check")
    {
        loadCache();
        startThread (juce::Thread::Priority::background);
    }

    ~UpdateChecker() override
    {
        signalThreadShouldExit();
        stopThread (10000);
    }

    bool hasUpdate() const
    {
        const juce::ScopedLock sl (lock);
        return isNewer (latest, JucePlugin_VersionString);
    }

    juce::String getLatestVersion() const { const juce::ScopedLock sl (lock); return latest; }

    juce::String getDownloadUrl() const
    {
        const juce::ScopedLock sl (lock);
        return url.startsWith ("https://") ? url : juce::String (kLicenseServer) + "/cuenta/#descargas";
    }

    /** true si la versión a ("1.0.10") es mayor que b ("1.0.9"). */
    static bool isNewer (const juce::String& a, const juce::String& b)
    {
        juce::StringArray x, y;
        x.addTokens (a.trim(), ".", {});
        y.addTokens (b.trim(), ".", {});
        if (a.trim().isEmpty())
            return false;
        for (int i = 0; i < juce::jmax (x.size(), y.size()); ++i)
        {
            const int p = x[i].getIntValue(), q = y[i].getIntValue();
            if (p != q)
                return p > q;
        }
        return false;
    }

private:
    static juce::File cacheFile() { return LicenseManager::getLicenseFile().getSiblingFile ("update.json"); }

    void loadCache()
    {
        const auto v = juce::JSON::parse (cacheFile());
        const juce::ScopedLock sl (lock);
        latest    = v["version"].toString();
        url       = v["url"].toString();
        lastCheck = (juce::int64) v["checked"];
    }

    void run() override
    {
        // dejar que el DAW termine de cargar antes de usar la red
        for (int i = 0; i < 60 && ! threadShouldExit(); ++i)
            sleep (100);

        const auto nowSec = juce::Time::currentTimeMillis() / 1000;
        {
            const juce::ScopedLock sl (lock);
            if (nowSec - lastCheck < 20 * 3600)
                return;
        }

        int status = 0;
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (8000)
                        .withStatusCode (&status)
                        .withExtraHeaders ("Accept: application/json");
        std::unique_ptr<juce::InputStream> in (juce::URL (juce::String (kLicenseServer) + "/api/plugin/version")
                                                   .createInputStream (opts));
        if (in == nullptr || status != 200 || threadShouldExit())
            return;

        const auto v = juce::JSON::parse (in->readEntireStreamAsString());
        const auto ver = v[nob::licenceProduct]["version"].toString().trim();
        const auto link = v[nob::licenceProduct]["url"].toString().trim();
        if (ver.isEmpty())
            return;

        {
            const juce::ScopedLock sl (lock);
            latest = ver;
            url = link;
            lastCheck = nowSec;
        }

        auto* obj = new juce::DynamicObject();
        obj->setProperty ("version", ver);
        obj->setProperty ("url", link);
        obj->setProperty ("checked", nowSec);
        cacheFile().getParentDirectory().createDirectory();
        cacheFile().replaceWithText (juce::JSON::toString (juce::var (obj)));

        sendChangeMessage();
    }

    mutable juce::CriticalSection lock;
    juce::String latest, url;
    juce::int64 lastCheck = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UpdateChecker)
};

} // namespace nobilis
