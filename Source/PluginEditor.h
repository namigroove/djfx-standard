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
    std::function<void()> onPopup;           // 右クリック(メニュー)。ドラッグはしない
    void mouseDown (const juce::MouseEvent& e) override
    {
        popupDown = e.mods.isPopupMenu() && onPopup != nullptr;
        if (popupDown) { onPopup(); return; }
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (! popupDown) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override   { if (! popupDown) juce::Slider::mouseUp (e); popupDown = false; }
    bool popupDown = false;
    double snapValue (double v, DragMode) override
    {
        const int n = stepCount ? stepCount() : 0;
        if (n < 2) return v;
        return std::round (v * (n - 1)) / (n - 1);
    }
};

class Knob : public juce::Component, private juce::Timer
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::ParameterID&, const juce::String& name,
          std::function<juce::String()> valueText, bool bipolar = false, bool stepped = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    StepSlider slider;
    juce::String name, minLabel, maxLabel;
    std::function<juce::String()> valueText;
    std::function<bool()> lockOn;            // CENTER LOCK(ONの間、中央を越えて回せない)
    // 数字だけ打てる欄。単位は欄の外に固定表示。numText: 表示する数字, onTyped: 確定した数字
    void makeEditable (const juce::String& unitText, std::function<juce::String()> numText, std::function<void (double)> onTyped);
    std::function<juce::String()> tagText;   // 値の左に出す小さな札(例: 1/4)
    void enableCenterLock();
private:
    void timerCallback() override;
    void layoutEdit();
    juce::Label edit;
    juce::String unit;
    std::function<juce::String()> numberText;
    bool locked = false; int lastSide = 0, lockSide = 0, guard = 0;
    std::unique_ptr<juce::SliderParameterAttachment> att;
};

class ChoiceGroup : public juce::Component, private juce::Timer
{
public:
    ChoiceGroup (juce::AudioProcessorValueTreeState&, const juce::ParameterID&, juce::StringArray labels, int cols, int gap = 4, bool led = false,
                 int indexOffset = 0, bool toggleOff = false);
    void resized() override;
    juce::Array<int> values;                 // 空なら i+offset。実機の並びとパラメータ順が違うときに使う
private:
    int valueOf (int i) const { return values.isEmpty() ? i + offset : values[i]; }
    void timerCallback() override;
    juce::AudioProcessorValueTreeState& apvts;
    juce::String pid;
    int cols, gap, offset;
    bool offToggle;
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
    void mouseDown (const juce::MouseEvent&) override;
    void stepBeat (int dir);                 // BEAT ◀▶ と同じ動き(拍同期に切り替える)
private:
    void setBeatIndex (int idx);
    void timerCallback() override { repaint(); }
    NamiDJFXProcessor& proc;
};

// エフェクト選択のロータリー。実機のように周りに名前が並び、つまみが選択中を指す。
class FxSelector : public juce::Component
{
public:
    explicit FxSelector (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
private:
    juce::Rectangle<float> labelRect (int i) const;
    float angleOf (int i) const;
    void select (int i);
    juce::AudioProcessorValueTreeState& apvts;
    int dragStartIndex = 0;
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
    juce::TextButton* addAction (const juce::String& label, std::function<void()> onClick);
    juce::TextButton* addToggle (const juce::ParameterID&, const juce::String& label, juce::Colour accent = ui::teal);
    HoldButton* addHold (const juce::ParameterID&, const juce::String& label);
    float raw (const juce::ParameterID& id) const { return proc.apvts.getRawParameterValue (id.getParamID())->load(); }

    NamiDJFXProcessor& proc;
    NamiLnF lnf;
    juce::Image faceImage;
    juce::Rectangle<int> boxA, boxB, boxC;

    std::unique_ptr<ChoiceGroup> colorTypes, latency, quantBeats;
    std::unique_ptr<FxSelector> fxSel;
    std::unique_ptr<XPad> xpad;
    std::unique_ptr<JogWheel> jog;
    std::unique_ptr<FxDisplay> lcd;
    std::unique_ptr<Knob> kTime, kLevel, kColor, kParam, kBs, kRate, kBrake, kRelease, kLoop, kBpm, kOut;
    juce::OwnedArray<juce::TextButton> toggles;
    juce::OwnedArray<HoldButton> holds;
    juce::OwnedArray<juce::ButtonParameterAttachment> btnAtts;
    juce::TextButton *bOn = nullptr, *bSync = nullptr, *bLock = nullptr, *bLow = nullptr, *bMid = nullptr, *bHi = nullptr;
    juce::TextButton *bPrev = nullptr, *bNext = nullptr, *bTap = nullptr;
    std::vector<double> tapTimes;
    HoldButton *hBack = nullptr, *hBrake = nullptr, *hRev = nullptr, *hLoop = nullptr, *hLive = nullptr;
    juce::TextButton *tSlip = nullptr, *tQuant = nullptr, *bReloop = nullptr, *bHalf = nullptr, *bDouble = nullptr;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NamiDJFXEditor)
};
