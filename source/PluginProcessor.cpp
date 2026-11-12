#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
    // Corde la plus grave selon le type de guitare (fréquence fondamentale en Hz)
    const juce::StringArray kRangeNames { "6-string E", "6-string drop D", "7-string B",
                                          "7-string drop A", "8-string F#", "8-string drop E" };
    constexpr float kF0[6] = { 82.41f, 73.42f, 61.74f, 55.00f, 46.25f, 41.20f };

    const char* const kIds[8] = { "thump", "body", "click", "tight", "mud", "sub", "squash", "mix" };

    inline float toDb (float v) { return 20.0f * std::log10 (v + 1.0e-6f); }
}

//==============================================================================
ThistleProcessor::ThistleProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      presets (apvts, "THISTLE", "thistle", makeFactoryPresets())
{
    thumpP  = apvts.getRawParameterValue ("thump");
    bodyP   = apvts.getRawParameterValue ("body");
    clickP  = apvts.getRawParameterValue ("click");
    tightP  = apvts.getRawParameterValue ("tight");
    mudP    = apvts.getRawParameterValue ("mud");
    subP    = apvts.getRawParameterValue ("sub");
    squashP = apvts.getRawParameterValue ("squash");
    mixP    = apvts.getRawParameterValue ("mix");
    rangeP  = apvts.getRawParameterValue ("range");
    inputP  = apvts.getRawParameterValue ("input");

    // le type de guitare et le gain d'entrée appartiennent à l'utilisateur : un preset ne les change pas
    presets.setKeptParameters ({ "range", "input" });
}

juce::AudioProcessorValueTreeState::ParameterLayout ThistleProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    using Attr = juce::AudioParameterFloatAttributes;
    using Range = juce::NormalisableRange<float>;

    auto pctText  = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto pctParse = [] (const juce::String& t) { return juce::jlimit (0.0f, 1.0f, t.getFloatValue() / 100.0f); };

    auto signedText = [] (float v, int)
    {
        const int p = juce::roundToInt (v * 100.0f);
        return juce::String (p > 0 ? "+" : "") + juce::String (p) + " %";
    };
    auto signedParse = [] (const juce::String& t) { return juce::jlimit (-1.0f, 1.0f, t.getFloatValue() / 100.0f); };

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addPct = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<F> (juce::ParameterID { id, 1 }, name, Range (0.0f, 1.0f, 0.001f), def,
                                          Attr().withStringFromValueFunction (pctText)
                                                .withValueFromStringFunction (pctParse)));
    };

    addPct ("thump", "Thump", 0.35f);

    // BODY : de -100 % (grave plus court) à +100 % (grave plus long)
    p.push_back (std::make_unique<F> (juce::ParameterID { "body", 1 }, "Body", Range (-1.0f, 1.0f, 0.001f), 0.0f,
                                      Attr().withStringFromValueFunction (signedText)
                                            .withValueFromStringFunction (signedParse)));

    addPct ("click",  "Click",  0.30f);
    addPct ("tight",  "Tight",  0.00f);
    addPct ("mud",    "Mud",    0.20f);
    addPct ("sub",    "Sub",    0.00f);
    addPct ("squash", "Squash", 0.30f);
    addPct ("mix",    "Mix",    1.00f);

    p.push_back (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "range", 1 }, "String Range",
                                                               kRangeNames, 3));      // 7 cordes, drop la
    p.push_back (std::make_unique<F> (juce::ParameterID { "input", 1 }, "Input", Range (-12.0f, 24.0f, 0.1f), 0.0f,
                                      Attr().withLabel ("dB")));

    return { p.begin(), p.end() };
}

std::vector<FactoryPreset> ThistleProcessor::makeFactoryPresets()
{
    // thump, body (-1 à 1), click, tight, mud, sub, squash, mix
    auto mk = [] (const char* name, float th, float bo, float cl, float ti, float mu, float su, float sq, float mx)
    {
        FactoryPreset fp;
        fp.name = name;
        const float v[8] = { th, (bo + 1.0f) * 0.5f, cl, ti, mu, su, sq, mx };   // body : valeur normalisée
        for (int i = 0; i < 8; ++i)
            fp.values.emplace_back (kIds[i], v[i]);
        return fp;
    };

    return {
        mk ("Init",           0.00f,  0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 1.00f),
        mk ("Soft Thump",     0.35f,  0.00f, 0.30f, 0.00f, 0.20f, 0.00f, 0.30f, 1.00f),
        mk ("Tight Slap",     0.45f, -0.40f, 0.65f, 0.40f, 0.25f, 0.00f, 0.35f, 1.00f),
        mk ("Fat Thump",      0.60f,  0.20f, 0.25f, 0.10f, 0.40f, 0.30f, 0.45f, 1.00f),
        mk ("Clean Pop",      0.20f, -0.10f, 0.70f, 0.15f, 0.15f, 0.00f, 0.25f, 1.00f),
        mk ("Mud Fix",        0.25f, -0.20f, 0.20f, 0.20f, 0.75f, 0.00f, 0.30f, 1.00f),
        mk ("Parallel Punch", 0.70f,  0.00f, 0.50f, 0.00f, 0.30f, 0.20f, 0.70f, 0.50f)
    };
}

