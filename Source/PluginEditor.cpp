#include "PluginEditor.h"

QuadMotionAudioProcessorEditor::QuadMotionAudioProcessorEditor (QuadMotionAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), pad (p)
{
    setLookAndFeel (&laf);
    setSize (780, 480);

    addAndMakeVisible (pad);

    // motion shape
    motionLabel.setText ("Motion", juce::dontSendNotification);
    addAndMakeVisible (motionLabel);
    motionBox.addItemList (QuadMotionAudioProcessor::motionNames(), 1);
    addAndMakeVisible (motionBox);
    motionAtt = std::make_unique<ComboAttachment> (proc.apvts, "motion", motionBox);

    // tempo division (replaces the Rate knob while Tempo Sync is on)
    divisionBox.addItemList (QuadMotionAudioProcessor::divisionNames(), 1);
    addAndMakeVisible (divisionBox);
    divisionAtt = std::make_unique<ComboAttachment> (proc.apvts, "division", divisionBox);

    addAndMakeVisible (syncButton);
    syncAtt = std::make_unique<ButtonAttachment> (proc.apvts, "sync", syncButton);

    addAndMakeVisible (reverseButton);
    reverseAtt = std::make_unique<ButtonAttachment> (proc.apvts, "reverse", reverseButton);

    setupKnob (rate,   "Rate",   "rate",   " Hz",  2);
    setupKnob (size,   "Size",   "size",   {},     2);
    setupKnob (phase,  "Phase",  "phase",  " deg", 0);
    setupKnob (smooth, "Smooth", "smooth", " ms",  0);
    setupKnob (width,  "Width",  "width",  {},     2);
    setupKnob (seed,   "Seed",   "seed",   {},     0);
    setupKnob (gain,   "Output", "gain",   " dB",  1);

    updateRateControls();
    startTimerHz (10);   // also follows Tempo Sync changes coming from host automation
}

QuadMotionAudioProcessorEditor::~QuadMotionAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void QuadMotionAudioProcessorEditor::setupKnob (Knob& k, const juce::String& text, const juce::String& paramID,
                                                const juce::String& suffix, int decimals)
{
    addAndMakeVisible (k.slider);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setNumDecimalPlacesToDisplay (decimals);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramID, k.slider);
}

void QuadMotionAudioProcessorEditor::updateRateControls()
{
    const bool sync = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;

    rate.slider.setVisible (! sync);
    divisionBox.setVisible (sync);
    rate.label.setText (sync ? "Division" : "Rate", juce::dontSendNotification);
}

//==============================================================================
void QuadMotionAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (Pastel::bgTop, 0.0f, 0.0f,
                                             Pastel::bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (Pastel::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("QuadMotion", 24, 12, 170, 34, juce::Justification::centredLeft);

    g.setColour (Pastel::text.withAlpha (0.65f));
    g.setFont (14.0f);
    g.drawText ("quadraphonic sound mover", 196, 18, 300, 24, juce::Justification::centredLeft);

    g.setColour (Pastel::text.withAlpha (0.7f));
    g.setFont (13.0f);
    g.drawFittedText ("Drag the pad to place the sound, double-click to centre it.\n"
                      "Ring = centre position, dot = where the sound is now.\n"
                      "Channels: L / R / Ls / Rs (Cubase Quadro 4.0).",
                      hintArea, juce::Justification::topLeft, 4);
}

void QuadMotionAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16);
    area.removeFromTop (44);                                // title

    pad.setBounds (area.removeFromLeft (410));
    area.removeFromLeft (16);

    // motion selector + toggles
    auto row = area.removeFromTop (54);
    auto motionCol = row.removeFromLeft (150);
    motionLabel.setBounds (motionCol.removeFromTop (20));
    motionBox.setBounds (motionCol.reduced (0, 2));
    row.removeFromLeft (12);
    syncButton.setBounds (row.removeFromTop (24));
    row.removeFromTop (6);
    reverseButton.setBounds (row.removeFromTop (24));

    area.removeFromTop (14);

    auto layoutKnobs = [] (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
    {
        const int cellW = 80;
        for (auto* k : knobs)
        {
            auto cell = r.removeFromLeft (cellW);
            k->label.setBounds (cell.removeFromTop (18));
            k->slider.setBounds (cell);
        }
    };

    auto row1 = area.removeFromTop (118);
    layoutKnobs (row1, { &rate, &size, &phase, &smooth });
    divisionBox.setBounds (rate.slider.getBounds().withSizeKeepingCentre (74, 26));

    layoutKnobs (area.removeFromTop (118), { &width, &seed, &gain });

    area.removeFromTop (8);
    hintArea = area;
}
