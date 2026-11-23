#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/Engine.h"

namespace ids
{
#define NAMI_ID(x) static const juce::ParameterID x { #x, 1 }
NAMI_ID (fxOn); NAMI_ID (fxType); NAMI_ID (bandLow); NAMI_ID (bandMid); NAMI_ID (bandHi); NAMI_ID (fxSync); NAMI_ID (fxBeat); NAMI_ID (fxTime);
NAMI_ID (fxLevel); NAMI_ID (xpadTouch); NAMI_ID (xpadPos);
NAMI_ID (colorType); NAMI_ID (colorKnob); NAMI_ID (colorParam); NAMI_ID (centerLock);
NAMI_ID (jogBackspin); NAMI_ID (jogBrake); NAMI_ID (jogReverse); NAMI_ID (jogScratch); NAMI_ID (jogLoop);
NAMI_ID (jogSpeed); NAMI_ID (bsTime); NAMI_ID (bsRate); NAMI_ID (brakeTime); NAMI_ID (loopBeat); NAMI_ID (jogLatency);
NAMI_ID (bpm); NAMI_ID (outGain); NAMI_ID (bsBeat); NAMI_ID (brakeBeat);
NAMI_ID (releaseTime); NAMI_ID (releaseBeat); NAMI_ID (slip); NAMI_ID (quantize); NAMI_ID (quantBeat); NAMI_ID (loopLatch); NAMI_ID (jogResync); NAMI_ID (inertia); NAMI_ID (pitchLock); NAMI_ID (stopQuant);
#undef NAMI_ID
}

static const float kLatencyMs[] = { 0.f, 60.f, 125.f, 250.f, 500.f };

// 再生速度系(BACKSPIN/BRAKE)の長さを音符で決める。0=FREE(msで指定)。拍数は4/4の1拍=四分音符
static const char* const kNoteNames[] = { "FREE", "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2", "1/2.", "1bar", "2bar", "4bar" };
static const float kNoteBeats[]       = { 0.f,    0.25f,  0.5f,  0.75f,  1.f,   1.5f,   2.f,   3.f,    4.f,    8.f,    16.f };
static constexpr int kNumNotes = 11;
static const float kQuantBeats[] = { 0.125f, 0.25f, 0.5f, 1.f };
inline float noteMs (int idx, double bpm) { return (float) (kNoteBeats[juce::jlimit (0, kNumNotes - 1, idx)] * 60000.0 / std::max (30.0, bpm)); }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto boolP = [&] (const ParameterID& id, const String& n, bool def) { p.push_back (std::make_unique<AudioParameterBool> (id, n, def)); };
    auto floatP = [&] (const ParameterID& id, const String& n, NormalisableRange<float> r, float def, const String& unit = {})
    { p.push_back (std::make_unique<AudioParameterFloat> (id, n, r, def, AudioParameterFloatAttributes().withLabel (unit))); };

    StringArray fx, color;
    for (int i = 0; i < djfx::kNumFx; ++i) fx.add (djfx::fxName (i));
    for (int i = 0; i < djfx::kNumColor; ++i) color.add (djfx::colorName (i));

    boolP (ids::fxOn, "FX On", false);
    p.push_back (std::make_unique<AudioParameterChoice> (ids::fxType, "FX Type", fx, (int) djfx::kEcho));
    boolP (ids::bandLow, "FX Freq Low", true);
    boolP (ids::bandMid, "FX Freq Mid", true);
    boolP (ids::bandHi, "FX Freq Hi", true);
    boolP (ids::fxSync, "FX BPM Sync", true);
    floatP (ids::fxBeat, "FX Beat", { 0.f, 1.f }, 0.5f);
    NormalisableRange<float> tr (1.f, 32000.f, 0.f, 0.3f);
    floatP (ids::fxTime, "FX Time", tr, 500.f, "ms");
    floatP (ids::fxLevel, "FX Level/Depth", { 0.f, 1.f }, 0.5f);
    boolP (ids::xpadTouch, "X-PAD Touch", false);
    floatP (ids::xpadPos, "X-PAD Position", { 0.f, 1.f }, 0.5f);

    p.push_back (std::make_unique<AudioParameterChoice> (ids::colorType, "Color FX", color, 0));
    floatP (ids::colorKnob, "Color", { -1.f, 1.f }, 0.f);
    floatP (ids::colorParam, "Color Parameter", { 0.f, 1.f }, 0.5f);
    boolP (ids::centerLock, "Center Lock", false);

    boolP (ids::jogBackspin, "Backspin", false);
    boolP (ids::jogBrake, "Brake", false);
    boolP (ids::jogReverse, "Reverse", false);
    boolP (ids::jogScratch, "Scratch Touch", false);
    boolP (ids::jogLoop, "Beat Loop", false);
    floatP (ids::jogSpeed, "Scratch Speed", { -4.f, 4.f }, 0.f);
    floatP (ids::bsTime, "Backspin Time", { 200.f, 4000.f, 0.f, 0.5f }, 1200.f, "ms");
    floatP (ids::bsRate, "Backspin Rate", { 1.f, 10.f }, 5.f, "x");
    floatP (ids::brakeTime, "Brake Time", { 50.f, 5000.f, 0.f, 0.5f }, 800.f, "ms");
    floatP (ids::loopBeat, "Loop Beats", { 0.f, 1.f }, 0.5f);
    p.push_back (std::make_unique<AudioParameterChoice> (ids::jogLatency, "Scratch Lookahead", StringArray { "0 ms", "60 ms", "125 ms", "250 ms", "500 ms" }, 0));

    {
        StringArray notes; for (auto* n : kNoteNames) notes.add (n);
        p.push_back (std::make_unique<AudioParameterChoice> (ids::bsBeat, "Backspin Length (note)", notes, 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ids::brakeBeat, "Brake Length (note)", notes, 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ids::releaseBeat, "Release Length (note)", notes, 0));
    }
    floatP (ids::releaseTime, "Release Time", { 50.f, 5000.f, 0.f, 0.5f }, 400.f, "ms");
    boolP (ids::slip, "Slip", true);
    boolP (ids::quantize, "Quantize", false);
    p.push_back (std::make_unique<AudioParameterChoice> (ids::quantBeat, "Quantize Beat", StringArray { "1/8", "1/4", "1/2", "1" }, 3));
    boolP (ids::loopLatch, "Loop Reloop/Exit", false);
    boolP (ids::jogResync, "Resync to Live", false);
    floatP (ids::inertia, "Inertia", { 0.f, 3000.f, 0.f, 0.5f }, 400.f, "ms");
    boolP (ids::pitchLock, "Key Lock", false);
    boolP (ids::stopQuant, "Stop Quantize", false);
    floatP (ids::bpm, "BPM (host unknown)", { 30.f, 300.f }, 120.f);
    floatP (ids::outGain, "Output", { -24.f, 6.f }, 0.f, "dB");
    return { p.begin(), p.end() };
}

