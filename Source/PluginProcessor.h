#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Loudness.h"

class NamiDJFXProcessor : public juce::AudioProcessor, private juce::AsyncUpdater, private juce::AudioProcessorParameter::Listener
{
public:
    NamiDJFXProcessor();
    ~NamiDJFXProcessor() override;

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

    // 記録: ONの間、操作系のパラメータを「触っている」状態(ジェスチャー)にし続ける。
    // DAWのオートメーションがTouch/Latch/Writeなら、その間の動きが(動かしていない間の値も)記録される。メッセージスレッドから呼ぶ。
    void setRecording (bool on);
    std::atomic<bool> recording { false };

    // 波形表示用: 10msごとの最大値を直近約10秒ぶん(入力/出力)。オーディオスレッドが書き、画面が読む(ロック無し)
    static constexpr int kWaveSize = 1024;
    std::atomic<float> waveIn[kWaveSize], waveOut[kWaveSize];
    std::atomic<int> waveWIn { 0 }, waveWOut { 0 };
    std::atomic<float> lufsM { -200.f }, lufsS { -200.f }, lufsI { -200.f }, peakDb { -120.f };   // 出力のラウドネス(UIが読む)
    std::atomic<bool> resetLoud { false };
    std::atomic<float> loopNearMs { -1.f }, loopFarMs { -1.f };    // ループ中: ライブからの距離(ms)。ループ外は-1

private:
    void handleAsyncUpdate() override;
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int index, bool starting) override;
    juce::Array<juce::RangedAudioParameter*> recParams;
    std::atomic<uint32_t> endedMask { 0 };    // 記録中に画面操作でジェスチャーが終わったパラメータ(つかみ直す)
    std::atomic<int> midiRec { -1 };          // MIDI(G1)の押下: 1=押した 0=離した -1=なし
    bool selfGesture = false, lastRecMidi = false;
    void pushPeaks (const float* L, const float* R, int n, float& acc, int& cnt, std::atomic<float>* ring, std::atomic<int>& w);
    float accIn = 0.f, accOut = 0.f; int cntIn = 0, cntOut = 0, hop = 480;
    djfx::Engine engine;
    djfx::Loudness loud;
    int midiHeld[64] = {};   // ノートごとの押下状態
    int lastLatency = -1;
    std::atomic<int> pendingLatency { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamiDJFXProcessor)
};
