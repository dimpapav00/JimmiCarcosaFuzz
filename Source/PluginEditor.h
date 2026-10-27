#pragma once
#include "PluginProcessor.h"

class CarcosaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CarcosaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class CarcosaEditor : public juce::AudioProcessorEditor
{
public:
    explicit CarcosaEditor (CarcosaAudioProcessor&);
    ~CarcosaEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAtt> att;
    };

    void addKnob (Knob&, const juce::String& id, const juce::String& text);
    void addTrim (juce::Label&, juce::Slider&, std::unique_ptr<SliderAtt>&, const char* id, const char* text);

    CarcosaAudioProcessor& proc;
    CarcosaLookAndFeel lnf;

    Knob before, after, output, hicut, octave, bass, mid, treble;
    juce::ToggleButton footSwitch { "ON" };
    juce::ComboBox modeBox, octModeBox;
    juce::Label modeLabel, octModeLabel, inLabel, trimLabel, gateLabel;
    juce::Slider inSlider, trimSlider, gateSlider;

    std::unique_ptr<ButtonAtt> footAtt;
    std::unique_ptr<ComboAtt> modeAtt, octModeAtt;
    std::unique_ptr<SliderAtt> inAtt, trimAtt, gateAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CarcosaEditor)
};
