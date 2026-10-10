#include "StudioClient.h"

namespace
{
    // Runs `fn` on the message thread unless the plugin window has closed meanwhile.
    template <typename SharedPtr>
    void onMessageThread (SharedPtr s, std::function<void()> fn)
    {
        juce::MessageManager::callAsync ([s, fn = std::move (fn)]
        {
            if (s->alive.load())
                fn();
        });
    }

    juce::String friendlyStatus (int status)
    {
        if (status == 401) return "The studio asked you to log in again. Log in inside the plugin and retry.";
        if (status == 404) return "That sound isn't in the studio any more.";
        if (status == 0)   return "Can't reach the studio. Is your RunPod machine running?";
        return "The studio answered with an error (" + juce::String (status) + ").";
    }
}

StudioClient::StudioClient() = default;

StudioClient::~StudioClient()
{
    shared->alive.store (false);
}

juce::ThreadPool& StudioClient::pool()
{
    static juce::ThreadPool p (2);
    return p;
}

void StudioClient::setConnection (const juce::String& baseUrl, const juce::String& newToken)
{
    const juce::ScopedLock sl (shared->lock);
    shared->base = baseUrl.trimCharactersAtEnd ("/");
    shared->token = newToken;
}

bool StudioClient::isReady() const
{
    return shared->baseUrl().isNotEmpty();
}

juce::String StudioClient::getBaseUrl() const
{
    return shared->baseUrl();
}

juce::String StudioClient::Shared::baseUrl() const
{
    const juce::ScopedLock sl (lock);
    return base;
}

juce::String StudioClient::Shared::headers() const
{
    const juce::ScopedLock sl (lock);
    return token.isNotEmpty() ? "X-Loopsmith-Token: " + token : juce::String();
}

juce::File StudioClient::libraryFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Loopsmith Library");
}

juce::File StudioClient::localPathFor (const juce::String& relativePath)
{
    auto parts = juce::StringArray::fromTokens (relativePath.replaceCharacter ('\\', '/'), "/", "");
    parts.removeEmptyStrings();
    if (parts.isEmpty() || parts.contains ("..") || parts[0].startsWithChar ('.'))
        return {};
    auto f = libraryFolder();
    for (const auto& p : parts)
        f = f.getChildFile (juce::File::createLegalFileName (p));
    return f;
}

bool StudioClient::download (const juce::URL& url, const juce::String& headers, const juce::File& dest,
                             juce::String& error, juce::String* suggestedName)
{
    int status = 0;
    juce::StringPairArray responseHeaders;
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders (headers)
                       .withConnectionTimeoutMs (30000)
                       .withStatusCode (&status)
                       .withResponseHeaders (&responseHeaders)
                       .withNumRedirectsToFollow (3);
    auto in = url.createInputStream (options);
    if (in == nullptr || status >= 400)
    {
        error = friendlyStatus (in == nullptr ? 0 : status);
        return false;
    }

    if (suggestedName != nullptr)
    {
        // Content-Disposition: attachment; filename="Name.wav"  (or filename*=utf-8''Name.wav)
        for (const auto& key : responseHeaders.getAllKeys())
            if (key.equalsIgnoreCase ("content-disposition"))
            {
                const auto cd = responseHeaders[key];
                auto name = cd.fromFirstOccurrenceOf ("filename*=utf-8''", false, true);
                if (name.isNotEmpty())
                    name = juce::URL::removeEscapeChars (name.upToFirstOccurrenceOf (";", false, false));
                else
                    name = cd.fromFirstOccurrenceOf ("filename=", false, true).unquoted().upToFirstOccurrenceOf (";", false, false);
                if (name.isNotEmpty())
                    *suggestedName = juce::File::createLegalFileName (name.trim());
            }
    }

    dest.getParentDirectory().createDirectory();
    const auto part = dest.getSiblingFile ("." + dest.getFileName() + ".part");
    {
        juce::FileOutputStream out (part);
        if (! out.openedOk())
        {
            error = "Couldn't save to " + dest.getParentDirectory().getFullPathName();
            return false;
        }
        out.setPosition (0);
        out.truncate();
        if (out.writeFromInputStream (*in, -1) <= 0 && in->getTotalLength() != 0)
        {
            error = "The download was interrupted. Try again.";
            part.deleteFile();
            return false;
        }
    }
    if (! part.moveFileTo (dest))
    {
        error = "Couldn't move the download into place.";
        part.deleteFile();
        return false;
    }
    return true;
}

void StudioClient::fetchForDrag (const Item& item, FileCallback onDone)
{
    auto target = localPathFor (item.path);
    if (target == juce::File())
        target = libraryFolder().getChildFile ("Dragged").getChildFile (juce::File::createLegalFileName (item.filename));

    if (target.existsAsFile())   // already synced or fetched earlier
    {
        onDone (target, {});
        return;
    }
    const auto url = juce::URL (getBaseUrl() + "/api/download/" + juce::URL::addEscapeChars (item.id, true));
    pool().addJob ([s = shared, url, target, onDone]
    {
        juce::String error;
        const bool ok = download (url, s->headers(), target, error);
        onMessageThread (s, [=] { onDone (ok ? target : juce::File(), error); });
    });
}

void StudioClient::downloadToDownloads (const juce::String& rawUrl, FileCallback onDone)
{
    auto absolute = rawUrl.startsWithIgnoreCase ("http") ? rawUrl : getBaseUrl() + rawUrl;
    const auto url = juce::URL (absolute);
    const auto downloads = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
    pool().addJob ([s = shared, url, downloads, onDone]
    {
        juce::String error, name;
        const auto temp = downloads.getChildFile (".loopsmith-download-" + juce::String (juce::Random::getSystemRandom().nextInt64()));
        const bool ok = download (url, s->headers(), temp, error, &name);
        juce::File finalFile;
        if (ok)
        {
            if (name.isEmpty())
                name = juce::URL::removeEscapeChars (url.getFileName());
            finalFile = downloads.getNonexistentChildFile (juce::File (name).getFileNameWithoutExtension(),
                                                           juce::File (name).getFileExtension(), false);
            if (! temp.moveFileTo (finalFile))
                finalFile = temp;
        }
        onMessageThread (s, [=] { onDone (finalFile, ok ? juce::String() : error); });
    });
}

void StudioClient::upload (const juce::File& audioFile, UploadCallback onDone)
{
    const auto url = juce::URL (getBaseUrl() + "/api/upload").withFileToUpload ("file", audioFile, "audio/wav");
    pool().addJob ([s = shared, url, onDone]
    {
        int status = 0;
        auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                           .withExtraHeaders (s->headers())
                           .withConnectionTimeoutMs (60000)
                           .withStatusCode (&status);
        juce::String id, title, error;
        if (auto in = url.createInputStream (options))
        {
            const auto body = in->readEntireStreamAsString();
            const auto json = juce::JSON::parse (body);
            if (status < 400 && json.hasProperty ("id"))
            {
                id = json["id"].toString();
                title = json["title"].toString();
            }
            else
            {
                error = json.hasProperty ("detail") ? json["detail"].toString() : friendlyStatus (status);
            }
        }
        else
        {
            error = friendlyStatus (0);
        }
        onMessageThread (s, [=] { onDone (id, title, error); });
    });
}
