// Adapted from TONE3000's MIT-licensed JUCE example (github.com/tone-3000/t3k-api), Copyright (c) 2026 TONE3000.

#include "Tone3000.h"

namespace abyssal::t3k
{

namespace
{
    constexpr juce::int64 refreshLeadMs = 60 * 1000;
    const juce::String tokenPath = "/api/v1/oauth/token";

    juce::String str (const juce::var& v, const char* key)
    {
        const auto p = v.getProperty (key, juce::var());
        return p.isVoid() || p.isUndefined() ? juce::String() : p.toString();
    }
    int num (const juce::var& v, const char* key) { return (int) v.getProperty (key, 0); }

    juce::String errorMessage (const HttpResponse& res)
    {
        if (res.status == 0) return res.error;
        const auto body = res.json();
        for (auto* key : { "error_description", "message", "error" })
        {
            const auto s = str (body, key);
            if (s.isNotEmpty()) return s;
        }
        if (res.status == 401) return "Your TONE3000 session expired - please connect again.";
        if (res.status == 403) return "TONE3000 refused the request (403).";
        if (res.status == 429) return "TONE3000 rate limit reached - wait a minute and try again.";
        return "TONE3000 returned HTTP " + juce::String (res.status);
    }
}

//============================================================================== types
Tone Tone::fromVar (const juce::var& v)
{
    Tone t;
    t.id = num (v, "id");
    t.title = str (v, "title");
    t.description = str (v, "description");
    t.gear = str (v, "gear");
    t.format = str (v, "format");
    t.license = str (v, "license");
    t.url = str (v, "url");
    t.modelsCount = num (v, "models_count");
    t.downloadsCount = num (v, "downloads_count");
    t.favoritesCount = num (v, "favorites_count");
    if (auto* arr = v.getProperty ("sizes", juce::var()).getArray())
        for (auto& s : *arr) t.sizes.add (s.toString());

    const auto u = v.getProperty ("user", juce::var());
    t.user.username = str (u, "username");
    t.user.displayName = str (u, "display_name");
    t.user.avatarUrl = str (u, "avatar_url");
    t.user.url = str (u, "url");
    t.user.verified = (bool) u.getProperty ("is_verified", false);
    return t;
}

Model Model::fromVar (const juce::var& v)
{
    Model m;
    m.id = num (v, "id");
    m.toneId = num (v, "tone_id");
    m.name = str (v, "name");
    m.modelUrl = str (v, "model_url");
    m.size = str (v, "size");
    m.architecture = str (v, "architecture_version");
    return m;
}

juce::String Model::extension() const
{
    const auto file = juce::URL (modelUrl).getSubPath().fromLastOccurrenceOf ("/", false, false);
    return file.contains (".") ? "." + file.fromLastOccurrenceOf (".", false, false).toLowerCase() : juce::String();
}

juce::String safeFileName (const juce::String& name)
{
    auto s = juce::File::createLegalFileName (name.trim()).replaceCharacters ("\t\r\n", "   ");
    while (s.contains ("  ")) s = s.replace ("  ", " ");
    s = s.trim().substring (0, 100);
    return s.isEmpty() ? juce::String ("model") : s;
}

//============================================================================== lifecycle
Client::Client()
{
    const auto json = juce::JSON::parse (store.load());
    tokens.access = str (json, "access_token");
    tokens.refresh = str (json, "refresh_token");
    tokens.expiresAtMs = (juce::int64) json.getProperty ("expires_at", 0);
    server.onCallback = [this, alive = alive] (juce::String query)
    {
        if (! *alive) return;
        server.stop();
        finishFlow (OAuthCallback::parse (query), {});
    };
}

Client::~Client()
{
    *alive = false;
    server.onCallback = nullptr;
    server.stop();
    pool.removeAllJobs (true, 5000);
}

template <typename Fn>
void Client::runAsync (Fn&& job)
{
    pool.addJob (std::forward<Fn> (job));
}

Client::Tokens Client::currentTokens() const
{
    const juce::ScopedLock sl (lock);
    return tokens;
}

bool Client::isConnected() const { return currentTokens().valid(); }

void Client::storeTokens (const Tokens& t)
{
    const bool wasConnected = isConnected();
    {
        const juce::ScopedLock sl (lock);
        tokens = t;
    }
    if (t.valid())
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("access_token", t.access);
        obj->setProperty ("refresh_token", t.refresh);
        obj->setProperty ("expires_at", t.expiresAtMs);
        store.save (juce::JSON::toString (juce::var (obj), true));
    }
    else
    {
        store.remove();
    }
    juce::ignoreUnused (wasConnected);
    sendChangeMessage();
}

