// 開発用: エディタを描画してPNGに保存する(xvfb-run で実行)
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    NamiDJFXProcessor proc;
    // 見栄え確認のため、いくつかの状態にしておく
    auto set = [&] (const juce::ParameterID& id, float v) { auto* p = proc.apvts.getParameter (id.getParamID()); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    set (ids::fxOn, 1.f); set (ids::fxType, (float) djfx::kEcho); set (ids::colorType, (float) djfx::kColorFilter); set (ids::colorKnob, -0.4f);
    set (ids::jogBackspin, 1.f); set (ids::bsBeat, 5.f);   // 1/4. (付点四分)
    // 波形表示の確認用に、ビートっぽい信号を数秒ぶん通しておく
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    {
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb; double ph = 0;
        for (int blk = 0; blk < 48000 * 5 / 512; ++blk)
        {
            if (blk == 48000 * 4 / 512) set (ids::jogLoop, 1.f);
            for (int i = 0; i < 512; ++i)
            {
                const double t = (double) (blk * 512 + i) / 48000.0, beat = std::fmod (t * 2.0, 1.0);
                const float env = (float) (std::exp (-beat * 6.0) * (0.35 + 0.5 * (std::fmod (t * 2.0, 2.0) < 1.0)));
                ph += 2.0 * juce::MathConstants<double>::pi * 110.0 / 48000.0;
                const float v = env * (float) std::sin (ph);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            proc.processBlock (buf, mb);
        }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* e = dynamic_cast<NamiDJFXEditor*> (ed.get());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
    auto img = e->renderSnapshot();
    juce::File out (argc > 1 ? argv[1] : "snapshot.png");
    out.deleteFile();
    juce::FileOutputStream fos (out);
    juce::PNGImageFormat().writeImageToStream (img, fos);
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
