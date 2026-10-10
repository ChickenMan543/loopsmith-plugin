#include "PluginProcessor.h"
#include "PluginEditor.h"

LoopsmithProcessor::LoopsmithProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    studioUrl = settings().getValue ("studioUrl");
}

juce::PropertiesFile& LoopsmithProcessor::settings()
{
    static juce::PropertiesFile file ([] {
        juce::PropertiesFile::Options o;
        o.applicationName = "Loopsmith Plugin";
        o.filenameSuffix = ".settings";
        o.folderName = "Loopsmith";
        o.osxLibrarySubFolder = "Application Support";
        o.commonToAllUsers = false;
        return o;
    }());
    return file;
}

void LoopsmithProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate.store (sampleRate);
}

bool LoopsmithProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void LoopsmithProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto state = getCaptureState();
    if (state != CaptureState::armed && state != CaptureState::recording)
        return;

    // Is the DAW playing? If the host doesn't tell us, record whenever armed.
    bool playing = true;
    bool hasTransport = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hasTransport = true;
            playing = pos->getIsPlaying();
        }
    hostHasTransport.store (hasTransport);

    if (state == CaptureState::armed)
    {
        if (! playing)
            return;
        writePosition.store (0);
        setState (CaptureState::recording);
    }
    else if (hasTransport && wasPlaying && ! playing)
    {
        // DAW stopped: the capture is done.
        wasPlaying = false;
        setState (writePosition.load() > 0 ? CaptureState::finished : CaptureState::armed);
        return;
    }
    wasPlaying = playing;

    if (! playing)
        return;

    const int capacity = captureBuffer.getNumSamples();
    int pos = writePosition.load();
    const int n = juce::jmin (buffer.getNumSamples(), capacity - pos);
    if (n > 0)
    {
        const int inChans = juce::jmax (1, getTotalNumInputChannels());
        for (int ch = 0; ch < captureBuffer.getNumChannels(); ++ch)
            captureBuffer.copyFrom (ch, pos, buffer, juce::jmin (ch, inChans - 1), 0, n);
        pos += n;
        writePosition.store (pos);
    }
    if (pos >= capacity)
        setState (CaptureState::finished);
}

void LoopsmithProcessor::armCapture()
{
    if (getCaptureState() == CaptureState::recording)
        return;
    const auto sr = currentSampleRate.load();
    const int samples = static_cast<int> (sr * maxCaptureSeconds);
    if (captureBuffer.getNumSamples() != samples)
    {
        // Allocate before arming, so the audio thread never allocates.
        setState (CaptureState::idle);
        captureBuffer.setSize (2, samples, false, true, false);
    }
    writePosition.store (0);
    wasPlaying = false;
    setState (CaptureState::armed);
}

void LoopsmithProcessor::stopCapture()
{
    const auto s = getCaptureState();
    if (s == CaptureState::recording)
        setState (writePosition.load() > 0 ? CaptureState::finished : CaptureState::idle);
    else if (s == CaptureState::armed)
        setState (CaptureState::idle);
}

void LoopsmithProcessor::cancelCapture()
{
    setState (CaptureState::idle);
    writePosition.store (0);
}

double LoopsmithProcessor::getCapturedSeconds() const noexcept
{
    return writePosition.load() / juce::jmax (1.0, currentSampleRate.load());
}

bool LoopsmithProcessor::writeFinishedCapture (const juce::File& wavFile, juce::String& error)
{
    if (getCaptureState() != CaptureState::finished)
    {
        error = "Nothing captured yet.";
        return false;
    }
    const int length = writePosition.load();
    if (length < static_cast<int> (currentSampleRate.load() * 0.5))
    {
        setState (CaptureState::idle);
        error = "That capture was shorter than half a second, so it wasn't sent.";
        return false;
    }

    wavFile.deleteFile();
    wavFile.getParentDirectory().createDirectory();
    std::unique_ptr<juce::OutputStream> stream (wavFile.createOutputStream().release());
    if (stream == nullptr)
    {
        error = "Couldn't write the capture to disk.";
        return false;
    }
    juce::WavAudioFormat wav;
    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (currentSampleRate.load())
                             .withNumChannels (2)
                             .withBitsPerSample (24);
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream, options));
    if (writer == nullptr)
    {
        error = "Couldn't create the WAV file.";
        return false;
    }
    writer->writeFromAudioSampleBuffer (captureBuffer, 0, length);
    writer.reset();

    setState (CaptureState::idle);
    writePosition.store (0);
    captureBuffer.setSize (0, 0);   // give the memory back
    return true;
}

juce::String LoopsmithProcessor::getStudioUrl() const
{
    const juce::ScopedLock sl (urlLock);
    return studioUrl;
}

void LoopsmithProcessor::setStudioUrl (const juce::String& url)
{
    {
        const juce::ScopedLock sl (urlLock);
        studioUrl = url;
    }
    settings().setValue ("studioUrl", url);
    settings().saveIfNeeded();
}

void LoopsmithProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("LoopsmithState");
    state.setProperty ("studioUrl", getStudioUrl(), nullptr);
    juce::MemoryOutputStream out (destData, false);
    state.writeToStream (out);
}

void LoopsmithProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto state = juce::ValueTree::readFromData (data, static_cast<size_t> (sizeInBytes));
    if (state.isValid() && state.hasProperty ("studioUrl"))
    {
        const juce::ScopedLock sl (urlLock);
        studioUrl = state["studioUrl"].toString();
    }
}

juce::AudioProcessorEditor* LoopsmithProcessor::createEditor()
{
    return new LoopsmithEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LoopsmithProcessor();
}
