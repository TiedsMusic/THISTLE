#include "PluginEditor.h"

namespace
{
    const juce::Colour kPaper   (0xfff6eee5);
    const juce::Colour kInk     (0xff3c2531);
    const juce::Colour kCrimson (0xffb31e07);

    constexpr float kPx = 450.0f / 148.0f;      // pixels par mm du gabarit A5

    const char* const kIds[8]   = { "thump", "body", "click", "tight", "mud", "sub", "squash", "mix" };
    const char* const kNames[8] = { "THUMP", "BODY", "CLICK", "TIGHT", "MUD", "SUB", "SQUASH", "MIX" };
}

//==============================================================================
ThistleEditor::ThistleEditor (ThistleProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (450, 639);

    for (int i = 0; i < 8; ++i)
    {
        auto* k = &knobs[(size_t) i];

        k->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                       juce::MathConstants<float>::pi * 2.75f, true);
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, kCrimson);
        k->slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xffd9c7b0));
        k->slider.setColour (juce::Slider::thumbColourId, kInk);
        addAndMakeVisible (k->slider);

        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            proc.apvts, kIds[i], k->slider);

        k->name.setText (kNames[i], juce::dontSendNotification);
        k->name.setJustificationType (juce::Justification::centred);
        k->name.setColour (juce::Label::textColourId, kInk);
        k->name.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (k->name);

        k->value.setJustificationType (juce::Justification::centred);
        k->value.setColour (juce::Label::textColourId, kInk);
        k->value.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (k->value);

        k->slider.onValueChange = [k]
        {
            k->value.setText (k->slider.getTextFromValue (k->slider.getValue()), juce::dontSendNotification);
        };
        k->slider.onValueChange();
    }

    // --- Bande sous le titre : type de guitare, gain d'entrée, niveau d'entrée
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("range")))
        rangeBox.addItemList (choice->choices, 1);

    rangeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xfffbf5ea));
    rangeBox.setColour (juce::ComboBox::textColourId, kInk);
    rangeBox.setColour (juce::ComboBox::outlineColourId, kInk);
    rangeBox.setColour (juce::ComboBox::arrowColourId, kInk);
    addAndMakeVisible (rangeBox);
    rangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "range", rangeBox);

    inputSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    inputSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    inputSlider.setColour (juce::Slider::trackColourId, kCrimson);
    inputSlider.setColour (juce::Slider::thumbColourId, kInk);
    inputSlider.setTooltip ("Input gain");
    addAndMakeVisible (inputSlider);
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, "input", inputSlider);

    // --- Presets
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xfffbf5ea));
    presetBox.setColour (juce::ComboBox::textColourId, kInk);
    presetBox.setColour (juce::ComboBox::outlineColourId, kInk);
    presetBox.setColour (juce::ComboBox::arrowColourId, kInk);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();

        if (id >= 2000)      proc.presets.loadUser (presetBox.getText());
        else if (id >= 1000) proc.presets.loadFactory (id - 1000);
    };
    addAndMakeVisible (presetBox);

    for (auto* b : { &saveBtn, &loadBtn })
    {
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xfffbf5ea));
        b->setColour (juce::TextButton::textColourOffId, kInk);
        addAndMakeVisible (*b);
    }
    saveBtn.onClick = [this] { savePreset(); };
    loadBtn.onClick = [this] { loadPreset(); };

    refreshPresets (proc.presets.currentName);
    startTimerHz (30);
}

//==============================================================================
void ThistleEditor::timerCallback()
{
    const float peak = proc.inputPeak.load();
    const float db   = 20.0f * std::log10 (peak + 1.0e-6f);
    const float norm = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);

    meterLevel = juce::jmax (norm, meterLevel * 0.85f);
    repaint (juce::Rectangle<int> ((int) (80 * kPx) - 6, (int) (34 * kPx), (int) (60 * kPx), (int) (6 * kPx)));
}

void ThistleEditor::refreshPresets (const juce::String& select)
{
    presetBox.clear (juce::dontSendNotification);

    int selectId = 0;

    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < proc.presets.getNumFactory(); ++i)
    {
        const auto name = proc.presets.getFactoryName (i);
        presetBox.addItem (name, 1000 + i);
        if (name == select) selectId = 1000 + i;
    }

    const auto userNames = proc.presets.getUserPresetNames();
    if (userNames.size() > 0)
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userNames.size(); ++i)
        {
            presetBox.addItem (userNames[i], 2000 + i);
            if (userNames[i] == select && selectId == 0) selectId = 2000 + i;
        }
    }

    if (selectId != 0)
        presetBox.setSelectedId (selectId, juce::dontSendNotification);
}

