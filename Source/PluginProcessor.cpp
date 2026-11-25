#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kChunk = 32;   // control-rate block (samples) for motion and matrix ramps

    // Deterministic noise in [-1, 1]. The random walk is therefore identical on every
    // playback and every bounce.
    float hashToSigned (juce::uint32 n) noexcept
    {
        n ^= n >> 16;  n *= 0x7feb352dU;
        n ^= n >> 15;  n *= 0x846ca68bU;
        n ^= n >> 16;
        return (float) (n & 0xFFFFFFu) / 8388607.5f - 1.0f;
    }

    // Triangle wave in [-1, 1], p in cycles, starts at 0 and rises.
    float triangle (double p) noexcept
    {
        const double q = p + 0.25;
        return (float) (4.0 * std::abs (q - std::floor (q + 0.5)) - 1.0);
    }

    // Motion shapes return a point in the unit square around the centre position.
    void motionShape (int type, double p, int seed, float& sx, float& sy) noexcept
    {
        constexpr double twoPi = juce::MathConstants<double>::twoPi;
        sx = 0.0f;
        sy = 0.0f;

        switch (type)
        {
            case 1: // Circle - clockwise, starts at the front
                sx = (float) std::sin (twoPi * p);
                sy = (float) std::cos (twoPi * p);
                break;

            case 2: // Figure 8
                sx = (float) std::sin (twoPi * p);
                sy = (float) std::sin (2.0 * twoPi * p);
                break;

            case 3: // Random walk - one new random point per cycle, cosine-interpolated
            {
                const double fl = std::floor (p);
                const float  f  = (float) (p - fl);
                const float  t  = 0.5f - 0.5f * std::cos (f * juce::MathConstants<float>::pi);
                const auto   i  = (juce::uint32) (juce::int64) fl;
                const juce::uint32 salt = (juce::uint32) seed * 7919u;

                auto point = [salt] (juce::uint32 idx, juce::uint32 axis)
                {
                    return hashToSigned (idx * 2654435761u + axis * 40503u + salt);
                };

                sx = juce::jmap (t, point (i, 1u), point (i + 1u, 1u));
                sy = juce::jmap (t, point (i, 2u), point (i + 1u, 2u));
                break;
            }

            case 4: // Bounce - straight lines ricocheting off the walls, never quite repeating
                sx = triangle (p);
                sy = triangle (p * 1.6180339887);
                break;

            default: // Manual
                break;
        }
    }

    // Every input channel is treated as a source sitting in its own speaker corner.
    // Width scales how far the sources sit from the centre (0 = all collapsed to one
    // point = classic mono panner, 1 = the full quad image). The position (x, y) then
    // shifts all of them, and each one is panned with a constant-power law.
    void buildMatrix (float x, float y, float width, float gain, float* m) noexcept
    {
        static constexpr float cornerX[4] = { -1.0f,  1.0f, -1.0f,  1.0f };   // L R Ls Rs
        static constexpr float cornerY[4] = {  1.0f,  1.0f, -1.0f, -1.0f };

        const float comp = (0.5f + 0.5f * width) * gain;   // keeps level similar between widths

        for (int in = 0; in < 4; ++in)
        {
            const float px = juce::jlimit (-1.0f, 1.0f, width * cornerX[in] + x);
            const float py = juce::jlimit (-1.0f, 1.0f, width * cornerY[in] + y);

            float g[4];
            QuadMotionAudioProcessor::speakerGains (px, py, g);

            for (int out = 0; out < 4; ++out)
                m[out * 4 + in] = g[out] * comp;
        }
    }
}

//==============================================================================
QuadMotionAudioProcessor::QuadMotionAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::quadraphonic(), true)
                        .withOutput ("Output", juce::AudioChannelSet::quadraphonic(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    pX        = apvts.getRawParameterValue ("x");
    pY        = apvts.getRawParameterValue ("y");
    pMotion   = apvts.getRawParameterValue ("motion");
    pSize     = apvts.getRawParameterValue ("size");
    pRate     = apvts.getRawParameterValue ("rate");
    pSync     = apvts.getRawParameterValue ("sync");
    pDivision = apvts.getRawParameterValue ("division");
    pReverse  = apvts.getRawParameterValue ("reverse");
    pPhase    = apvts.getRawParameterValue ("phase");
    pSmooth   = apvts.getRawParameterValue ("smooth");
    pWidth    = apvts.getRawParameterValue ("width");
    pSeed     = apvts.getRawParameterValue ("seed");
    pGain     = apvts.getRawParameterValue ("gain");
}

juce::AudioProcessorValueTreeState::ParameterLayout QuadMotionAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "x", 1 }, "Position X",
                    NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "y", 1 }, "Position Y",
                    NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "motion", 1 }, "Motion",
                    motionNames(), 1));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "size", 1 }, "Size",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "rate", 1 }, "Rate",
                    NormalisableRange<float> (0.02f, 20.0f, 0.001f, 0.35f), 0.25f));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Tempo Sync", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Division",
                    divisionNames(), 2));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "reverse", 1 }, "Reverse", false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "phase", 1 }, "Phase",
                    NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "smooth", 1 }, "Smooth",
                    NormalisableRange<float> (0.0f, 500.0f, 1.0f, 0.5f), 50.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width",
                    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "seed", 1 }, "Random Seed", 0, 999, 1));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Output",
                    NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    return { p.begin(), p.end() };
}

