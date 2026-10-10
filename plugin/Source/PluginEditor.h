#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "StudioClient.h"

namespace LoopsmithColours
{
    const juce::Colour body   { 0xff2b3038 };
    const juce::Colour panel  { 0xff353b45 };
    const juce::Colour well   { 0xff1d2127 };
    const juce::Colour line   { 0xff4a525e };
    const juce::Colour text   { 0xffece9e3 };
    const juce::Colour muted  { 0xff9aa2ad };
    const juce::Colour beat   { 0xffff8a3d };
    const juce::Colour remix  { 0xffff5c8a };
    const juce::Colour stem   { 0xff3dd6c6 };
    const juce::Colour sample { 0xfff5c542 };
    const juce::Colour midi   { 0xff9b8cff };
    const juce::Colour bad    { 0xffff6b6b };

    juce::Colour forKind (const juce::String& kind);
}

/** The studio web page, kept on your own studio's address. */
class StudioBrowser : public juce::WebBrowserComponent
{
public:
    StudioBrowser (const Options& options) : WebBrowserComponent (options) {}

    std::function<juce::String()> allowedOrigin;
    std::function<void (const juce::String& url)> onDownloadRequested;

    bool pageAboutToLoad (const juce::String& newURL) override;
    void newWindowAttemptingToLoad (const juce::String& newURL) override;
};

/** The bar at the bottom: shows the sound you picked; drag it from here into your DAW. */
class DragStrip : public juce::Component
{
public:
    void setIdle();
    void setFetching (const juce::String& title, const juce::String& kind);
    void setReady (const juce::File& file, const juce::String& title, const juce::String& kind);
    void setError (const juce::String& message);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override  { repaint(); }
    void mouseExit (const juce::MouseEvent&) override   { repaint(); }

    DragStrip();

private:
    enum class Mode { idle, fetching, ready, error } mode = Mode::idle;
    juce::File file;
    juce::String title, kind, message;
    bool dragStarted = false;
    juce::TextButton revealButton { "Show in Finder" };
};

/** First-run instructions shown until a studio link is entered. */
class WelcomePanel : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
};

class LoopsmithEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::Timer
{
public:
    explicit LoopsmithEditor (LoopsmithProcessor&);
    ~LoopsmithEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void openStudio();
    void onHello (const juce::var& data);
    void onDragRequest (const juce::var& data);
    void onDownloadRequest (const juce::String& url);
    void onCaptureClicked();
    void sendCapture();
    void toastInPage (const juce::String& message, bool bad = false);
    juce::String studioOrigin() const;

    LoopsmithProcessor& processor;
    StudioClient client;
    juce::LookAndFeel_V4 lookAndFeel;

    juce::Label logo;
    juce::TextEditor urlBox;
    juce::TextButton openButton { "Open studio" };
    juce::TextButton captureButton { "Capture this track" };
    juce::TextButton cancelButton { "Cancel" };
    juce::Label statusLabel;

    std::unique_ptr<StudioBrowser> browser;
    WelcomePanel welcome;
    DragStrip dragStrip;

    bool sending = false;
    juce::File captureFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopsmithEditor)
};