//==============================================================================
bool ThistleProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void ThistleProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const float fs = (float) sr;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    for (auto* f : { &lowA, &lowB, &clickHP, &clickLP, &mudHP, &mudLP, &subHP, &subLP })
    {
        f->prepare (spec);
        f->setResonance (0.707f);
        f->reset();
    }

    lowA.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowB.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    clickHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    clickLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    mudHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    mudLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    subHP.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    subLP.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    lowFast.set (1.0f, 20.0f, sr);    lowSlow.set (25.0f, 150.0f, sr);   lowRef.set (5.0f, 3000.0f, sr);
    clickFast.set (0.5f, 15.0f, sr);  clickSlow.set (20.0f, 120.0f, sr); clickRef.set (5.0f, 3000.0f, sr);
    mudEnv.set (3.0f, 60.0f, sr);     mudRef.set (5.0f, 3000.0f, sr);
    fullEnv.set (2.0f, 150.0f, sr);   fullRef.set (5.0f, 4000.0f, sr);

    for (auto* f : { &lowFast, &lowSlow, &lowRef, &clickFast, &clickSlow, &clickRef, &mudEnv, &mudRef, &fullEnv, &fullRef })
        f->reset();

    gLowDb = gRestDb = gClickDb = mudDepth = tightDb = 0.0f;
    kG     = 1.0f - std::exp (-1.0f / (0.003f * fs));
    kClose = 1.0f - std::exp (-1.0f / (0.030f * fs));
    kOpen  = 1.0f - std::exp (-1.0f / (0.003f * fs));

    juce::SmoothedValue<float>* smoothers[9] = { &thumpS, &bodyS, &clickS, &tightS, &mudS, &subS, &squashS, &mixS, &inputS };
    std::atomic<float>* params[9] = { thumpP, bodyP, clickP, tightP, mudP, subP, squashP, mixP, inputP };
    for (int i = 0; i < 9; ++i)
    {
        smoothers[i]->reset (sampleRate, 0.02);
        smoothers[i]->setCurrentAndTargetValue (params[i]->load());
    }

    setLatencySamples (0);
}

