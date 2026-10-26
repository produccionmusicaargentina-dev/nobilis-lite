#include "LicenseManager.h"
#include "LicenseKeys.h"
#include "Product.h"

extern "C" {
#include "ThirdParty/monocypher/monocypher.h"
#include "ThirdParty/monocypher/monocypher-ed25519.h"
}

#ifndef NOB_REQUIRE_LOGIN
 #define NOB_REQUIRE_LOGIN 1
#endif

namespace nobilis
{
namespace
{
    const char* const kProduct = nob::licenceProduct;

    juce::int64 nowSeconds() { return juce::Time::currentTimeMillis() / 1000; }

    bool decodeBase64Url (juce::String s, juce::MemoryBlock& out)
    {
        s = s.replaceCharacter ('-', '+').replaceCharacter ('_', '/');
        while (s.length() % 4 != 0)
            s << "=";
        juce::MemoryOutputStream mo (out, false);
        return juce::Base64::convertFromBase64 (mo, s);
    }

    bool decodeHex (const juce::String& hex, uint8_t* out, size_t n)
    {
        if ((size_t) hex.length() != n * 2 || ! hex.containsOnly ("0123456789abcdefABCDEF"))
            return false;
        for (size_t i = 0; i < n; ++i)
            out[i] = (uint8_t) hex.substring ((int) i * 2, (int) i * 2 + 2).getHexValue32();
        return true;
    }

