#pragma once

#include "PluginProcessor.h"
#include <array>
#include <memory>

// Interface fonctionnelle au format A5 portrait (450 x 639 px), calée sur le gabarit :
// titre + presets en haut, bande sous le titre (type de guitare, gain d'entrée, niveau),
// puis 8 potards en 2 colonnes de 4. Les dessins remplaceront le fond et les potards plus tard.
class ThistleEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit ThistleEditor (ThistleProcessor&);
    ~ThistleEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label name, value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void refreshPresets (const juce::String& select);
    void savePreset();
    void loadPreset();

    ThistleProcessor& proc;
    std::array<Knob, 8> knobs;

    juce::ComboBox presetBox, rangeBox;
    juce::TextButton saveBtn { "Save" }, loadBtn { "Load" };
    juce::Slider inputSlider;
    std::unique_ptr<juce::FileChooser> chooser;

    float meterLevel = 0.0f;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rangeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThistleEditor)
};
