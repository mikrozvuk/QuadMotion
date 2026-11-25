#pragma once

#include "PluginProcessor.h"

//==============================================================================
// Pastel blue palette
//==============================================================================
namespace Pastel
{
    inline const juce::Colour bgTop    { 0xffeaf5ff };
    inline const juce::Colour bgBottom { 0xffc9e1f6 };
    inline const juce::Colour panel    { 0xfff6fbff };
    inline const juce::Colour line     { 0xffa9cbe8 };
    inline const juce::Colour track    { 0xffc3dcf2 };
    inline const juce::Colour accent   { 0xff7db4e8 };
    inline const juce::Colour accentDk { 0xff4f8ccc };
    inline const juce::Colour text     { 0xff3a5672 };
}

//==============================================================================
class PastelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PastelLookAndFeel()
    {
        setColour (juce::Label::textColourId,                 Pastel::text);
        setColour (juce::Slider::textBoxTextColourId,         Pastel::text);
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId,        Pastel::panel);
        setColour (juce::ComboBox::outlineColourId,           Pastel::line);
        setColour (juce::ComboBox::textColourId,              Pastel::text);
        setColour (juce::ComboBox::arrowColourId,             Pastel::accentDk);
        setColour (juce::PopupMenu::backgroundColourId,       Pastel::panel);
        setColour (juce::PopupMenu::textColourId,             Pastel::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Pastel::accent);
        setColour (juce::PopupMenu::highlightedTextColourId,  juce::Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto  centre = bounds.getCentre();
        const float angle  = startAngle + pos * (endAngle - startAngle);

        // knob face
        const float faceR = radius - 6.0f;
        g.setColour (Pastel::panel);
        g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
        g.setColour (Pastel::line);
        g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.5f);

        // track + value arc
        const float arcR = radius - 1.5f;
        const juce::PathStrokeType stroke (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Pastel::track);
        g.strokePath (trackPath, stroke);

        juce::Path valuePath;
        valuePath.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (Pastel::accentDk);
        g.strokePath (valuePath, stroke);

        // pointer
        const float s = std::sin (angle), c = std::cos (angle);
        const float inner = faceR * 0.25f, outer = faceR * 0.85f;
        g.setColour (Pastel::accentDk);
        g.drawLine (centre.x + inner * s, centre.y - inner * c,
                    centre.x + outer * s, centre.y - outer * c, 3.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();

        g.setColour (on ? Pastel::accent : Pastel::panel.withAlpha (highlighted ? 1.0f : 0.8f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? Pastel::accentDk : Pastel::line);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.5f);

        g.setColour (on ? juce::Colours::white : Pastel::text);
        g.setFont (14.0f);
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
    }
};

//==============================================================================
// Top-down view of the four speakers. Drag to set the centre position (X / Y).
// The filled dot is where the sound actually is right now; the ring is the centre.
//==============================================================================
class XYPad : public juce::Component, private juce::Timer
{
public:
    explicit XYPad (QuadMotionAudioProcessor& p)
        : proc (p),
          paramX (p.apvts.getParameter ("x")),
          paramY (p.apvts.getParameter ("y"))
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto room = getRoom();
        const auto c    = room.getCentre();

        auto toScreen = [room] (float x, float y)
        {
            return juce::Point<float> (room.getX() + (x * 0.5f + 0.5f) * room.getWidth(),
                                       room.getY() + (1.0f - (y * 0.5f + 0.5f)) * room.getHeight());
        };

        // room
        g.setGradientFill (juce::ColourGradient (Pastel::panel, room.getTopLeft(),
                                                 Pastel::track.brighter (0.2f), room.getBottomRight(), false));
        g.fillRoundedRectangle (room, 18.0f);
        g.setColour (Pastel::line);
        g.drawRoundedRectangle (room, 18.0f, 2.0f);

        // guides
        g.setColour (Pastel::line.withAlpha (0.6f));
        g.drawLine (c.x, room.getY(), c.x, room.getBottom(), 1.0f);
        g.drawLine (room.getX(), c.y, room.getRight(), c.y, 1.0f);
        for (float f : { 0.5f, 1.0f })
        {
            const float rx = f * room.getWidth() * 0.5f, ry = f * room.getHeight() * 0.5f;
            g.drawEllipse (c.x - rx, c.y - ry, rx * 2.0f, ry * 2.0f, 1.0f);
        }

        g.setColour (Pastel::text.withAlpha (0.45f));
        g.setFont (12.0f);
        g.drawText ("FRONT", juce::Rectangle<float> (c.x - 40.0f, room.getY() + 6.0f, 80.0f, 16.0f).toNearestInt(),
                    juce::Justification::centred);
        g.drawText ("REAR", juce::Rectangle<float> (c.x - 40.0f, room.getBottom() - 22.0f, 80.0f, 16.0f).toNearestInt(),
                    juce::Justification::centred);

        // speakers (glow follows the actual gain)
        const float lx = proc.liveX.load(), ly = proc.liveY.load();
        float gains[4];
        QuadMotionAudioProcessor::speakerGains (lx, ly, gains);

