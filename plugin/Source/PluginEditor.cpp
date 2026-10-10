#include "PluginEditor.h"

using namespace LoopsmithColours;

juce::Colour LoopsmithColours::forKind (const juce::String& kind)
{
    if (kind == "remix") return remix;
    if (kind == "stem") return stem;
    if (kind == "sample" || kind == "loop") return sample;
    if (kind == "midi") return midi;
    if (kind == "upload") return muted;
    return beat;
}

namespace
{
    juce::String originOf (const juce::String& url)
    {
        // "https://abc-7870.proxy.runpod.net/anything" -> "https://abc-7870.proxy.runpod.net"
        const auto scheme = url.upToFirstOccurrenceOf ("://", true, false);
        const auto rest = url.fromFirstOccurrenceOf ("://", false, false);
        return (scheme + rest.upToFirstOccurrenceOf ("/", false, false)).toLowerCase();
    }

    juce::String normaliseStudioUrl (juce::String text)
    {
        text = text.trim().unquoted().trim();
        if (text.isEmpty())
            return {};
        if (! text.containsIgnoreCase ("://"))
            text = "https://" + text;
        return originOf (text);
    }

    juce::String formatSeconds (double s)
    {
        const int total = static_cast<int> (s);
        return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
    }
}

// ============================================================================ StudioBrowser

bool StudioBrowser::pageAboutToLoad (const juce::String& newURL)
{
    const auto origin = allowedOrigin ? allowedOrigin() : juce::String();
    if (newURL.startsWithIgnoreCase ("about:") || newURL.startsWithIgnoreCase ("data:")
        || newURL.startsWithIgnoreCase ("blob:") || newURL.startsWithIgnoreCase ("juce:"))
        return true;

    if (origin.isNotEmpty() && originOf (newURL) == origin)
    {
        // File downloads can't happen inside the plugin's page; the plugin fetches them instead.
        const auto path = newURL.substring (origin.length());
        if (path.startsWith ("/api/download/") || path.startsWith ("/api/exports/"))
        {
            if (onDownloadRequested)
                onDownloadRequested (newURL);
            return false;
        }
        return true;
    }

    // Anything else (help links etc.) opens in your normal browser, never inside the plugin.
    juce::URL (newURL).launchInDefaultBrowser();
    return false;
}

void StudioBrowser::newWindowAttemptingToLoad (const juce::String& newURL)
{
    juce::URL (newURL).launchInDefaultBrowser();
}

// ============================================================================ DragStrip

DragStrip::DragStrip()
{
    addChildComponent (revealButton);
    revealButton.onClick = [this] { if (file.existsAsFile()) file.revealToUser(); };
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void DragStrip::setIdle()
{
    mode = Mode::idle;
    revealButton.setVisible (false);
    repaint();
}

void DragStrip::setFetching (const juce::String& t, const juce::String& k)
{
    mode = Mode::fetching;
    title = t;
    kind = k;
    revealButton.setVisible (false);
    repaint();
}

void DragStrip::setReady (const juce::File& f, const juce::String& t, const juce::String& k)
{
    mode = Mode::ready;
    file = f;
    title = t;
    kind = k;
    revealButton.setVisible (true);
    resized();
    repaint();
}

void DragStrip::setError (const juce::String& m)
{
    mode = Mode::error;
    message = m;
    revealButton.setVisible (false);
    repaint();
}

void DragStrip::resized()
{
    revealButton.setBounds (getLocalBounds().removeFromRight (150).reduced (12, 14));
}

void DragStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.fillAll (juce::Colour (0xff30363f));
    g.setColour (line);
    g.drawHorizontalLine (0, 0.0f, static_cast<float> (getWidth()));

    auto inner = r.reduced (12, 9);
    if (revealButton.isVisible())
        inner.removeFromRight (150);

    const auto pad = inner.removeFromLeft (inner.getHeight()).toFloat();
    const auto colour = mode == Mode::error ? bad : (mode == Mode::idle ? line : forKind (kind));
    g.setColour (colour.withAlpha (mode == Mode::fetching ? 0.5f : 1.0f));
    g.fillRoundedRectangle (pad, 8.0f);
    g.setColour (body);
    if (mode == Mode::ready)
    {
        // a little "grab" mark
        for (int i = 0; i < 3; ++i)
            g.fillRoundedRectangle (pad.getCentreX() - 9.0f, pad.getY() + 11.0f + static_cast<float> (i) * 7.0f, 18.0f, 3.0f, 1.5f);
    }

    inner.removeFromLeft (12);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.setColour (text);
    juce::String line1, line2;
    switch (mode)
    {
        case Mode::idle:
            line1 = "Pick a sound to drag into your DAW";
            line2 = "Click the arrow button on any sound in the studio, then drag it from this bar onto a track.";
            break;
        case Mode::fetching:
            line1 = "Getting \"" + title + "\"...";
            line2 = "Downloading it to Music > Loopsmith Library.";
            break;
        case Mode::ready:
            line1 = file.getFileName();
            line2 = isMouseOverOrDragging() ? "Drag this onto a track or into your project" : "Ready: drag this bar into your DAW";
            break;
        case Mode::error:
            line1 = "Couldn't get that sound";
            line2 = message;
            break;
    }
    g.drawFittedText (line1, inner.removeFromTop (inner.getHeight() / 2), juce::Justification::bottomLeft, 1);
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (mode == Mode::error ? bad : muted);
    g.drawFittedText (line2, inner, juce::Justification::topLeft, 1);
}

