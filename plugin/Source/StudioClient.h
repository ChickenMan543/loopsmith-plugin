#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <functional>

/*  Talks to the Loopsmith studio from the plugin itself (not the web page):
    downloads sounds so they can be dragged into the DAW, and uploads captured audio.

    The page hands us a short-lived key ("token") after you log in, because the plugin
    can't read the page's login cookie. All network work runs on a background thread;
    callbacks come back on the message thread.
*/
class StudioClient
{
public:
    struct Item
    {
        juce::String id, title, kind, path, filename;
    };

    using FileCallback = std::function<void (juce::File file, juce::String error)>;
    using UploadCallback = std::function<void (juce::String newItemId, juce::String title, juce::String error)>;

    StudioClient();
    ~StudioClient();

    void setConnection (const juce::String& baseUrl, const juce::String& token);
    bool isReady() const;
    juce::String getBaseUrl() const;

    /** Where synced/dragged sounds live on this computer (same place Loopsmith Sync uses). */
    static juce::File libraryFolder();

    /** Makes sure the sound is on disk (reusing a Loopsmith Sync copy if there is one). */
    void fetchForDrag (const Item& item, FileCallback onDone);

    /** Downloads any studio URL (a sound or a zip) into ~/Downloads. */
    void downloadToDownloads (const juce::String& url, FileCallback onDone);

    void upload (const juce::File& audioFile, UploadCallback onDone);

    /** Safe local path for a studio-relative path like "Stems/My beat/My beat - Kick.wav". */
    static juce::File localPathFor (const juce::String& relativePath);

private:
    // Shared with background jobs, so a job finishing after the plugin window closes stays safe.
    struct Shared
    {
        juce::CriticalSection lock;
        juce::String base, token;
        std::atomic<bool> alive { true };
        juce::String headers() const;
        juce::String baseUrl() const;
    };
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();

    static juce::ThreadPool& pool();
    static bool download (const juce::URL& url, const juce::String& headers, const juce::File& dest,
                          juce::String& error, juce::String* suggestedName = nullptr);
};