void Client::disconnect() { storeTokens ({}); }

//============================================================================== browser flows
void Client::startFlow (const juce::StringPairArray& extra, std::function<void (const OAuthCallback&, const juce::String&)> onDone)
{
    if (isFlowActive())
        cancelFlow();

    flowDone = std::move (onDone);
    if (! hasKey())
        return finishFlow ({}, "Add your TONE3000 publishable key first.");
    if (! server.start())
        return finishFlow ({}, "Couldn't open a local port for the sign-in redirect.");

    pkce = Pkce::generate();
    flowRedirectUri = server.redirectUri();

    auto url = juce::URL (juce::String (origin) + "/api/v1/oauth/authorize")
                   .withParameter ("client_id", publishableKey)
                   .withParameter ("redirect_uri", flowRedirectUri)
                   .withParameter ("response_type", "code")
                   .withParameter ("code_challenge", pkce.codeChallenge)
                   .withParameter ("code_challenge_method", "S256")
                   .withParameter ("state", pkce.state);
    for (auto& key : extra.getAllKeys())
        url = url.withParameter (key, extra[key]);

    if (! url.launchInDefaultBrowser())
    {
        server.stop();
        finishFlow ({}, "Couldn't open your web browser.");
    }
}

void Client::cancelFlow()
{
    server.stop();
    OAuthCallback cb;
    cb.canceled = true;
    finishFlow (cb, {});
}

void Client::finishFlow (const OAuthCallback& cb, const juce::String& error)
{
    auto done = std::exchange (flowDone, nullptr);
    if (done == nullptr)
        return;

    if (error.isNotEmpty())          return done (cb, error);
    if (cb.canceled && cb.code.isEmpty()) return done (cb, "canceled");
    if (cb.state != pkce.state)      return done (cb, "The sign-in response didn't match this request (state mismatch).");
    if (cb.error.isNotEmpty())       return done (cb, "TONE3000: " + cb.error);
    if (cb.code.isEmpty())           return done (cb, "TONE3000 didn't return an authorization code.");

    // The PKCE verifier is single-use; the exchange runs on a worker.
    const auto verifier = std::exchange (pkce.codeVerifier, {});
    const auto redirect = flowRedirectUri;
    runAsync ([this, cb, verifier, redirect, done]
    {
        juce::String err;
        exchangeCode (cb.code, verifier, redirect, err);
        juce::MessageManager::callAsync ([done, cb, err] { done (cb, err); });
    });
}

bool Client::exchangeCode (const juce::String& code, const juce::String& verifier, const juce::String& redirectUri, juce::String& error)
{
    HttpRequest req;
    req.url = juce::URL (juce::String (origin) + tokenPath);
    req.method = "POST";
    req.headers.set ("Content-Type", "application/x-www-form-urlencoded");
    req.body = formEncode ({ { "grant_type", "authorization_code" }, { "code", code }, { "code_verifier", verifier },
                             { "redirect_uri", redirectUri }, { "client_id", publishableKey } });
    const auto res = sendSync (req);
    if (! res.ok())
    {
        error = errorMessage (res);
        return false;
    }
    const auto body = res.json();
    Tokens t;
    t.access = str (body, "access_token");
    t.refresh = str (body, "refresh_token");
    t.expiresAtMs = juce::Time::currentTimeMillis() + (juce::int64) ((double) body.getProperty ("expires_in", 3600) * 1000.0);
    juce::MessageManager::callAsync ([this, alive = alive, t] { if (*alive) storeTokens (t); });
    {
        const juce::ScopedLock sl (lock);
        tokens = t; // usable immediately by the next worker request
    }
    return true;
}

void Client::connect (std::function<void (Result<bool>)> done)
{
    startFlow ({}, [done] (const OAuthCallback&, const juce::String& err)
    {
        done ({ err.isEmpty(), err.isEmpty(), err == "canceled" ? juce::String() : err });
    });
}