void DragStrip::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
}

void DragStrip::mouseDrag (const juce::MouseEvent& e)
{
    if (mode != Mode::ready || dragStarted || e.getDistanceFromDragStart() < 6 || ! file.existsAsFile())
        return;
    dragStarted = true;
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

// ============================================================================ WelcomePanel

void WelcomePanel::paint (juce::Graphics& g)
{
    g.fillAll (body);
    auto r = getLocalBounds().reduced (48, 40);
    g.setColour (text);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("Your studio, inside your DAW", r.removeFromTop (44), juce::Justification::topLeft);
    r.removeFromTop (12);

    const juce::StringArray steps {
        "Start your studio on RunPod (in its terminal: bash /workspace/Loopsmith/start.sh).",
        "Paste the studio link it prints into the box above, then click Open studio.",
        "Log in with your studio password. Everything works like the browser version.",
        "Click the arrow button on any sound, then drag it from the bar at the bottom onto a track.",
        "Capture this track records what plays through this plugin and sends it to the studio for stems, MIDI or chopping."
    };
    g.setFont (juce::FontOptions (16.0f));
    for (int i = 0; i < steps.size(); ++i)
    {
        auto row = r.removeFromTop (46);
        auto num = row.removeFromLeft (30).withSizeKeepingCentre (26, 26).toFloat();
        g.setColour (i < 3 ? beat : stem);
        g.fillRoundedRectangle (num, 6.0f);
        g.setColour (body);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (juce::String (i + 1), num, juce::Justification::centred);
        row.removeFromLeft (12);
        g.setColour (text);
        g.setFont (juce::FontOptions (16.0f));
        g.drawFittedText (steps[i], row, juce::Justification::centredLeft, 2);
    }
}

// ============================================================================ Editor

LoopsmithEditor::LoopsmithEditor (LoopsmithProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    lookAndFeel.setColourScheme ({ body, panel, panel, line, text, panel, text, beat, text });
    lookAndFeel.setColour (juce::TextButton::buttonColourId, panel);
    lookAndFeel.setColour (juce::TextButton::textColourOffId, text);
    lookAndFeel.setColour (juce::TextEditor::backgroundColourId, well);
    lookAndFeel.setColour (juce::TextEditor::outlineColourId, line);
    lookAndFeel.setColour (juce::TextEditor::focusedOutlineColourId, stem);
    lookAndFeel.setColour (juce::TextEditor::textColourId, text);
    setLookAndFeel (&lookAndFeel);

    logo.setText ("Loopsmith", juce::dontSendNotification);
    logo.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    logo.setColour (juce::Label::textColourId, text);
    addAndMakeVisible (logo);

    urlBox.setTextToShowWhenEmpty ("Paste your studio link (https://...-7870.proxy.runpod.net)", muted);
    urlBox.setText (processor.getStudioUrl(), false);
    urlBox.setFont (juce::FontOptions (15.0f));
    urlBox.onReturnKey = [this] { openStudio(); };
    addAndMakeVisible (urlBox);

    openButton.setColour (juce::TextButton::buttonColourId, beat);
    openButton.setColour (juce::TextButton::textColourOffId, body);
    openButton.onClick = [this] { openStudio(); };
    addAndMakeVisible (openButton);

    captureButton.onClick = [this] { onCaptureClicked(); };
    addAndMakeVisible (captureButton);
    cancelButton.onClick = [this] { processor.cancelCapture(); };
    addChildComponent (cancelButton);

    statusLabel.setFont (juce::FontOptions (13.0f));
    statusLabel.setColour (juce::Label::textColourId, muted);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    auto options = juce::WebBrowserComponent::Options{}
                       .withNativeIntegrationEnabled()
                       .withKeepPageLoadedWhenBrowserIsHidden()
                       .withEventListener ("loopsmithHello", [this] (const juce::var& d) { onHello (d); })
                       .withEventListener ("loopsmithDrag", [this] (const juce::var& d) { onDragRequest (d); })
                       .withEventListener ("loopsmithDownload", [this] (const juce::var& d) { onDownloadRequest (d["url"].toString()); });
    browser = std::make_unique<StudioBrowser> (options);
    browser->allowedOrigin = [this] { return studioOrigin(); };
    browser->onDownloadRequested = [this] (const juce::String& url) { onDownloadRequest (url); };
    addChildComponent (*browser);
    addAndMakeVisible (welcome);
    addAndMakeVisible (dragStrip);

    setResizable (true, true);
    setResizeLimits (820, 560, 2400, 1600);
    setSize (1180, 800);

    if (processor.getStudioUrl().isNotEmpty())
        openStudio();
    startTimerHz (8);
}

LoopsmithEditor::~LoopsmithEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::String LoopsmithEditor::studioOrigin() const
{
    return normaliseStudioUrl (processor.getStudioUrl());
}

void LoopsmithEditor::paint (juce::Graphics& g)
{
    g.fillAll (body);
    g.setColour (line);
    g.drawHorizontalLine (52, 0.0f, static_cast<float> (getWidth()));
}

void LoopsmithEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (52).reduced (12, 9);
    logo.setBounds (top.removeFromLeft (110));
    auto right = top.removeFromRight (juce::jmin (430, top.getWidth() / 2));
    cancelButton.setBounds (right.removeFromRight (cancelButton.isVisible() ? 70 : 0));
    if (cancelButton.isVisible())
        right.removeFromRight (6);
    captureButton.setBounds (right.removeFromRight (230));
    right.removeFromRight (8);
    statusLabel.setBounds (right);
    openButton.setBounds (top.removeFromRight (120));
    top.removeFromRight (8);
    urlBox.setBounds (top);

    dragStrip.setBounds (r.removeFromBottom (62));
    browser->setBounds (r);
    welcome.setBounds (r);
}

