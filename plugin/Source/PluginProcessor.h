#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>

/*  Loopsmith plugin processor.

    Audio passes straight through untouched. The only audio work it does is "capture":
    record what's playing on the track (while the DAW plays) so it can be sent to the studio
    for stems, MIDI, chopping or remixing.
*/
class LoopsmithProcessor : public juce::AudioProcessor
{
public:
    enum class CaptureState { idle, armed, recording, finished };

    static constexpr double maxCaptureSeconds = 6.0 * 60.0;

    LoopsmithProcessor();
    ~LoopsmithProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Studio link (saved with the project, and remembered for new instances)
    juce::String getStudioUrl() const;
    void setStudioUrl (const juce::String& url);

    // Capture (call from the message thread)
    void armCapture();
    void stopCapture();      // finish now (manual stop)
    void cancelCapture();
    CaptureState getCaptureState() const noexcept   { return static_cast<CaptureState> (captureState.load()); }
    double getCapturedSeconds() const noexcept;
    bool hostReportsTransport() const noexcept      { return hostHasTransport.load(); }
    /** Writes the finished capture to a WAV file and resets to idle. Returns false if nothing was recorded. */
    bool writeFinishedCapture (const juce::File& wavFile, juce::String& error);

    static juce::PropertiesFile& settings();

private:
    std::atomic<int> captureState { static_cast<int> (CaptureState::idle) };
    std::atomic<int> writePosition { 0 };
    std::atomic<bool> hostHasTransport { false };
    std::atomic<double> currentSampleRate { 44100.0 };
    juce::AudioBuffer<float> captureBuffer;
    bool wasPlaying = false;

    juce::String studioUrl;
    mutable juce::CriticalSection urlLock;

    void setState (CaptureState s) noexcept { captureState.store (static_cast<int> (s)); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopsmithProcessor)
};