void Client::selectTone (const juce::String& format, std::function<void (Result<int>)> done)
{
    juce::StringPairArray extra;
    extra.set ("prompt", "select_tone");
    extra.set ("format", format);
    extra.set ("preview", "true");
    extra.set ("menubar", "true");
    startFlow (extra, [done] (const OAuthCallback& cb, const juce::String& err)
    {
        Result<int> r;
        r.error = err == "canceled" ? juce::String() : err;
        r.value = cb.toneId.getIntValue();
        r.ok = err.isEmpty() && r.value > 0;
        if (err.isEmpty() && r.value <= 0)
            r.error = "Connected, but no tone was selected.";
        done (r);
    });
}

//============================================================================== authenticated requests (worker threads)
bool Client::refreshNow()
{
    std::lock_guard<std::mutex> single (refreshMutex);
    auto current = currentTokens();
    if (! current.valid()) return false;
    if (juce::Time::currentTimeMillis() < current.expiresAtMs - refreshLeadMs) return true; // someone else refreshed

    HttpRequest req;
    req.url = juce::URL (juce::String (origin) + tokenPath);
    req.method = "POST";
    req.headers.set ("Content-Type", "application/x-www-form-urlencoded");
    req.body = formEncode ({ { "grant_type", "refresh_token" }, { "refresh_token", current.refresh }, { "client_id", publishableKey } });

    const auto res = sendSync (req);
    if (res.ok())
    {
        const auto body = res.json();
        Tokens t;
        t.access = str (body, "access_token");
        t.refresh = str (body, "refresh_token");
        t.expiresAtMs = juce::Time::currentTimeMillis() + (juce::int64) ((double) body.getProperty ("expires_in", 3600) * 1000.0);
        { const juce::ScopedLock sl (lock); tokens = t; }
        juce::MessageManager::callAsync ([this, alive = alive, t] { if (*alive) storeTokens (t); });
        return true;
    }
    if (res.status == 400 || res.status == 401)
    {
        // The refresh token itself is dead: the session is over.
        { const juce::ScopedLock sl (lock); tokens = {}; }
        juce::MessageManager::callAsync ([this, alive = alive] { if (*alive) storeTokens ({}); });
        return false;
    }
    return true; // transient failure: keep tokens, this request will fail
}

juce::String Client::accessToken (bool forceRefresh)
{
    auto current = currentTokens();
    if (! current.valid()) return {};
    if (forceRefresh || juce::Time::currentTimeMillis() > current.expiresAtMs - refreshLeadMs)
    {
        if (forceRefresh) { const juce::ScopedLock sl (lock); tokens.expiresAtMs = 0; }
        if (! refreshNow()) return {};
        current = currentTokens();
    }
    return current.access;
}

HttpResponse Client::authed (const juce::String& method, const juce::String& pathOrUrl)
{
    auto token = accessToken (false);
    if (token.isEmpty())
    {
        HttpResponse r;
        r.status = 401;
        return r;
    }

    auto send = [&] (const juce::String& bearer)
    {
        HttpRequest req;
        req.url = juce::URL (pathOrUrl.startsWith ("http") ? pathOrUrl : juce::String (origin) + pathOrUrl);
        req.method = method;
        req.headers.set ("Authorization", "Bearer " + bearer);
        req.headers.set ("Accept", "application/json");
        req.timeoutMs = 60000;
        return sendSync (req);
    };

    auto res = send (token);
    if (res.status == 401) // expiry race: refresh once and retry
        if (auto fresh = accessToken (true); fresh.isNotEmpty())
            res = send (fresh);
    return res;
}

