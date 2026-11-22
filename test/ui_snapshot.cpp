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
    set (ids::jogBackspin, 1.f);
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