//==============================================================================
void ThistleProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();

    for (int c = numCh; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, n);

    if (numCh == 0 || n == 0)
        return;

    // --- Zones de fréquences calculées à partir de la corde la plus grave
    const int range = juce::jlimit (0, 5, (int) rangeP->load());
    const float f0  = kF0[range];
    const float nyq = 0.45f * (float) sr;

    lowA.setCutoffFrequency (juce::jmin (3.5f * f0, nyq));       // composante grave : f0 à ~3,5 x f0
    lowB.setCutoffFrequency (juce::jmin (3.5f * f0, nyq));
    clickHP.setCutoffFrequency (1500.0f);                        // zone du claquement : 1,5 à 5 kHz
    clickLP.setCutoffFrequency (juce::jmin (5000.0f, nyq));
    mudHP.setCutoffFrequency (2.5f * f0);                        // zone de la boue : 2,5 à 6 x f0
    mudLP.setCutoffFrequency (juce::jmin (6.0f * f0, nyq));
    subHP.setCutoffFrequency (0.8f * f0);                        // harmoniques des graves
    subLP.setCutoffFrequency (4.0f * f0);

    thumpS.setTargetValue (thumpP->load());   bodyS.setTargetValue (bodyP->load());
    clickS.setTargetValue (clickP->load());   tightS.setTargetValue (tightP->load());
    mudS.setTargetValue (mudP->load());       subS.setTargetValue (subP->load());
    squashS.setTargetValue (squashP->load()); mixS.setTargetValue (mixP->load());
    inputS.setTargetValue (inputP->load());

    float* ch[2] { buffer.getWritePointer (0), numCh > 1 ? buffer.getWritePointer (1) : nullptr };
    float blockPeak = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float thump = thumpS.getNextValue(),   body = bodyS.getNextValue();
        const float click = clickS.getNextValue(),   tight = tightS.getNextValue();
        const float mud = mudS.getNextValue(),       sub = subS.getNextValue();
        const float squash = squashS.getNextValue(), mix = mixS.getNextValue();
        const float inGain = juce::Decibels::decibelsToGain (inputS.getNextValue());

        // --- Entrée et séparation en composantes
        float x[2] { 0.0f, 0.0f }, L[2] { 0.0f, 0.0f }, C[2] { 0.0f, 0.0f }, M[2] { 0.0f, 0.0f };
        float eL = 0.0f, eC = 0.0f, eM = 0.0f, eF = 0.0f;

        for (int c = 0; c < numCh; ++c)
        {
            x[c] = ch[c][i] * inGain;
            blockPeak = juce::jmax (blockPeak, std::abs (x[c]));

            L[c] = lowB.processSample (c, lowA.processSample (c, x[c]));
            C[c] = clickLP.processSample (c, clickHP.processSample (c, x[c]));
            M[c] = mudLP.processSample (c, mudHP.processSample (c, x[c]));

            eL = juce::jmax (eL, std::abs (L[c]));
            eC = juce::jmax (eC, std::abs (C[c]));
            eM = juce::jmax (eM, std::abs (M[c]));
            eF = juce::jmax (eF, std::abs (x[c]));
        }

        // --- Graves : THUMP (attaque), BODY (durée), SQUASH (compression)
        const float lfDb = toDb (lowFast.process (eL));
        const float lsDb = toDb (lowSlow.process (eL));
        const float lrDb = toDb (lowRef.process (eL));
        const float wL   = juce::jlimit (0.0f, 1.0f, (lfDb - (lrDb - 45.0f)) / 10.0f);
        const float tr   = juce::jlimit (0.0f, 12.0f, lfDb - lsDb) * wL;      // phase d'attaque
        const float dec  = juce::jlimit (0.0f, 12.0f, lsDb - lfDb) * wL;      // phase de décroissance

        float gLowT = thump * 0.9f * tr + (body > 0.0f ? body * 0.6f * dec : body * 0.8f * dec);

        const float over    = juce::jmax (0.0f, lfDb - (lrDb - 6.0f));        // seuil relatif aux crêtes récentes
        const float ratio   = 1.0f + 7.0f * squash;
        const float grLow   = over * (1.0f - 1.0f / ratio);
        const float makeup  = squash * 3.0f;

        gLowT += -grLow + makeup;
        const float gRestT = -0.35f * grLow + makeup;                          // les aigus sont moins compressés

        gLowDb  += kG * (gLowT - gLowDb);
        gRestDb += kG * (gRestT - gRestDb);

        // --- CLICK : attaque dans les médiums-aigus
        const float cfDb = toDb (clickFast.process (eC));
        const float csDb = toDb (clickSlow.process (eC));
        const float crDb = toDb (clickRef.process (eC));
        const float wC   = juce::jlimit (0.0f, 1.0f, (cfDb - (crDb - 45.0f)) / 10.0f);
        const float trC  = juce::jlimit (0.0f, 12.0f, cfDb - csDb) * wC;
        gClickDb += kG * (click * 0.9f * trC - gClickDb);

        // --- MUD : coupe dynamique, plus forte quand la zone est près de son niveau maximal récent
        const float me   = mudEnv.process (eM);
        const float mr   = mudRef.process (eM);
        const float lvlN = juce::jlimit (0.0f, 1.0f, me / (mr + 1.0e-6f));
        mudDepth += kG * (mud * 0.8f * std::pow (lvlN, 1.5f) - mudDepth);

        // --- TIGHT : expandeur dont le seuil suit les crêtes récentes (indépendant du niveau)
        const float feDb = toDb (fullEnv.process (eF));
        const float frDb = toDb (fullRef.process (eF));
        float tightT = 0.0f;
        if (tight > 0.001f)
        {
            const float thr = frDb - 40.0f + 30.0f * tight;
            if (feDb < thr)
                tightT = -juce::jmin (40.0f, (thr - feDb) * (1.0f + 3.0f * tight));
        }
        tightDb += (tightT < tightDb ? kClose : kOpen) * (tightT - tightDb);

        // --- Recombinaison
        const float gL = juce::Decibels::decibelsToGain (gLowDb);
        const float gR = juce::Decibels::decibelsToGain (gRestDb);
        const float gC = juce::Decibels::decibelsToGain (gClickDb) - 1.0f;
        const float gT = juce::Decibels::decibelsToGain (tightDb);

        for (int c = 0; c < numCh; ++c)
        {
            // SUB : harmoniques des graves par redressement (2e harmonique dominante)
            const float subSig = subLP.processSample (c, subHP.processSample (c, std::abs (L[c]))) * sub * 2.0f;

            const float R   = x[c] - L[c];
            const float wet = (L[c] * gL + R * gR + C[c] * gC - M[c] * mudDepth + subSig) * gT;

            ch[c][i] = x[c] * (1.0f - mix) + wet * mix;
        }
    }

    inputPeak.store (blockPeak);
}

//==============================================================================
juce::AudioProcessorEditor* ThistleProcessor::createEditor()
{
    return new ThistleEditor (*this);
}

void ThistleProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    apvts.state.setProperty ("preset", presets.currentName, nullptr);

    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ThistleProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            presets.currentName = apvts.state.getProperty ("preset").toString();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThistleProcessor();
}
