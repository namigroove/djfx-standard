// プラグイン本体(Processor)の結合テスト: パラメータ・MIDI・モノ・状態保存・レイテンシ通知
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { ++failures; std::printf ("  FAIL: %s\n", msg); } } while (0)

static void setP (NamiDJFXProcessor& p, const juce::ParameterID& id, float v)
{
    auto* q = p.apvts.getParameter (id.getParamID());
    q->setValueNotifyingHost (q->convertTo0to1 (v));
}

static double runBlocks (NamiDJFXProcessor& p, juce::AudioBuffer<float>& b, juce::MidiBuffer& m, int blocks, double& energy, bool& finite, int startSample = 0)
{
    energy = 0; finite = true;
    const int n = b.getNumSamples();
    for (int k = 0; k < blocks; ++k)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < n; ++i)
                b.setSample (c, i, 0.5f * (float) std::sin (2 * juce::MathConstants<double>::pi * 440.0 * (startSample + k * n + i) / 48000.0));
        p.processBlock (b, m);
        m.clear();
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < n; ++i) { float v = b.getSample (c, i); if (! std::isfinite (v)) finite = false; energy += (double) v * v; }
    }
    return energy;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("[A] ステレオで素通し/パラメータ変更\n");
    {
        NamiDJFXProcessor p;
        p.setPlayConfigDetails (2, 2, 48000, 256);
        p.prepareToPlay (48000, 256);
        juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m; double e; bool fin;
        setP (p, ids::jogLatency, 0.f); // 0ms
        runBlocks (p, b, m, 4, e, fin);
        std::printf ("  idle energy=%.1f (sine 0.5amp: expect %.1f)\n", e, 0.5 * 0.5 / 2 * 256 * 2 * 4);
        CHECK (fin && std::fabs (e - 0.125 * 256 * 2 * 4) < 20.0, "bypass energy");
        for (int k = 0; k < 150; ++k) runBlocks (p, b, m, 1, e, fin, 1024 + k * 256);   // 履歴をためる

        std::printf ("[B] MIDI C1(36)でバックスピン → 音が変化し、ノートオフで戻る\n");
        juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
        for (int k = 0; k < 20; ++k) { runBlocks (p, b, k == 0 ? on : m, 1, e, fin, 40000 + k * 256); if (k == 0) on.clear(); }
        // 逆再生+減速: 440Hz正弦が壊れる(ゼロクロス数が変わる)ことを見る
        int z = 0; for (int i = 1; i < 256; ++i) if ((b.getSample (0, i - 1) < 0) != (b.getSample (0, i) < 0)) ++z;
        std::printf ("  zero crossings in block 20 = %d (idle 440Hz -> ~%d)\n", z, (int) (440.0 * 2 * 256 / 48000));
        CHECK (fin, "finite"); CHECK (std::abs (z - 5) > 2, "backspin changed the signal");
        juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 36), 0);
        runBlocks (p, b, off, 1, e, fin, 50000);
        for (int k = 0; k < 40; ++k) runBlocks (p, b, m, 1, e, fin, 50300 + k * 256);
        double ref = 0; for (int i = 0; i < 256; ++i) { double s = 0.5 * std::sin (2 * juce::MathConstants<double>::pi * 440.0 * (50300 + 39 * 256 + i) / 48000.0); ref = std::max (ref, std::fabs (s - b.getSample (0, i))); }
        std::printf ("  after note-off max diff vs live = %g\n", ref);
        CHECK (ref < 1e-3, "returned to live");

        std::printf ("[C] BEAT FXをパラメータでON → 反復が出る\n");
        setP (p, ids::fxType, (float) djfx::kEcho); setP (p, ids::fxLevel, 1.f); setP (p, ids::fxOn, 1.f); setP (p, ids::fxBeat, 0.f);
        runBlocks (p, b, m, 30, e, fin);
        std::printf ("  energy with echo = %.1f finite=%d\n", e, fin);
        CHECK (fin, "finite"); CHECK (e > 0.125 * 256 * 2 * 30 * 1.2, "echo adds energy");
    }
    std::printf ("[D] モノ(1ch)でも動く / カラーFILTER(ステレオ)\n");
    for (int ch = 1; ch <= 2; ++ch)
        for (int useColor = 0; useColor < 2; ++useColor)
        {
            NamiDJFXProcessor p;
            p.setPlayConfigDetails (ch, ch, 48000, 128);
            p.prepareToPlay (48000, 128);
            juce::AudioBuffer<float> b (ch, 128); juce::MidiBuffer m; double e; bool fin;
            if (useColor) { setP (p, ids::colorType, (float) djfx::kColorFilter); setP (p, ids::colorKnob, -0.6f); }
            runBlocks (p, b, m, 20, e, fin);
            std::printf ("  ch=%d color=%d energy=%.2f finite=%d\n", ch, useColor, e, (int) fin);
            CHECK (fin && e > 0.0, "finite & non-silent");
        }
    std::printf ("[E] 状態の保存/復元\n");
    {
        NamiDJFXProcessor a, c;
        setP (a, ids::fxType, (float) djfx::kMobius); setP (a, ids::colorKnob, 0.37f); setP (a, ids::bsTime, 2500.f);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        c.setStateInformation (mb.getData(), (int) mb.getSize());
        auto pa = readParams (a.apvts), pc = readParams (c.apvts);
        std::printf ("  type %d->%d color %.2f->%.2f bs %.0f->%.0f\n", pa.fxType, pc.fxType, pa.colorKnob, pc.colorKnob, pa.backspinTimeMs, pc.backspinTimeMs);
        CHECK (pa.fxType == pc.fxType && std::fabs (pa.colorKnob - pc.colorKnob) < 1e-4 && std::fabs (pa.backspinTimeMs - pc.backspinTimeMs) < 1e-2, "state roundtrip");
    }
    std::printf ("[F] パラメータ数: %d\n", (int) NamiDJFXProcessor().getParameters().size());
    std::printf ("\n%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
