// TONE3000 sign-in plumbing: HTTP, PKCE, loopback redirect receiver, protected token storage.
//
// Adapted from TONE3000's MIT-licensed JUCE example (github.com/tone-3000/t3k-api, juce/Source/T3K),
// Copyright (c) 2026 TONE3000. Changes: single header, no JuceHeader, PKCE randomness from the OS
// CSPRNG instead of juce::Random, Windows-only secure storage (DPAPI).
#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

namespace abyssal::t3k
{

//============================================================================== HTTP
struct HttpRequest
{
    juce::URL url;
    juce::String method { "GET" };
    juce::StringPairArray headers;
    juce::String body;
    int timeoutMs = 30000;
};

struct HttpResponse
{
    int status = 0;                 // 0 = no response (network error)
    juce::MemoryBlock body;
    juce::String error;

    bool ok() const { return status >= 200 && status < 300; }
    juce::String text() const { return body.toString(); }
    juce::var json() const { return juce::JSON::parse (text()); }
};

/** Blocking request; call from a worker thread only. */
HttpResponse sendSync (const HttpRequest& request);

//============================================================================== OAuth (PKCE S256)
struct Pkce
{
    juce::String codeVerifier, codeChallenge, state;
    static Pkce generate();
};

struct OAuthCallback
{
    juce::String code, state, error, toneId;
    bool canceled = false;
    static OAuthCallback parse (const juce::String& queryOrUrl);
};

juce::String formEncode (std::initializer_list<std::pair<juce::String, juce::String>> fields);

//============================================================================== Loopback redirect receiver
/** Binds an ephemeral port on 127.0.0.1 and serves the OAuth redirect landing page (RFC 8252 §7.3).
    TONE3000 accepts loopback redirect URIs without registering them. */
class LoopbackServer : private juce::Thread
{
public:
    LoopbackServer();
    ~LoopbackServer() override;

    bool start();
    void stop();
    bool isRunning() const { return port != 0; }
    juce::String redirectUri() const;

    /** Called on the message thread with the raw query string of the redirect. */
    std::function<void (juce::String)> onCallback;

private:
    void run() override;
    void handleConnection (juce::StreamingSocket& connection);

    juce::StreamingSocket listener;
    std::atomic<int> port { 0 };
};

//============================================================================== Protected storage
/** Stores a secret encrypted with Windows DPAPI (only this Windows user can decrypt it). */
class SecureStore
{
public:
    explicit SecureStore (juce::String account);
    bool save (const juce::String& secret);
    juce::String load();
    void remove();

private:
    juce::String account;
};

} // namespace abyssal::t3k