inline djfx::Params readParams (juce::AudioProcessorValueTreeState& s)
{
    auto f = [&] (const juce::ParameterID& id) { return s.getRawParameterValue (id.getParamID())->load(); };
    djfx::Params p;
    p.fxOn = f (ids::fxOn) > 0.5f;           p.fxType = (int) f (ids::fxType);
    p.fxBandMask = (f (ids::bandLow) > 0.5f ? 1 : 0) | (f (ids::bandMid) > 0.5f ? 2 : 0) | (f (ids::bandHi) > 0.5f ? 4 : 0);
    p.fxSync = f (ids::fxSync) > 0.5f;       p.fxBeat = f (ids::fxBeat);             p.fxTimeMs = f (ids::fxTime);
    p.fxLevel = f (ids::fxLevel);            p.xpadTouch = f (ids::xpadTouch) > 0.5f; p.xpad = f (ids::xpadPos);
    p.colorType = (int) f (ids::colorType);  p.colorKnob = f (ids::colorKnob);       p.colorParam = f (ids::colorParam);
    p.centerLock = f (ids::centerLock) > 0.5f;
    p.jogBackspin = f (ids::jogBackspin) > 0.5f; p.jogBrake = f (ids::jogBrake) > 0.5f; p.jogReverse = f (ids::jogReverse) > 0.5f;
    p.jogScratch = f (ids::jogScratch) > 0.5f;   p.jogLoop = f (ids::jogLoop) > 0.5f;   p.jogSpeed = f (ids::jogSpeed);
    p.backspinTimeMs = f (ids::bsTime);      p.backspinRate = f (ids::bsRate);       p.brakeTimeMs = f (ids::brakeTime);
    p.loopBeat = f (ids::loopBeat);
    p.releaseTimeMs = f (ids::releaseTime);  p.releaseBeats = kNoteBeats[juce::jlimit (0, kNumNotes - 1, (int) f (ids::releaseBeat))];
    p.inertiaMs = f (ids::inertia); p.pitchLock = f (ids::pitchLock) > 0.5f; p.stopQuant = f (ids::stopQuant) > 0.5f;
    p.slip = f (ids::slip) > 0.5f;           p.quantize = f (ids::quantize) > 0.5f;
    p.quantBeats = kQuantBeats[juce::jlimit (0, 3, (int) f (ids::quantBeat))];
    p.loopLatch = f (ids::loopLatch) > 0.5f; p.jogResync = f (ids::jogResync) > 0.5f;
    p.backspinBeats = kNoteBeats[juce::jlimit (0, kNumNotes - 1, (int) f (ids::bsBeat))];
    p.brakeBeats = kNoteBeats[juce::jlimit (0, kNumNotes - 1, (int) f (ids::brakeBeat))];
    const int li = juce::jlimit (0, 4, (int) f (ids::jogLatency));
    p.jogLatencyMs = kLatencyMs[li];
    p.bpmFallback = f (ids::bpm);            p.outGainDb = f (ids::outGain);
    return p;
}