//==============================================================================
void QuadMotionAudioProcessor::speakerGains (float x, float y, float* g) noexcept
{
    const float px = (juce::jlimit (-1.0f, 1.0f, x) * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    const float py = (juce::jlimit (-1.0f, 1.0f, y) * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;

    const float left  = std::cos (px), right = std::sin (px);
    const float rear  = std::cos (py), front = std::sin (py);

    g[0] = left  * front;   // L
    g[1] = right * front;   // R
    g[2] = left  * rear;    // Ls
    g[3] = right * rear;    // Rs
}

bool QuadMotionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::quadraphonic()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::quadraphonic();
}

void QuadMotionAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    firstBlock = true;
}

//==============================================================================
void QuadMotionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 4 || numSamples == 0)
        return;

    float* ch[4] = { buffer.getWritePointer (0), buffer.getWritePointer (1),
                     buffer.getWritePointer (2), buffer.getWritePointer (3) };

    // ---- parameters --------------------------------------------------------
    const float baseX    = pX->load();
    const float baseY    = pY->load();
    const int   motion   = juce::roundToInt (pMotion->load());
    const float size     = pSize->load();
    const float rateHz   = pRate->load();
    const bool  sync     = pSync->load() > 0.5f;
    const int   division = juce::roundToInt (pDivision->load());
    const bool  reverse  = pReverse->load() > 0.5f;
    const float phaseDeg = pPhase->load();
    const float smoothMs = pSmooth->load();
    const float width    = pWidth->load();
    const int   seed     = juce::roundToInt (pSeed->load());
    const float outGain  = juce::Decibels::decibelsToGain (pGain->load());

    // ---- host transport ----------------------------------------------------
    double bpm = 120.0, ppqStart = 0.0;
    bool playing = false, havePpq = false;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())          bpm = *b;
            if (auto q = pos->getPpqPosition())  { ppqStart = *q; havePpq = true; }
            playing = pos->getIsPlaying();
        }

    if (bpm < 1.0)
        bpm = 120.0;

    const double beatsPerSec   = bpm / 60.0;
    const double bpc           = beatsPerCycle (division);
    const double cyclesPerSec  = sync ? beatsPerSec / bpc : (double) rateHz;
    const double sr            = sampleRateHz;

    // ---- process in small chunks ------------------------------------------
    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int n = juce::jmin (kChunk, numSamples - start);

        // 1) motion clock: locked to the project position while playing in sync mode,
        //    free-running otherwise (so it keeps moving when the transport is stopped)
        if (sync && playing && havePpq)
            phase = (ppqStart + (double) (start + n) / sr * beatsPerSec) / bpc;
        else
            phase += (double) n * cyclesPerSec / sr;

        // 2) target position = centre (X/Y) + size * shape
        const double p = (reverse ? -phase : phase) + (double) phaseDeg / 360.0;
        float sx = 0.0f, sy = 0.0f;
        motionShape (motion, p, seed, sx, sy);

        const float targetX = juce::jlimit (-1.0f, 1.0f, baseX + size * sx);
        const float targetY = juce::jlimit (-1.0f, 1.0f, baseY + size * sy);

        // 3) glide
        if (firstBlock)
        {
            curX = targetX;
            curY = targetY;
        }
        else
        {
            const float a = smoothMs < 0.5f ? 1.0f
                                            : 1.0f - std::exp (-(float) n / (float) (sr * 0.001 * smoothMs));
            curX += (targetX - curX) * a;
            curY += (targetY - curY) * a;
        }

        // 4) 4x4 matrix, ramped linearly across the chunk (no zipper noise)
        std::array<float, 16> target {}, delta {};
        buildMatrix (curX, curY, width, outGain, target.data());

        if (firstBlock)
        {
            mat = target;
            firstBlock = false;
        }

        for (size_t j = 0; j < 16; ++j)
            delta[j] = (target[j] - mat[j]) / (float) n;

        for (int s = 0; s < n; ++s)
        {
            const int idx = start + s;
            const float in0 = ch[0][idx], in1 = ch[1][idx], in2 = ch[2][idx], in3 = ch[3][idx];

            for (size_t j = 0; j < 16; ++j)
                mat[j] += delta[j];

            for (int out = 0; out < 4; ++out)
            {
                const float* m = &mat[(size_t) out * 4];
                ch[out][idx] = m[0] * in0 + m[1] * in1 + m[2] * in2 + m[3] * in3;
            }
        }

        mat = target;
    }

    liveX.store (curX);
    liveY.store (curY);
}

//==============================================================================
juce::AudioProcessorEditor* QuadMotionAudioProcessor::createEditor()
{
    return new QuadMotionAudioProcessorEditor (*this);
}

void QuadMotionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void QuadMotionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuadMotionAudioProcessor();
}