void LoopsmithEditor::openStudio()
{
    const auto url = normaliseStudioUrl (urlBox.getText());
    if (url.isEmpty())
    {
        urlBox.grabKeyboardFocus();
        return;
    }
    urlBox.setText (url, false);
    processor.setStudioUrl (url);
    client.setConnection (url, {});
    welcome.setVisible (false);
    browser->setVisible (true);
    browser->goToURL (url + "/");
    statusLabel.setText ("Opening your studio...", juce::dontSendNotification);
}

void LoopsmithEditor::onHello (const juce::var& data)
{
    const auto base = normaliseStudioUrl (data["base"].toString());
    if (base != studioOrigin())
        return;  // only trust the studio we opened
    client.setConnection (base, data["token"].toString());
    statusLabel.setText ("Connected", juce::dontSendNotification);
}

void LoopsmithEditor::onDragRequest (const juce::var& data)
{
    if (! client.isReady())
        return;
    StudioClient::Item item;
    item.id = data["id"].toString();
    item.title = data["title"].toString();
    item.kind = data["kind"].toString();
    item.path = data["path"].toString();
    item.filename = data["filename"].toString();
    if (item.id.isEmpty())
        return;

    dragStrip.setFetching (item.title, item.kind);
    juce::Component::SafePointer<LoopsmithEditor> safe (this);
    client.fetchForDrag (item, [safe, item] (juce::File file, juce::String error)
    {
        if (safe == nullptr)
            return;
        if (file.existsAsFile())
            safe->dragStrip.setReady (file, item.title, item.kind);
        else
            safe->dragStrip.setError (error.isNotEmpty() ? error : "Unknown problem.");
    });
}