        const juce::Point<float> corners[4] = { room.getTopLeft(), room.getTopRight(),
                                                room.getBottomLeft(), room.getBottomRight() };
        const char* names[4] = { "L", "R", "Ls", "Rs" };

        for (int i = 0; i < 4; ++i)
        {
            const auto  sp   = corners[i];
            const float glow = gains[i] * gains[i];
            const float rad  = 16.0f + 24.0f * glow;

            g.setColour (Pastel::accent.withAlpha (0.10f + 0.40f * glow));
            g.fillEllipse (sp.x - rad, sp.y - rad, rad * 2.0f, rad * 2.0f);

            g.setColour (Pastel::panel);
            g.fillEllipse (sp.x - 14.0f, sp.y - 14.0f, 28.0f, 28.0f);
            g.setColour (Pastel::accentDk);
            g.drawEllipse (sp.x - 14.0f, sp.y - 14.0f, 28.0f, 28.0f, 2.0f);

            g.setColour (Pastel::text);
            g.setFont (13.0f);
            g.drawText (names[i], juce::Rectangle<float> (sp.x - 14.0f, sp.y - 14.0f, 28.0f, 28.0f).toNearestInt(),
                        juce::Justification::centred);
        }

        // trail
        const int numTrail = (int) trail.size();
        for (int i = 0; i < numTrail; ++i)
        {
            const auto  pt = trail[(size_t) ((trailHead + i) % numTrail)];
            const float t  = (float) (i + 1) / (float) numTrail;
            const auto  sp = toScreen (pt.x, pt.y);
            const float r  = 2.0f + 4.0f * t;

            g.setColour (Pastel::accentDk.withAlpha (0.45f * t * t));
            g.fillEllipse (sp.x - r, sp.y - r, r * 2.0f, r * 2.0f);
        }

        // centre position (draggable)
        const auto cp = toScreen (paramX->convertFrom0to1 (paramX->getValue()),
                                  paramY->convertFrom0to1 (paramY->getValue()));
        g.setColour (Pastel::accentDk);
        g.drawEllipse (cp.x - 10.0f, cp.y - 10.0f, 20.0f, 20.0f, 2.0f);

        // live position
        const auto lp = toScreen (lx, ly);
        g.setColour (Pastel::accentDk);
        g.fillEllipse (lp.x - 9.0f, lp.y - 9.0f, 18.0f, 18.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (lp.x - 9.0f, lp.y - 9.0f, 18.0f, 18.0f, 2.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        paramX->beginChangeGesture();
        paramY->beginChangeGesture();
        setFromMouse (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override { setFromMouse (e); }

    void mouseUp (const juce::MouseEvent&) override
    {
        paramX->endChangeGesture();
        paramY->endChangeGesture();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        for (auto* prm : { paramX, paramY })
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (0.5f);
            prm->endChangeGesture();
        }
    }

private:
    juce::Rectangle<float> getRoom() const
    {
        const auto b = getLocalBounds().toFloat().reduced (36.0f);
        const float s = juce::jmin (b.getWidth(), b.getHeight());
        return juce::Rectangle<float> (s, s).withCentre (b.getCentre());
    }

    void setFromMouse (const juce::MouseEvent& e)
    {
        const auto room = getRoom();
        const float nx = juce::jlimit (0.0f, 1.0f, (e.position.x - room.getX()) / room.getWidth());
        const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - room.getY()) / room.getHeight());
        paramX->setValueNotifyingHost (nx);
        paramY->setValueNotifyingHost (ny);
    }

    void timerCallback() override
    {
        trail[(size_t) trailHead] = juce::Point<float> (proc.liveX.load(), proc.liveY.load());
        trailHead = (trailHead + 1) % (int) trail.size();
        repaint();
    }

    QuadMotionAudioProcessor& proc;
    juce::RangedAudioParameter* paramX;
    juce::RangedAudioParameter* paramY;
    std::array<juce::Point<float>, 48> trail {};
    int trailHead = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XYPad)
};

//==============================================================================
struct Knob
{
    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
    juce::Label  label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class QuadMotionAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QuadMotionAudioProcessorEditor (QuadMotionAudioProcessor&);
    ~QuadMotionAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override { updateRateControls(); }
    void updateRateControls();
    void setupKnob (Knob&, const juce::String& text, const juce::String& paramID,
                    const juce::String& suffix, int decimals);

    QuadMotionAudioProcessor& proc;
    PastelLookAndFeel laf;
    XYPad pad;

    juce::Label motionLabel;
    juce::ComboBox motionBox, divisionBox;
    juce::ToggleButton syncButton { "Tempo Sync" }, reverseButton { "Reverse" };
    Knob rate, size, phase, smooth, width, seed, gain;

    std::unique_ptr<ComboAttachment>  motionAtt, divisionAtt;
    std::unique_ptr<ButtonAttachment> syncAtt, reverseAtt;

    juce::Rectangle<int> hintArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadMotionAudioProcessorEditor)
};