//============================================================================== API calls
void Client::listTones (List which, int page, std::function<void (Result<TonePage>)> done)
{
    juce::String path;
    switch (which)
    {
        case List::favorited:  path = "/api/v1/tones/favorited?page_size=50&page=" + juce::String (page); break;
        case List::downloaded: path = "/api/v1/tones/downloaded?page_size=50&page=" + juce::String (page); break;
        case List::created:    path = "/api/v1/tones/created?page_size=50&page=" + juce::String (page); break;
        case List::trending:   path = "/api/v1/tones/trending"; break;
        case List::latest:     path = "/api/v1/tones/latest"; break;
    }

    runAsync ([this, path, page, done]
    {
        const auto res = authed ("GET", path);
        Result<TonePage> r;
        r.status = res.status;
        if (res.ok())
        {
            r.ok = true;
            const auto body = res.json();
            const auto data = body.isArray() ? body : body.getProperty ("data", juce::var());
            if (auto* arr = data.getArray())
                for (auto& t : *arr) r.value.tones.add (Tone::fromVar (t));
            r.value.page = body.isObject() ? (int) body.getProperty ("page", page) : page;
            r.value.totalPages = body.isObject() ? std::max (1, (int) body.getProperty ("total_pages", 1)) : 1;
        }
        else r.error = errorMessage (res);
        juce::MessageManager::callAsync ([done, r] { done (r); });
    });
}

void Client::setFavorite (int toneId, bool favorite, std::function<void (Result<bool>)> done)
{
    runAsync ([this, toneId, favorite, done]
    {
        // Both directions are idempotent: PUT answers 200 with the favorite, DELETE 204.
        const auto res = authed (favorite ? "PUT" : "DELETE", "/api/v1/tones/" + juce::String (toneId) + "/favorite");
        Result<bool> r;
        r.status = res.status;
        r.ok = res.ok();
        r.value = favorite;
        if (! r.ok) r.error = errorMessage (res);
        juce::MessageManager::callAsync ([done, r] { done (r); });
    });
}

void Client::fetchImage (const juce::String& url, std::function<void (juce::Image)> done)
{
    runAsync ([url, done]
    {
        HttpRequest req;
        req.url = juce::URL (url);
        req.timeoutMs = 15000;
        const auto res = sendSync (req);
        auto image = res.ok() ? juce::ImageFileFormat::loadFrom (res.body.getData(), res.body.getSize()) : juce::Image();
        juce::MessageManager::callAsync ([done, image] { done (image); });
    });
}

void Client::getTone (int id, std::function<void (Result<Tone>)> done)
{
    runAsync ([this, id, done]
    {
        const auto res = authed ("GET", "/api/v1/tones/" + juce::String (id));
        Result<Tone> r;
        r.status = res.status;
        if (res.ok()) { r.ok = true; r.value = Tone::fromVar (res.json()); }
        else r.error = errorMessage (res);
        juce::MessageManager::callAsync ([done, r] { done (r); });
    });
}

void Client::listModels (int toneId, std::function<void (Result<juce::Array<Model>>)> done)
{
    runAsync ([this, toneId, done]
    {
        // Without `architecture` the API returns A1 + custom models (and IRs); A2 needs its own query.
        Result<juce::Array<Model>> r;
        juce::Array<int> seen;
        for (auto arch : { juce::String(), juce::String ("2") })
        {
            const auto path = "/api/v1/models?tone_id=" + juce::String (toneId) + "&page_size=300"
                              + (arch.isNotEmpty() ? "&architecture=" + arch : juce::String());
            const auto res = authed ("GET", path);
            if (! res.ok())
            {
                if (arch.isEmpty()) { r.error = errorMessage (res); r.status = res.status; break; }
                continue; // A2 query is best-effort
            }
            r.ok = true;
            if (auto* arr = res.json().getProperty ("data", juce::var()).getArray())
                for (auto& m : *arr)
                {
                    auto model = Model::fromVar (m);
                    if (! seen.contains (model.id)) { seen.add (model.id); r.value.add (model); }
                }
        }
        juce::MessageManager::callAsync ([done, r] { done (r); });
    });
}

void Client::downloadModel (const Model& model, const juce::File& target, std::function<void (Result<juce::File>)> done)
{
    runAsync ([this, model, target, done]
    {
        const auto res = authed ("GET", model.modelUrl);
        Result<juce::File> r;
        r.status = res.status;
        if (! res.ok())
            r.error = errorMessage (res);
        else if (res.body.getSize() == 0)
            r.error = "TONE3000 sent an empty file for " + model.name;
        else if (target.getParentDirectory().createDirectory() && target.replaceWithData (res.body.getData(), res.body.getSize()))
        {
            r.ok = true;
            r.value = target;
        }
        else
            r.error = "Couldn't write " + target.getFullPathName();
        juce::MessageManager::callAsync ([done, r] { done (r); });
    });
}

} // namespace abyssal::t3k