void LoopsmithEditor::onDownloadRequest (const juce::String& url)
{
    if (! client.isReady() || url.isEmpty())
        return;
    toastInPage ("Downloading to your Downloads folder...");
    juce::Component::SafePointer<LoopsmithEditor> safe (this);
    client.downloadToDownloads (url, [safe] (juce::File file, juce::String error)
    {
        if (safe == nullptr)
            return;
        if (file.existsAsFile())
        {
            safe->toastInPage ("Saved " + file.getFileName() + " to Downloads.");
            file.revealToUser();
        }
        else
        {
            safe->toastInPage (error, true);
        }
    });
}

void LoopsmithEditor::toastInPage (const juce::String& message, bool bad)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("message", message);
    obj->setProperty ("bad", bad);
    browser->emitEventIfBrowserIsVisible ("loopsmithToast", juce::var (obj));
}

void LoopsmithEditor::onCaptureClicked()
{
    using S = LoopsmithProcessor::CaptureState;
    switch (processor.getCaptureState())
    {
        case S::idle:
            if (! client.isReady())
            {
                statusLabel.setText ("Open your studio and log in first.", juce::dontSendNotification);
                return;
            }
            processor.armCapture();
            break;
        case S::armed:
        case S::recording:
            processor.stopCapture();
            break;
        case S::finished:
            break;
    }
}

void LoopsmithEditor::sendCapture()
{
    if (sending)
        return;
    sending = true;
    juce::String error;
    const auto stamp = juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H.%M.%S");
    captureFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                      .getChildFile ("Loopsmith capture " + stamp + ".wav");
    if (! processor.writeFinishedCapture (captureFile, error))
    {
        sending = false;
        statusLabel.setText (error, juce::dontSendNotification);
        return;
    }
    statusLabel.setText ("Sending to your studio...", juce::dontSendNotification);
    juce::Component::SafePointer<LoopsmithEditor> safe (this);
    client.upload (captureFile, [safe] (juce::String id, juce::String title, juce::String err)
    {
        if (safe == nullptr)
            return;
        safe->sending = false;
        safe->captureFile.deleteFile();
        if (id.isEmpty())
        {
            safe->statusLabel.setText (err, juce::dontSendNotification);
            return;
        }
        safe->statusLabel.setText ("Sent \"" + title + "\"", juce::dontSendNotification);
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("id", id);
        safe->browser->emitEventIfBrowserIsVisible ("loopsmithUploaded", juce::var (obj));
    });
}

void LoopsmithEditor::timerCallback()
{
    using S = LoopsmithProcessor::CaptureState;
    const auto state = processor.getCaptureState();
    juce::String label;
    switch (state)
    {
        case S::idle:      label = sending ? "Sending..." : "Capture this track"; break;
        case S::armed:     label = "Waiting for play (stop)"; break;
        case S::recording: label = "Recording " + formatSeconds (processor.getCapturedSeconds()) + "  (stop & send)"; break;
        case S::finished:  label = "Sending..."; break;
    }
    if (captureButton.getButtonText() != label)
    {
        captureButton.setButtonText (label);
        captureButton.setColour (juce::TextButton::buttonColourId,
                                 state == S::recording ? bad : (state == S::armed ? stem.darker (0.5f) : panel));
    }
    const bool showCancel = state == S::armed || state == S::recording;
    if (cancelButton.isVisible() != showCancel)
    {
        cancelButton.setVisible (showCancel);
        resized();
    }
    if (state == S::armed && ! statusLabel.getText().startsWith ("Press play"))
        statusLabel.setText (processor.hostReportsTransport() ? "Press play in your DAW to start capturing."
                                                              : "Press play in your DAW to start capturing.",
                             juce::dontSendNotification);
    if (state == S::recording && ! statusLabel.getText().startsWith ("Stop the DAW"))
        statusLabel.setText ("Stop the DAW (or click the button) to send.", juce::dontSendNotification);
    if (state == S::finished)
        sendCapture();
}
