// Adapted from TONE3000's MIT-licensed JUCE example (github.com/tone-3000/t3k-api), Copyright (c) 2026 TONE3000.

#if defined(_WIN32)
 #include <windows.h>
 #include <wincrypt.h>
 #include <bcrypt.h>
#endif

#include "T3KAuth.h"
#include <juce_cryptography/juce_cryptography.h>
#include <juce_data_structures/juce_data_structures.h>

namespace abyssal::t3k
{

//============================================================================== HTTP
HttpResponse sendSync (const HttpRequest& request)
{
    HttpResponse response;

    juce::String headerBlock;
    for (auto& key : request.headers.getAllKeys())
        headerBlock << key << ": " << request.headers[key] << "\r\n";

    auto url = request.url;
    if (request.method == "POST" || request.method == "PUT")
        url = url.withPOSTData (request.body);

    int status = 0;
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withHttpRequestCmd (request.method)
                       .withExtraHeaders (headerBlock)
                       .withConnectionTimeoutMs (request.timeoutMs)
                       .withNumRedirectsToFollow (5)
                       .withStatusCode (&status);

    if (auto stream = url.createInputStream (options))
    {
        stream->readIntoMemoryBlock (response.body);
        response.status = status;
    }
    else
    {
        response.status = status;
        if (status == 0)
            response.error = "Couldn't reach " + request.url.getDomain() + " - check your internet connection.";
    }
    return response;
}

//============================================================================== OAuth
namespace
{
    juce::String base64Url (const void* data, size_t size)
    {
        return juce::Base64::toBase64 (data, size).replaceCharacter ('+', '-').replaceCharacter ('/', '_').removeCharacters ("=");
    }

