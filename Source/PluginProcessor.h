#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

class NamiDJFXProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    NamiDJFXProcessor();
    ~NamiDJFXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "DJFX Standard"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> jogLagMs { 0.f };    // 画面表示用(SLIP OFFでライブより遅れている長さ)
    std::atomic<float> lastBpm { 120.f };   // 画面表示用(ホストのBPM。不明なら手入力BPM)

private:
    void handleAsyncUpdate() override;
    djfx::Engine engine;
    int midiHeld[64] = {};   // ノートごとの押下状態
    int lastLatency = -1;
    std::atomic<int> pendingLatency { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamiDJFXProcessor)
};
