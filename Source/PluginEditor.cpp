#include "PluginEditor.h"
#include <array>

namespace
{
const juce::Colour kAccent { 0xffb07ae0 };
const juce::Colour kBlack  { 0xff111111 };
const juce::Colour kPanel  { 0xff1c1a20 };
}

CarcosaLookAndFeel::CarcosaLookAndFeel()
{
    setColour (juce::Label::textColourId, kAccent);
    setColour (juce::ComboBox::backgroundColourId, kBlack);
    setColour (juce::ComboBox::textColourId, kAccent);
    setColour (juce::ComboBox::outlineColourId, kAccent.withAlpha (0.6f));
    setColour (juce::ComboBox::arrowColourId, kAccent);
    setColour (juce::PopupMenu::backgroundColourId, kBlack);
    setColour (juce::PopupMenu::textColourId, kAccent);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.3f));
    setColour (juce::Slider::trackColourId, kAccent);
    setColour (juce::Slider::thumbColourId, kAccent);
    setColour (juce::Slider::backgroundColourId, kBlack);
    setColour (juce::TextEditor::textColourId, kAccent);
    setColour (juce::TextEditor::backgroundColourId, kBlack);
}

void CarcosaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                          float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
    g.setColour (kAccent.withAlpha (0.18f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kAccent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float kr = radius - 8.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3a), centre.x, centre.y - kr,
                                             juce::Colour (0xff0b0b0b), centre.x, centre.y + kr, false));
    g.fillEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (kAccent.withAlpha (0.7f));
    g.drawEllipse (centre.x - kr, centre.y - kr, kr * 2.0f, kr * 2.0f, 1.5f);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.8f, -kr + 3.0f, 3.6f, kr * 0.55f, 1.5f);
    g.setColour (kAccent);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void CarcosaLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto area = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();

    if (b.getComponentID() == "foot")
    {
        const float d = std::min (area.getWidth() * 0.55f, area.getHeight() - 28.0f);
        const auto sw = juce::Rectangle<float> (d, d).withCentre ({ area.getCentreX(), area.getY() + 20.0f + d * 0.5f });
        const juce::Rectangle<float> led (area.getCentreX() - 5.0f, area.getY() + 2.0f, 10.0f, 10.0f);
        if (on)
        {
            g.setColour (kAccent.withAlpha (0.35f));
            g.fillEllipse (led.expanded (4.0f));
        }
        g.setColour (on ? kAccent : juce::Colour (0xff2e1d40));
        g.fillEllipse (led);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9c9c9), sw.getX(), sw.getY(),
                                                 juce::Colour (0xff6a6a6a), sw.getRight(), sw.getBottom(), false));
        g.fillEllipse (sw);
        g.setColour (highlighted ? kAccent : juce::Colour (0xff222222));
        g.drawEllipse (sw, 2.0f);
        g.setColour (kAccent);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (b.getButtonText(), b.getLocalBounds().removeFromBottom (18), juce::Justification::centred);
        return;
    }

    // small toggle: a lamp and the name
    auto r = area.reduced (2.0f);
    g.setColour (kBlack);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (highlighted ? kAccent : kAccent.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 6.0f, 1.5f);
    const juce::Rectangle<float> lamp (r.getX() + 10.0f, r.getCentreY() - 5.0f, 10.0f, 10.0f);
    if (on)
    {
        g.setColour (kAccent.withAlpha (0.35f));
        g.fillEllipse (lamp.expanded (3.0f));
    }
    g.setColour (on ? kAccent : juce::Colour (0xff2e1d40));
    g.fillEllipse (lamp);
    g.setColour (kAccent);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (28.0f).toNearestInt(), juce::Justification::centredLeft);
}

