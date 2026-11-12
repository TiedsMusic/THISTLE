#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PresetManager.h"
#include <atomic>
#include <cmath>

// THISTLE : mise en forme du "thump" (technique percussive au pouce) pour guitares 6, 7 et 8 cordes.
// Aucune latence. Les détections se font sur des rapports d'enveloppes : le traitement ne dépend
// pas du niveau absolu du signal (micros passifs ou actifs).
class ThistleProcessor : public juce::AudioProcessor
{
public:
    ThistleProcessor();
    ~ThistleProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "THISTLE"; }
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

    std::atomic<float> inputPeak { 0.0f };       // pour l'indicateur de niveau d'entrée

    juce::AudioProcessorValueTreeState apvts;    // doit être déclaré avant 'presets'
    PresetManager presets;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<FactoryPreset> makeFactoryPresets();

    // suiveur d'enveloppe (attaque / relâchement)
    struct Follower
    {
        float env = 0.0f, att = 0.0f, rel = 0.0f;

        void set (float attMs, float relMs, double sr)
        {
            att = std::exp (-1.0f / (0.001f * attMs * (float) sr));
            rel = std::exp (-1.0f / (0.001f * relMs * (float) sr));
        }
        void reset() { env = 0.0f; }
        float process (float x)
        {
            const float c = x > env ? att : rel;
            env = c * env + (1.0f - c) * x;
            return env;
        }
    };

    using SVF = juce::dsp::StateVariableTPTFilter<float>;

    std::atomic<float>* thumpP  = nullptr;
    std::atomic<float>* bodyP   = nullptr;
    std::atomic<float>* clickP  = nullptr;
    std::atomic<float>* tightP  = nullptr;
    std::atomic<float>* mudP    = nullptr;
    std::atomic<float>* subP    = nullptr;
    std::atomic<float>* squashP = nullptr;
    std::atomic<float>* mixP    = nullptr;
    std::atomic<float>* rangeP  = nullptr;
    std::atomic<float>* inputP  = nullptr;

    juce::SmoothedValue<float> thumpS, bodyS, clickS, tightS, mudS, subS, squashS, mixS, inputS;

    double sr = 44100.0;

    // Filtres : composante grave (passe-bas 4e ordre), bande du "click", bande de la "boue", sous-harmoniques
    SVF lowA, lowB, clickHP, clickLP, mudHP, mudLP, subHP, subLP;

    Follower lowFast, lowSlow, lowRef;
    Follower clickFast, clickSlow, clickRef;
    Follower mudEnv, mudRef;
    Follower fullEnv, fullRef;

    // gains lissés (en dB)
    float gLowDb = 0.0f, gRestDb = 0.0f, gClickDb = 0.0f, mudDepth = 0.0f, tightDb = 0.0f;
    float kG = 0.0f, kClose = 0.0f, kOpen = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThistleProcessor)
};
