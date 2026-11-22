#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace ui
{
// 実機(ミキサー/プレーヤー)の面板をイメージした配色。基調はパレット(#0B132B #1C2541 #3A506B #5BC0BE #F3C969)。
// 面板は金属テクスチャ、文字はシルク印刷、点灯するものだけ色が付く。
const juce::Colour bg      (0xff0b132b);
const juce::Colour face    (0xff131c36);   // 面板の地(#0B132Bと#1C2541の間)
const juce::Colour panel   (0xff1c2541);
const juce::Colour line    (0xff3a506b);   // 刻印の線・ノブの縁
const juce::Colour padOff  (0xff0e1527);   // 押されていないパッド
const juce::Colour padEdge (0xff2c3b57);
const juce::Colour well    (0xff060b1a);   // 凹み(液晶・X-PAD・ジョグ)
const juce::Colour ink     (0xff0b132b);
const juce::Colour text    (0xffe3e9f2);   // シルク文字
const juce::Colour dim     (0xff8197b3);
const juce::Colour teal    (0xff5bc0be);
const juce::Colour amber   (0xfff3c969);
}

// フォントは Ricty Diminished Discord(assets/fonts/UIFont*.ttf)を埋め込む。無ければ等幅フォントで代用。
class NamiLnF : public juce::LookAndFeel_V4
{
public:
    NamiLnF();
    juce::Font font (float scale = 1.f) const;
    juce::Font fontBold (float scale = 1.f) const;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font(); }
    juce::Font getLabelFont (juce::Label&) override { return font(); }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    bool hasEmbeddedFont() const { return embedded; }
private:
    juce::Typeface::Ptr face, faceBold;
    bool embedded = false;
};

class HoldButton : public juce::TextButton
{
public:
    std::function<void (bool)> onHold;
    void buttonStateChanged() override
    {
        const bool d = isDown();
        if (d != last) { last = d; if (onHold) onHold (d); }
    }
private:
    bool last = false;
};

class StepSlider : public juce::Slider
{
public:
    std::function<int()> stepCount;
    double snapValue (double v, DragMode) override
    {
        const int n = stepCount ? stepCount() : 0;
        if (n < 2) return v;
        return std::round (v * (n - 1)) / (n - 1);
    }
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::ParameterID&, const juce::String& name,
          std::function<juce::String()> valueText, bool bipolar = false, bool stepped = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    StepSlider slider;
    juce::String name;
    std::function<juce::String()> valueText;
private:
    std::unique_ptr<juce::SliderParameterAttachment> att;
};

class ChoiceGroup : public juce::Component, private juce::Timer
{
public:
    ChoiceGroup (juce::AudioProcessorValueTreeState&, const juce::ParameterID&, juce::StringArray labels, int cols, int gap = 4, bool led = false);
    void resized() override;
private:
    void timerCallback() override;
    juce::AudioProcessorValueTreeState& apvts;
    juce::String pid;
    int cols, gap;
    juce::OwnedArray<juce::TextButton> buttons;
};

class XPad : public juce::Component
{
public:
    XPad (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void setFrom (const juce::MouseEvent&);
    juce::ParameterAttachment pos, touch;
    float x = 0.5f; bool touching = false;
};

class JogWheel : public juce::Component, private juce::Timer
{
public:
    JogWheel (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    float angleAt (juce::Point<float>) const;
    juce::ParameterAttachment speed, touch;
    bool dragging = false;
    float lastAngle = 0.f, curAngle = 0.f, shown = 0.f, speedSmoothed = 0.f;
    double lastTime = 0.0;
};

// BEAT FXの液晶表示(エフェクト名・拍・時間・BPM・レベル)
class FxDisplay : public juce::Component, private juce::Timer
{
public:
    explicit FxDisplay (NamiDJFXProcessor&);
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    NamiDJFXProcessor& proc;
};

class NamiDJFXEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NamiDJFXEditor (NamiDJFXProcessor&);
    ~NamiDJFXEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Image renderSnapshot() { return createComponentSnapshot (getLocalBounds()); }

private:
    void timerCallback() override;
    void sectionHeader (juce::Graphics&, juce::Rectangle<int>, const juce::String&);
    void buildFace (int w, int h);
    juce::TextButton* addToggle (const juce::ParameterID&, const juce::String& label, juce::Colour accent = ui::teal);
    HoldButton* addHold (const juce::ParameterID&, const juce::String& label);
    float raw (const juce::ParameterID& id) const { return proc.apvts.getRawParameterValue (id.getParamID())->load(); }

    NamiDJFXProcessor& proc;
    NamiLnF lnf;
    juce::Image faceImage;
    juce::Rectangle<int> boxA, boxB, boxC;

    std::unique_ptr<ChoiceGroup> fxTypes, fxBands, colorTypes, latency;
    std::unique_ptr<XPad> xpad;
    std::unique_ptr<JogWheel> jog;
    std::unique_ptr<FxDisplay> lcd;
    std::unique_ptr<Knob> kBeat, kTime, kLevel, kColor, kParam, kBs, kRate, kBrake, kLoop, kBpm, kOut;
    juce::OwnedArray<juce::TextButton> toggles;
    juce::OwnedArray<HoldButton> holds;
    juce::OwnedArray<juce::ButtonParameterAttachment> btnAtts;
    juce::TextButton* bOn = nullptr; juce::TextButton* bSync = nullptr; juce::TextButton* bLock = nullptr;
    HoldButton *hBack = nullptr, *hBrake = nullptr, *hRev = nullptr, *hLoop = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamiDJFXEditor)
};