void ThistleEditor::savePreset()
{
    const auto ext = proc.presets.getExtension();
    chooser = std::make_unique<juce::FileChooser> ("Save preset",
                                                   proc.presets.getPresetsFolder().getChildFile ("My preset." + ext),
                                                   "*." + ext);

    juce::Component::SafePointer<ThistleEditor> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe, ext] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        auto file = fc.getResult();
        if (file == juce::File())
            return;

        file = file.withFileExtension (ext);

        if (safe->proc.presets.saveToFile (file))
        {
            const auto library = safe->proc.presets.getPresetsFolder();
            if (file.getParentDirectory() != library)
                file.copyFileTo (library.getChildFile (file.getFileName()));

            safe->refreshPresets (safe->proc.presets.currentName);
        }
    });
}

void ThistleEditor::loadPreset()
{
    const auto ext = proc.presets.getExtension();
    chooser = std::make_unique<juce::FileChooser> ("Load preset", proc.presets.getPresetsFolder(), "*." + ext);

    juce::Component::SafePointer<ThistleEditor> safe (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;

        const auto file = fc.getResult();
        if (file == juce::File())
            return;

        if (safe->proc.presets.loadFromFile (file, true))
            safe->refreshPresets (safe->proc.presets.currentName);
        else
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "THISTLE", "This file is not a valid THISTLE preset.");
    });
}

//==============================================================================
void ThistleEditor::paint (juce::Graphics& g)
{
    g.fillAll (kPaper);

    // repères de l'encart titre et de la bande du gabarit (à supprimer quand le fond dessiné sera en place)
    g.setColour (kInk.withAlpha (0.15f));
    g.drawRect ((int) (12 * kPx), (int) (12 * kPx), (int) (124 * kPx), (int) (20 * kPx));
    g.drawRect ((int) (12 * kPx), (int) (34 * kPx), (int) (124 * kPx), (int) (6 * kPx));

    g.setColour (kInk);
    g.setFont (32.0f);
    g.drawText ("THISTLE", (int) (14 * kPx), (int) (13 * kPx), (int) (64 * kPx), (int) (11 * kPx),
                juce::Justification::centredLeft);

    g.setFont (13.0f);
    g.drawText ("Thump shaper", (int) (14 * kPx), (int) (23 * kPx), (int) (64 * kPx), (int) (8 * kPx),
                juce::Justification::centredLeft);

    // indicateur de niveau d'entrée (vert, puis orange, puis rouge près de la saturation)
    const auto meter = juce::Rectangle<float> (112 * kPx, 35.5f * kPx, 22 * kPx, 3.0f * kPx);
    g.setColour (kInk.withAlpha (0.12f));
    g.fillRect (meter);
    const float w = meter.getWidth() * meterLevel;
    g.setColour (meterLevel > 0.97f ? juce::Colours::red
                 : meterLevel > 0.85f ? juce::Colours::orange
                                      : juce::Colour (0xff4f8058));
    g.fillRect (meter.withWidth (w));

    g.setColour (kInk);
    g.setFont (11.0f);
    g.drawText ("IN", (int) (79 * kPx), (int) (34 * kPx), (int) (6 * kPx), (int) (6 * kPx),
                juce::Justification::centredRight);
}

void ThistleEditor::resized()
{
    // encart du titre : presets à droite
    presetBox.setBounds ((int) (80 * kPx), (int) (14.5f * kPx), (int) (54 * kPx), 24);
    saveBtn.setBounds   ((int) (80 * kPx), (int) (14.5f * kPx) + 28, (int) (26 * kPx), 22);
    loadBtn.setBounds   ((int) (108 * kPx), (int) (14.5f * kPx) + 28, (int) (26 * kPx), 22);

    // bande sous le titre : type de guitare, gain d'entrée, niveau
    rangeBox.setBounds ((int) (12 * kPx), (int) (34 * kPx) - 1, (int) (52 * kPx), 20);
    inputSlider.setBounds ((int) (86 * kPx), (int) (34 * kPx) - 1, (int) (25 * kPx), 20);

    for (int i = 0; i < 8; ++i)
    {
        auto& k = knobs[(size_t) i];
        const float cx = (i % 2 == 0 ? 43.0f : 105.0f) * kPx;
        const float cy = (58.0f + (float) (i / 2) * 36.5f) * kPx;

        k.slider.setBounds (juce::Rectangle<int> (64, 64).withCentre ({ (int) cx, (int) cy }));
        k.name.setBounds ((int) (cx - 22.0f * kPx), (int) (cy + 12.0f * kPx), (int) (44.0f * kPx), 18);
        k.value.setBounds ((int) (cx - 13.0f * kPx), (int) (cy + 18.5f * kPx), (int) (26.0f * kPx), 14);
    }
}
