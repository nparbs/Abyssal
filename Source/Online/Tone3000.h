// TONE3000 API client for Abyssal (https://www.tone3000.com/api).
//
// Structure adapted from TONE3000's MIT-licensed JUCE example (github.com/tone-3000/t3k-api),
// Copyright (c) 2026 TONE3000. Uses only what the free, non-commercial tier allows: the select_tone
// browser flow, tone/model lookups for downloading, and the bounded list endpoints.
#pragma once

#include "T3KAuth.h"

namespace abyssal::t3k
{

struct EmbeddedUser
{
    juce::String username, displayName, avatarUrl, url;
    bool verified = false;
};

struct Tone
{
    int id = 0;
    juce::String title, description, gear, format, license, url;
    EmbeddedUser user;
    int modelsCount = 0, downloadsCount = 0, favoritesCount = 0;
    juce::StringArray sizes;

    static Tone fromVar (const juce::var&);
    bool isIR() const { return format == "ir"; }
};

struct Model
{
    int id = 0, toneId = 0;
    juce::String name, modelUrl, size, architecture;

    static Model fromVar (const juce::var&);
    juce::String extension() const;
};

template <typename T>
struct Result
{
    bool ok = false;
    T value {};
    juce::String error;
    int status = 0;
};

/** Shared by every plugin instance (juce::SharedResourcePointer). All callbacks run on the message
    thread. Broadcasts a change message when the connection state changes. */
class Client : public juce::ChangeBroadcaster
{
public:
    Client();
    ~Client() override;

    static constexpr const char* origin = "https://www.tone3000.com";

    // ---- key + session
    void setPublishableKey (const juce::String& key) { publishableKey = key.trim(); }
    juce::String getPublishableKey() const { return publishableKey; }
    bool hasKey() const { return publishableKey.isNotEmpty(); }
    bool isConnected() const;
    void disconnect();

    // ---- browser flows (system browser + loopback redirect)
    /** Sign in only. */
    void connect (std::function<void (Result<bool>)> done);
    /** Opens the TONE3000 catalog filtered to `format` ("nam" / "ir"); returns the picked tone id. */
    void selectTone (const juce::String& format, std::function<void (Result<int>)> done);
    bool isFlowActive() const { return flowDone != nullptr; }
    void cancelFlow();

    // ---- API
    enum class List { favorited, downloaded, created, trending, latest };
    void listTones (List which, int page, std::function<void (Result<juce::Array<Tone>>)> done);
    void getTone (int id, std::function<void (Result<Tone>)> done);
    /** All models of a tone: A1/custom and A2 NAM models, or the IRs. */
    void listModels (int toneId, std::function<void (Result<juce::Array<Model>>)> done);
    void downloadModel (const Model& model, const juce::File& target, std::function<void (Result<juce::File>)> done);

private:
    struct Tokens
    {
        juce::String access, refresh;
        juce::int64 expiresAtMs = 0;
        bool valid() const { return access.isNotEmpty() && refresh.isNotEmpty(); }
    };

    void startFlow (const juce::StringPairArray& extra, std::function<void (const OAuthCallback&, const juce::String& error)> onDone);
    void finishFlow (const OAuthCallback& cb, const juce::String& error);
    bool exchangeCode (const juce::String& code, const juce::String& verifier, const juce::String& redirectUri, juce::String& error);

    HttpResponse authed (const juce::String& method, const juce::String& pathOrUrl);
    juce::String accessToken (bool forceRefresh);
    bool refreshNow();
    void storeTokens (const Tokens& t);
    Tokens currentTokens() const;
    template <typename Fn> void runAsync (Fn&& job);

    juce::String publishableKey;
    SecureStore store { "oauth-tokens" };
    mutable juce::CriticalSection lock;
    std::mutex refreshMutex;
    Tokens tokens;

    LoopbackServer server;
    Pkce pkce;
    std::function<void (const OAuthCallback&, const juce::String&)> flowDone;
    juce::String flowRedirectUri;

    // Guards message-thread callbacks queued by workers against running after destruction.
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    juce::ThreadPool pool { 2 };
};

/** Sanitised file name for a downloaded model. */
juce::String safeFileName (const juce::String& name);

} // namespace abyssal::t3k
