#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>

//==============================================================================
// QuadMotion - quadraphonic (4.0 / "Quadro") sound mover.
// Channel order follows Cubase Quadro: 0 = L, 1 = R, 2 = Ls, 3 = Rs.
//==============================================================================
class QuadMotionAudioProcessor : public juce::AudioProcessor
{
public:
    QuadMotionAudioProcessor();
    ~QuadMotionAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "QuadMotion"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // x: -1 (left) .. +1 (right),  y: -1 (rear) .. +1 (front).
    // Writes constant-power gains for L, R, Ls, Rs (sum of squares == 1).
    static void speakerGains (float x, float y, float* gains4) noexcept;

    static juce::StringArray motionNames()   { return { "Manual", "Circle", "Figure 8", "Random Walk", "Bounce" }; }
    static juce::StringArray divisionNames() { return { "4 Bars", "2 Bars", "1 Bar", "1/2", "1/4", "1/8", "1/16" }; }
    static double beatsPerCycle (int index) noexcept
    {
        static constexpr double beats[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25 };   // assumes 4/4
        return beats[juce::jlimit (0, 6, index)];
    }

    juce::AudioProcessorValueTreeState apvts;

    // Actual (smoothed) position, published for the editor display.
    std::atomic<float> liveX { 0.0f }, liveY { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::atomic<float> *pX = nullptr, *pY = nullptr, *pMotion = nullptr, *pSize = nullptr,
                       *pRate = nullptr, *pSync = nullptr, *pDivision = nullptr, *pReverse = nullptr,
                       *pPhase = nullptr, *pSmooth = nullptr, *pWidth = nullptr, *pSeed = nullptr,
                       *pGain = nullptr;

    double sampleRateHz = 44100.0;
    double phase = 0.0;                 // motion clock, in cycles (unwrapped)
    float  curX = 0.0f, curY = 0.0f;    // smoothed position
    bool   firstBlock = true;
    std::array<float, 16> mat {};       // 4x4 mixing matrix [out * 4 + in]

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadMotionAudioProcessor)
};
