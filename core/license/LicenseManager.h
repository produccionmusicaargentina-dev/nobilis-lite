#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>

/** Texto con acentos (los literales del código están en UTF-8). */
#define NOB_TEXT(x) juce::String (juce::CharPointer_UTF8 (x))

namespace nobilis
{

/** Login y licencia del plugin.

    Flujo (igual que activar Spotify en una TV):
      1. "Iniciar sesión" → el plugin pide un código a la web y abre el navegador en /activar/?c=CODIGO.
      2. La persona inicia sesión en la web y toca "Activar".
      3. El plugin (que pregunta cada 3 s) recibe una licencia firmada con Ed25519 y la guarda en disco.
      4. Desde ahí funciona sin internet. Cada 3 días intenta renovarla en segundo plano;
         si no hay conexión, sigue andando hasta que vence (30 días).

    Una sola instancia compartida por todas las copias del plugin abiertas en el DAW
    (juce::SharedResourcePointer<LicenseManager>). Toda la red corre en un hilo propio,
    nunca en el hilo de audio ni en el de la interfaz.
*/
class LicenseManager : private juce::Thread,
                       public juce::ChangeBroadcaster
{
public:
    enum class State { checking, loggedOut, waitingBrowser, active, error };

    LicenseManager();   // usa kLicenseServer / kLicensePublicKeyHex (LicenseKeys.h)
    LicenseManager (const juce::String& serverUrl, const juce::String& publicKeyHex,
                    const juce::File& licenseFile, bool openBrowser);   // tests
    ~LicenseManager() override;

    // --- para el hilo de audio (sin bloqueos) ---
    bool isUnlocked() const noexcept { return unlocked.load (std::memory_order_relaxed); }
    // Full features: paid, NFR, admin, or a TRIAL that has not ended yet (checked against the clock,
    // so a 7-day trial falls back to the Free EQ on time even offline).
    bool isFullTier() const noexcept
    {
        if (! unlocked.load (std::memory_order_relaxed) || ! fullTier.load (std::memory_order_relaxed)) return false;
        const auto until = fullUntil.load (std::memory_order_relaxed);
        return until <= 0 || juce::Time::currentTimeMillis() / 1000 < until;
    }
    bool isTrial() const noexcept { return fullUntil.load (std::memory_order_relaxed) > 0; }
    /** Days of trial left (0 = ended today or no trial). */
    int trialDaysLeft() const noexcept
    {
        const auto until = fullUntil.load (std::memory_order_relaxed);
        if (until <= 0) return 0;
        const auto left = until - juce::Time::currentTimeMillis() / 1000;
        return left <= 0 ? 0 : (int) ((left + 86399) / 86400);
    }

    // --- para la interfaz ---
    State getState() const;
    juce::String getEmail() const;
    juce::String getTier() const;        // "free" | "full"
    juce::String getKind() const;        // "admin" | "perpetual" | "monthly" | "yearly" | "nfr" | "trial" | "free" (may be empty)

    juce::String getUserCode() const;    // código que se ve en el navegador ("ABCD-EFGH")
    juce::String getLoginUrl() const;
    juce::String getMessage() const;     // texto de error / estado para mostrar

    void beginLogin();                   // abre el navegador y espera la confirmación
    void cancelLogin();
    void reopenBrowser() const;
    void logout();                       // borra la licencia de este equipo

    static juce::String getMachineId();  // hash del identificador del equipo
    static juce::File getLicenseFile();

    /** Verifica una licencia (firma, producto, equipo y vencimiento). Público para los tests. */
    struct Parsed { bool valid = false; juce::String email, tier, kind, reason; juce::int64 refreshAt = 0, expiresAt = 0, fullUntil = 0; };
    static Parsed verifyToken (const juce::String& token, const juce::String& publicKeyHex,
                               const juce::String& machineId, const juce::String& product,
                               juce::int64 nowSeconds);

private:
    void run() override;
    void doLogin();
    void doRefresh();
    void loadFromDisk();
    bool acceptToken (const juce::String& token);
    void setState (State s, const juce::String& msg = {});
    juce::var post (const juce::String& path, const juce::var& body, int& status);

    mutable juce::CriticalSection lock;
    State state = State::checking;
    juce::String email, tier, kind, userCode, loginUrl, message, token, server, pubKey;
    juce::File file;
    bool openBrowserOnLogin = true;
    std::atomic<bool> unlocked { false }, fullTier { false };
    std::atomic<juce::int64> fullUntil { 0 };          // trial: full features until this time (0 = no limit)
    std::atomic<bool> wantLogin { false }, wantRefresh { false }, cancelFlag { false };
    juce::int64 refreshAt = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseManager)
};

} // namespace nobilis