    /** Cryptographically secure random bytes from the OS (the PKCE verifier and state are secrets). */
    juce::String secureRandomString (int numBytes)
    {
        juce::MemoryBlock bytes ((size_t) numBytes);
       #if defined(_WIN32)
        if (BCryptGenRandom (nullptr, (PUCHAR) bytes.getData(), (ULONG) bytes.getSize(), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0)
            jassertfalse;
       #else
        std::random_device rd;
        for (size_t i = 0; i < bytes.getSize(); ++i)
            bytes[i] = (char) (rd() & 0xff);
       #endif
        return base64Url (bytes.getData(), bytes.getSize());
    }
}

Pkce Pkce::generate()
{
    Pkce p;
    p.codeVerifier = secureRandomString (32); // 43 chars after base64url
    juce::SHA256 hash (p.codeVerifier.toRawUTF8(), p.codeVerifier.getNumBytesAsUTF8());
    const auto raw = hash.getRawData();
    p.codeChallenge = base64Url (raw.getData(), raw.getSize());
    p.state = secureRandomString (16);
    return p;
}

OAuthCallback OAuthCallback::parse (const juce::String& queryOrUrl)
{
    auto query = queryOrUrl.contains ("?") ? queryOrUrl.fromFirstOccurrenceOf ("?", false, false) : queryOrUrl;
    query = query.upToFirstOccurrenceOf ("#", false, false);

    OAuthCallback cb;
    for (auto& pair : juce::StringArray::fromTokens (query, "&", {}))
    {
        const auto key = juce::URL::removeEscapeChars (pair.upToFirstOccurrenceOf ("=", false, false));
        const auto value = juce::URL::removeEscapeChars (pair.fromFirstOccurrenceOf ("=", false, false).replaceCharacter ('+', ' '));
        if (key == "code")          cb.code = value;
        else if (key == "state")    cb.state = value;
        else if (key == "error")    cb.error = value;
        else if (key == "tone_id")  cb.toneId = value;
        else if (key == "canceled") cb.canceled = value == "true";
    }
    return cb;
}

juce::String formEncode (std::initializer_list<std::pair<juce::String, juce::String>> fields)
{
    juce::StringArray parts;
    for (auto& [k, v] : fields)
        parts.add (juce::URL::addEscapeChars (k, true) + "=" + juce::URL::addEscapeChars (v, true));
    return parts.joinIntoString ("&");
}

//============================================================================== LoopbackServer
LoopbackServer::LoopbackServer() : juce::Thread ("TONE3000 loopback") {}
LoopbackServer::~LoopbackServer() { stop(); }

bool LoopbackServer::start()
{
    if (isRunning())
        return true;
    // Port 0 = any free port; loopback only, so nothing on the network can reach it.
    if (! listener.createListener (0, "127.0.0.1"))
        return false;
    port = listener.getBoundPort();
    if (port <= 0)
    {
        listener.close();
        port = 0;
        return false;
    }
    startThread();
    return true;
}

void LoopbackServer::stop()
{
    if (! isRunning() && ! isThreadRunning())
        return;
    signalThreadShouldExit();
    listener.close(); // unblocks waitForNextConnection()
    stopThread (3000);
    port = 0;
}

juce::String LoopbackServer::redirectUri() const
{
    return isRunning() ? "http://127.0.0.1:" + juce::String (port.load()) + "/callback" : juce::String();
}

void LoopbackServer::run()
{
    while (! threadShouldExit())
    {
        std::unique_ptr<juce::StreamingSocket> connection (listener.waitForNextConnection());
        if (connection == nullptr || threadShouldExit())
            continue;
        handleConnection (*connection);
    }
}

void LoopbackServer::handleConnection (juce::StreamingSocket& connection)
{
    juce::MemoryOutputStream head;
    char buffer[1024];
    while (! head.toString().contains ("\r\n\r\n") && head.getDataSize() < 16 * 1024)
    {
        if (connection.waitUntilReady (true, 2000) != 1) break;
        const auto n = connection.read (buffer, (int) sizeof (buffer), false);
        if (n <= 0) break;
        head.write (buffer, (size_t) n);
    }

    const auto requestLine = head.toString().upToFirstOccurrenceOf ("\r\n", false, false);
    const auto target = requestLine.fromFirstOccurrenceOf (" ", false, false).upToFirstOccurrenceOf (" ", false, false);
    const auto path = target.upToFirstOccurrenceOf ("?", false, false);
    const auto query = target.fromFirstOccurrenceOf ("?", false, false);

    juce::String response;
    if (requestLine.startsWith ("GET ") && path == "/callback")
    {
        const juce::String html =
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>Abyssal + TONE3000</title>"
            "<style>body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;"
            "background:#0a0b0e;color:#e9e9ec;font:16px/1.5 Segoe UI,Helvetica,Arial,sans-serif}"
            "main{text-align:center;padding:32px}h1{font-size:22px;margin:0 0 8px;color:#e8432a}p{margin:0;opacity:.75}</style></head>"
            "<body><main><h1>Connected to Abyssal</h1><p>You can close this tab and return to Abyssal.</p></main></body></html>";
        response << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                 << "Content-Length: " << (int) html.getNumBytesAsUTF8() << "\r\n"
                 << "Cache-Control: no-store\r\nConnection: close\r\n\r\n" << html;

        if (onCallback != nullptr)
            juce::MessageManager::callAsync ([cb = onCallback, query] { cb (query); });
    }
    else
    {
        response << "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    }

    connection.write (response.toRawUTF8(), (int) response.getNumBytesAsUTF8());
    connection.close();
}

//============================================================================== SecureStore
namespace
{
    std::unique_ptr<juce::PropertiesFile> tokenFile()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "tone3000";
        o.filenameSuffix = ".settings";
        o.folderName = "Abyssal";
        o.osxLibrarySubFolder = "Application Support";
        return std::make_unique<juce::PropertiesFile> (o);
    }
}

SecureStore::SecureStore (juce::String a) : account (std::move (a)) {}

#if defined(_WIN32)
bool SecureStore::save (const juce::String& secret)
{
    DATA_BLOB in { (DWORD) secret.getNumBytesAsUTF8(), (BYTE*) secret.toRawUTF8() };
    DATA_BLOB out {};
    if (! CryptProtectData (&in, L"Abyssal TONE3000 tokens", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return false;
    const auto encoded = juce::Base64::toBase64 (out.pbData, out.cbData);
    LocalFree (out.pbData);

    auto file = tokenFile();
    file->setValue (account, encoded);
    return file->saveIfNeeded();
}

juce::String SecureStore::load()
{
    const auto encoded = tokenFile()->getValue (account);
    if (encoded.isEmpty())
        return {};
    juce::MemoryOutputStream raw;
    if (! juce::Base64::convertFromBase64 (raw, encoded))
        return {};

    DATA_BLOB in { (DWORD) raw.getDataSize(), (BYTE*) raw.getData() };
    DATA_BLOB out {};
    if (! CryptUnprotectData (&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return {};
    auto secret = juce::String::fromUTF8 ((const char*) out.pbData, (int) out.cbData);
    LocalFree (out.pbData);
    return secret;
}
#else
// Non-Windows builds keep tokens in memory only (sign in once per session).
bool SecureStore::save (const juce::String&) { return false; }
juce::String SecureStore::load() { return {}; }
#endif

void SecureStore::remove()
{
    auto file = tokenFile();
    file->removeValue (account);
    file->saveIfNeeded();
}

} // namespace abyssal::t3k