CarcosaEditor::CarcosaEditor (CarcosaAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setSize (640, 700);

    addKnob (before, "before", "BEFORE");
    addKnob (after,  "after",  "AFTER");
    addKnob (output, "output", "OUTPUT");
    addKnob (hicut,  "hicut",  "HI-CUT");
    addKnob (octave, "octave", "OCTAVE");
    addKnob (bass,   "bass",   "BASS");
    addKnob (mid,    "mid",    "MID");
    addKnob (treble, "treble", "TREBLE");

    footSwitch.setComponentID ("foot");
    addAndMakeVisible (footSwitch);
    footAtt = std::make_unique<ButtonAtt> (proc.apvts, "on", footSwitch);

    modeBox.addItemList ({ "Demhe (adds bass)", "Hali (cuts bass)" }, 1);
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<ComboAtt> (proc.apvts, "mode", modeBox);
    octModeBox.addItemList ({ "Down (sub-octave)", "Up (rectifier)" }, 1);
    addAndMakeVisible (octModeBox);
    octModeAtt = std::make_unique<ComboAtt> (proc.apvts, "octmode", octModeBox);
    octModeLabel.setText ("OCTAVE", juce::dontSendNotification);
    octModeLabel.setJustificationType (juce::Justification::centredLeft);
    octModeLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (octModeLabel);
    modeLabel.setText ("MODE", juce::dontSendNotification);
    modeLabel.setJustificationType (juce::Justification::centredLeft);
    modeLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (modeLabel);

    addTrim (inLabel,   inSlider,   inAtt,   "input", "INPUT");
    addTrim (trimLabel, trimSlider, trimAtt, "trim",  "TRIM");
    addTrim (gateLabel, gateSlider, gateAtt, "gate",  "NOISE GATE");
    gateSlider.textFromValueFunction = [] (double v) { return v <= -79.9 ? juce::String ("Off") : juce::String (v, 1) + " dB"; };
    gateSlider.updateText();
}

CarcosaEditor::~CarcosaEditor() { setLookAndFeel (nullptr); }

void CarcosaEditor::addTrim (juce::Label& l, juce::Slider& s, std::unique_ptr<SliderAtt>& att, const char* id, const char* text)
{
    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centredLeft);
    l.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (l);
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    s.setTextValueSuffix (" dB");
    addAndMakeVisible (s);
    att = std::make_unique<SliderAtt> (proc.apvts, id, s);
}

void CarcosaEditor::addKnob (Knob& k, const juce::String& id, const juce::String& text)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setDoubleClickReturnValue (true, proc.apvts.getParameter (id)->convertFrom0to1 (
                                                  proc.apvts.getParameter (id)->getDefaultValue()));
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void CarcosaEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0a0a0a));
    const auto body = getLocalBounds().toFloat().reduced (8.0f);
    g.setGradientFill (juce::ColourGradient (kPanel.brighter (0.05f), 0, 0, kBlack, 0, (float) getHeight(), false));
    g.fillRoundedRectangle (body, 14.0f);
    g.setColour (kAccent);
    g.drawRoundedRectangle (body, 14.0f, 2.5f);

    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("JIMMI CARCOSA FUZZ", getLocalBounds().removeFromTop (62).withTrimmedTop (14), juce::Justification::centred);
    g.setFont (juce::FontOptions (11.0f));
    g.setColour (kAccent.withAlpha (0.6f));
    g.drawText ("silicon fuzz with octave and 3-band tone  -  based on the DOD Carcosa, unofficial, not affiliated",
                getLocalBounds().removeFromTop (78).withTrimmedTop (60), juce::Justification::centred);
}

void CarcosaEditor::resized()
{
    auto r = getLocalBounds().reduced (24);
    auto gateRow = r.removeFromBottom (34);
    gateLabel.setBounds (gateRow.removeFromLeft (92));
    gateSlider.setBounds (gateRow);
    r.removeFromTop (74);

    for (auto knobs : { std::array<Knob*, 4> { &before, &after, &output, &hicut }, std::array<Knob*, 4> { &octave, &bass, &mid, &treble } })
    {
        auto row = r.removeFromTop (136);
        const int w = row.getWidth() / 4;
        for (auto* k : knobs)
        {
            auto cell = row.removeFromLeft (w);
            k->label.setBounds (cell.removeFromBottom (20));
            k->slider.setBounds (cell);
        }
    }

    r.removeFromTop (8);
    auto sw = r.removeFromTop (34);
    modeLabel.setBounds (sw.removeFromLeft (56));
    modeBox.setBounds (sw.removeFromLeft (190));
    sw.removeFromLeft (20);
    octModeLabel.setBounds (sw.removeFromLeft (64));
    octModeBox.setBounds (sw.removeFromLeft (170));

    r.removeFromTop (6);
    footSwitch.setBounds (r.removeFromTop (100).withSizeKeepingCentre (110, 100));

    r.removeFromTop (6);
    auto io = r.removeFromTop (30);
    inLabel.setBounds (io.removeFromLeft (56));
    inSlider.setBounds (io.removeFromLeft (io.getWidth() / 2 - 8));
    io.removeFromLeft (16);
    trimLabel.setBounds (io.removeFromLeft (50));
    trimSlider.setBounds (io);
}