    juce::String formatCode (const juce::String& c)
    {
        return c.length() == 8 ? c.substring (0, 4) + "-" + c.substring (4) : c;
    }
}

//==============================================================================
LicenseManager::LicenseManager()
    : LicenseManager (kLicenseServer, kLicensePublicKeyHex, getLicenseFile(), true) {}

LicenseManager::LicenseManager (const juce::String& serverUrl, const juce::String& publicKeyHex,
                                const juce::File& licenseFile, bool openBrowser)
    : juce::Thread ("Nobilis license"), server (serverUrl.trimCharactersAtEnd ("/")),
      pubKey (publicKeyHex.trim()), file (licenseFile), openBrowserOnLogin (openBrowser)
{
   #if ! NOB_REQUIRE_LOGIN
    // Compilación de desarrollo: sin login.
    state = State::active; tier = "full"; kind = "admin"; email = "modo desarrollo";
    unlocked = true; fullTier = true;
   #else
    startThread (juce::Thread::Priority::low);
   #endif
}

LicenseManager::~LicenseManager()
{
    cancelFlag = true;
    signalThreadShouldExit();
    notify();
    stopThread (12000);
}

//==============================================================================
LicenseManager::State LicenseManager::getState() const { const juce::ScopedLock sl (lock); return state; }
juce::String LicenseManager::getEmail() const          { const juce::ScopedLock sl (lock); return email; }
juce::String LicenseManager::getTier() const           { const juce::ScopedLock sl (lock); return tier; }
juce::String LicenseManager::getKind() const           { const juce::ScopedLock sl (lock); return kind; }
juce::String LicenseManager::getUserCode() const       { const juce::ScopedLock sl (lock); return userCode; }
juce::String LicenseManager::getLoginUrl() const       { const juce::ScopedLock sl (lock); return loginUrl; }
juce::String LicenseManager::getMessage() const        { const juce::ScopedLock sl (lock); return message; }

void LicenseManager::setState (State s, const juce::String& msg)
{
    {
        const juce::ScopedLock sl (lock);
        state = s;
        message = msg;
    }
    sendChangeMessage();
}

void LicenseManager::beginLogin()
{
   #if NOB_REQUIRE_LOGIN
    cancelFlag = false;
    wantLogin = true;
    setState (State::waitingBrowser, NOB_TEXT ("Conectando…"));
    notify();
   #endif
}

void LicenseManager::cancelLogin()
{
    cancelFlag = true;
    notify();
}

void LicenseManager::reopenBrowser() const
{
    const auto u = getLoginUrl();
    if (u.isNotEmpty())
        juce::URL (u).launchInDefaultBrowser();
}

void LicenseManager::logout()
{
   #if NOB_REQUIRE_LOGIN
    cancelFlag = true;
    file.deleteFile();
    unlocked = false;
    {
        const juce::ScopedLock sl (lock);
        token.clear(); email.clear(); tier.clear(); kind.clear(); refreshAt = 0;
    }
    fullUntil = 0;
    setState (State::loggedOut);
   #endif
}

//==============================================================================
juce::File LicenseManager::getLicenseFile()
{
   #if JUCE_MAC
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/Nobilis").getChildFile (nob::packFolder);
   #else
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Nobilis").getChildFile (nob::packFolder);
   #endif
    return dir.getChildFile ("license.key");
}

juce::String LicenseManager::getMachineId()
{
    auto id = juce::SystemStats::getUniqueDeviceID();
    if (id.isEmpty())
        id = juce::SystemStats::getComputerName() + "|" + juce::SystemStats::getLogonName();
    const auto utf8 = ("nobilis|" + id).toStdString();
    uint8_t hash[32];
    crypto_blake2b (hash, sizeof (hash), reinterpret_cast<const uint8_t*> (utf8.data()), utf8.size());
    return juce::String::toHexString (hash, (int) sizeof (hash), 0);
}

LicenseManager::Parsed LicenseManager::verifyToken (const juce::String& tok, const juce::String& publicKeyHex,
                                                    const juce::String& machineId, const juce::String& product,
                                                    juce::int64 now)
{
    Parsed p;
    uint8_t pk[32];
    if (! decodeHex (publicKeyHex, pk, sizeof (pk)))
    {
        p.reason = "El plugin no tiene configurada la clave de licencias.";
        return p;
    }

    const int dot = tok.indexOfChar ('.');
    if (dot <= 0 || tok.length() > 8000)
    {
        p.reason = NOB_TEXT ("Licencia inválida.");
        return p;
    }
    const auto body = tok.substring (0, dot);
    juce::MemoryBlock sig, json;
    if (! decodeBase64Url (tok.substring (dot + 1), sig) || sig.getSize() != 64 || ! decodeBase64Url (body, json))
    {
        p.reason = NOB_TEXT ("Licencia inválida.");
        return p;
    }

    const auto bodyUtf8 = body.toStdString();
    if (crypto_ed25519_check (static_cast<const uint8_t*> (sig.getData()), pk,
                              reinterpret_cast<const uint8_t*> (bodyUtf8.data()), bodyUtf8.size()) != 0)
    {
        p.reason = NOB_TEXT ("La firma de la licencia no es válida.");
        return p;
    }

    const auto v = juce::JSON::parse (json.toString());
    if (v["prod"].toString() != product)              { p.reason = "La licencia es de otro producto."; return p; }
    if (v["mid"].toString() != machineId)             { p.reason = "La licencia es de otra computadora."; return p; }

    p.email     = v["email"].toString();
    p.tier      = v["tier"].toString();
    p.kind      = v["kind"].toString();        // admin | perpetual | monthly | yearly | nfr | free (servers before ACIES: empty)
    p.refreshAt = (juce::int64) v["ref"];
    p.expiresAt = (juce::int64) v["exp"];
    p.fullUntil = (juce::int64) v["until"];     // trial end (absent = no limit)
    if (p.expiresAt <= now)                            { p.reason = NOB_TEXT ("La licencia venció. Conectate a internet para renovarla."); return p; }

    p.valid = true;
    return p;
}

bool LicenseManager::acceptToken (const juce::String& tok)
{
    const auto p = verifyToken (tok, pubKey, getMachineId(), kProduct, nowSeconds());
    if (! p.valid)
        return false;

    file.getParentDirectory().createDirectory();
    file.replaceWithText (tok);
    {
        const juce::ScopedLock sl (lock);
        token = tok; email = p.email; tier = p.tier; kind = p.kind; refreshAt = p.refreshAt;
        userCode.clear(); loginUrl.clear();
    }
    fullTier = p.tier == "full";
    fullUntil = p.fullUntil;
    unlocked = true;
    setState (State::active);
    return true;
}

//==============================================================================
juce::var LicenseManager::post (const juce::String& path, const juce::var& body, int& status)
{
    status = 0;
    auto url = juce::URL (server + path).withPOSTData (juce::JSON::toString (body, true));
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders ("Content-Type: application/json")
                    .withConnectionTimeoutMs (10000)
                    .withStatusCode (&status)
                    .withProgressCallback ([this] (int, int) { return ! threadShouldExit(); });

    std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
    if (in == nullptr)
        return {};
    return juce::JSON::parse (in->readEntireStreamAsString());
}

void LicenseManager::loadFromDisk()
{
    const auto saved = file.loadFileAsString().trim();
    if (saved.isEmpty())
    {
        setState (State::loggedOut);
        return;
    }

    const auto p = verifyToken (saved, pubKey, getMachineId(), kProduct, nowSeconds());
    if (p.valid)
    {
        acceptToken (saved);
        if (nowSeconds() >= p.refreshAt)
            wantRefresh = true;
        return;
    }

    // Vencida (o de otra versión): intentar renovarla. Si no hay internet, queda deslogueado.
    {
        const juce::ScopedLock sl (lock);
        token = saved;
    }
    doRefresh();
    if (! unlocked)
        setState (State::loggedOut, p.reason);
}

void LicenseManager::doRefresh()
{
    juce::String current;
    {
        const juce::ScopedLock sl (lock);
        current = token;
    }
    if (current.isEmpty())
        return;

    juce::DynamicObject::Ptr b = new juce::DynamicObject();
    b->setProperty ("token", current);
    int status = 0;
    const auto r = post ("/api/plugin/refresh", juce::var (b.get()), status);
    if (status != 200 || ! r.isObject())
        return;                                   // sin conexión: seguimos con la que hay

    const auto st = r["status"].toString();
    if (st == "ok")
    {
        if (! acceptToken (r["token"].toString()))
            setState (State::error, "No se pudo verificar la licencia renovada.");
    }
    else if (st == "revoked")
    {
        file.deleteFile();
        unlocked = false;
        {
            const juce::ScopedLock sl (lock);
            token.clear(); email.clear(); tier.clear(); kind.clear();
        }
        setState (State::loggedOut, r["error"].toString());
    }
}

void LicenseManager::doLogin()
{
    juce::DynamicObject::Ptr b = new juce::DynamicObject();
    b->setProperty ("product", kProduct);
    b->setProperty ("machine_id", getMachineId());
    b->setProperty ("machine_name", juce::SystemStats::getComputerName());
    b->setProperty ("os", juce::SystemStats::getOperatingSystemName());
   #ifdef JucePlugin_VersionString
    b->setProperty ("version", JucePlugin_VersionString);
   #endif

    int status = 0;
    const auto r = post ("/api/plugin/start", juce::var (b.get()), status);
    if (status != 200 || ! r.isObject() || r["poll_secret"].toString().isEmpty())
    {
        setState (State::error, status == 0 ? NOB_TEXT ("No hay conexión con nobilis. Revisá tu internet y probá de nuevo.")
                                            : r["error"].toString().isNotEmpty() ? r["error"].toString()
                                                                                 : NOB_TEXT ("No se pudo iniciar sesión (") + juce::String (status) + ").");
        return;
    }

    const auto secret = r["poll_secret"].toString();
    const int interval = juce::jlimit (2, 10, (int) r["interval"]);
    {
        const juce::ScopedLock sl (lock);
        userCode = formatCode (r["user_code"].toString());
        loginUrl = r["url"].toString();
    }
    setState (State::waitingBrowser, NOB_TEXT ("Confirmá en el navegador…"));
    if (openBrowserOnLogin)
        juce::MessageManager::callAsync ([u = getLoginUrl()] { juce::URL (u).launchInDefaultBrowser(); });

    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 15.0 * 60.0 * 1000.0;
    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        wait (interval * 1000);
        if (threadShouldExit() || cancelFlag)
        {
            setState (State::loggedOut);
            return;
        }

        juce::DynamicObject::Ptr pb = new juce::DynamicObject();
        pb->setProperty ("poll_secret", secret);
        const auto pr = post ("/api/plugin/poll", juce::var (pb.get()), status);
        if (status != 200 || ! pr.isObject())
            continue;                                         // corte momentáneo: seguir esperando

        const auto st = pr["status"].toString();
        if (st == "pending")
            continue;
        if (st == "ok")
        {
            if (! acceptToken (pr["token"].toString()))
                setState (State::error, NOB_TEXT ("La licencia recibida no es válida. ¿La clave pública del plugin coincide con la del servidor?"));
            return;
        }
        setState (State::error, st == "denied"  ? (pr["error"].toString().isNotEmpty() ? pr["error"].toString() : NOB_TEXT ("Se canceló el inicio de sesión."))
                              : st == "expired" ? NOB_TEXT ("El código venció. Tocá de nuevo \"Iniciar sesión\".")
                                                : NOB_TEXT ("El inicio de sesión no es válido. Probá de nuevo."));
        return;
    }
    setState (State::error, NOB_TEXT ("Pasó mucho tiempo. Tocá de nuevo \"Iniciar sesión\"."));
}

void LicenseManager::run()
{
    loadFromDisk();

    while (! threadShouldExit())
    {
        if (wantLogin.exchange (false))
            doLogin();
        else if (wantRefresh.exchange (false))
            doRefresh();

        if (threadShouldExit())
            break;

        // Revisa cada hora si toca renovar (por si el DAW queda abierto varios días)
        wait (60 * 60 * 1000);
        juce::int64 due = 0;
        {
            const juce::ScopedLock sl (lock);
            due = refreshAt;
        }
        const auto until = fullUntil.load();
        if (unlocked && ((due > 0 && nowSeconds() >= due) || (until > 0 && nowSeconds() >= until && fullTier)))
            wantRefresh = true;
    }
}

} // namespace nobilis
