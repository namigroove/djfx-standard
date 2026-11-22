#include "PluginEditor.h"
#if NAMIDJFX_HAS_FONT
 #include "BinaryData.h"
#endif
using namespace juce;

static float textWidth (const Font& f, const String& s)
{
    GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.f, 0.f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// ------------------------------------------------------------------ LookAndFeel
NamiLnF::NamiLnF()
{
#if NAMIDJFX_HAS_FONT
    face = Typeface::createSystemTypefaceFor (BinaryData::UIFont_ttf, (size_t) BinaryData::UIFont_ttfSize);
    faceBold = Typeface::createSystemTypefaceFor (BinaryData::UIFontBold_ttf, (size_t) BinaryData::UIFontBold_ttfSize);
    embedded = face != nullptr;
#endif
    setColour (Label::textColourId, ui::text);
}

Font NamiLnF::font (float scale) const
{
    if (face != nullptr) return Font (FontOptions (face).withHeight (13.f * scale));
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f * scale, Font::plain));
}
Font NamiLnF::fontBold (float scale) const
{
    if (faceBold != nullptr) return Font (FontOptions (faceBold).withHeight (13.f * scale));
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f * scale, Font::bold));
}

// 小さなパッド。点灯中は縁と文字に色が付く。"led"なら左にランプ、"round"なら丸い大ボタン、"bold"なら実機のFREQUENCYボタンのような太い縁。
void NamiLnF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool hover, bool down)
{
    const Colour accent ((uint32) (int) b.getProperties().getWithDefault ("accent", (int) ui::teal.getARGB()));
    const bool on = b.getToggleState();
    const bool round = (bool) b.getProperties().getWithDefault ("round", false);
    const bool bold = (bool) b.getProperties().getWithDefault ("bold", false);
    auto r = b.getLocalBounds().toFloat();

    if (round)
    {
        const bool lit = on || down;
        const float d = std::min (r.getWidth(), r.getHeight()) - 2.f;
        auto c = r.getCentre();
        g.setColour (ui::well);   g.fillEllipse (c.x - d / 2 - 3, c.y - d / 2 - 3, d + 6, d + 6);
        g.setColour (ui::padEdge); g.drawEllipse (c.x - d / 2 - 3, c.y - d / 2 - 3, d + 6, d + 6, 1.f);
        g.setColour (lit ? accent : ui::line);
        g.fillEllipse (c.x - d / 2, c.y - d / 2, d, d);                 // リングのLED
        const float di = d - 7.f;
        g.setColour (ui::padOff);
        g.fillEllipse (c.x - di / 2, c.y - di / 2, di, di);
        g.setColour (ui::padEdge);
        g.drawEllipse (c.x - di / 2, c.y - di / 2, di, di, 1.f);
        return;
    }

    r = r.reduced (0.5f);
    g.setColour (down ? ui::panel : ui::padOff);
    g.fillRoundedRectangle (r, bold ? 3.f : 2.f);
    if (on) { g.setColour (accent.withAlpha (bold ? 0.22f : 0.14f)); g.fillRoundedRectangle (r, bold ? 3.f : 2.f); }
    if (bold)
    {
        g.setColour (on ? accent : ui::padEdge);
        g.drawRoundedRectangle (r, 3.f, 2.f);
        g.setColour (on ? accent.withAlpha (0.45f) : Colour (0xff18233e));
        g.drawRoundedRectangle (r.reduced (3.f), 2.f, 1.f);
    }
    else
    {
        g.setColour (on ? accent : (hover ? ui::dim : ui::padEdge));
        g.drawRoundedRectangle (r, 2.f, 1.f);
    }
    if ((bool) b.getProperties().getWithDefault ("led", false))
    {
        const float cy = r.getCentreY();
        g.setColour (on ? accent : Colour (0xff202c47));
        g.fillEllipse (r.getX() + 7.f, cy - 2.5f, 5.f, 5.f);
    }
}

void NamiLnF::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const Colour accent ((uint32) (int) b.getProperties().getWithDefault ("accent", (int) ui::teal.getARGB()));
    const bool on = b.getToggleState();
    const bool round = (bool) b.getProperties().getWithDefault ("round", false);
    const int arrow = (int) b.getProperties().getWithDefault ("arrow", 0);
    if (arrow != 0)                                            // ◀ ▶ はフォントに頼らず三角形を描く
    {
        auto c = b.getLocalBounds().toFloat().getCentre();
        Path t; const float sz = 6.f;
        t.addTriangle (c.x + arrow * sz, c.y, c.x - arrow * sz * 0.8f, c.y - sz, c.x - arrow * sz * 0.8f, c.y + sz);
        g.setColour (ui::text);
        g.fillPath (t);
        return;
    }
    g.setFont (font (round ? 0.8f : 0.9f));
    g.setColour (on ? (round ? ui::text : accent) : ui::text.withAlpha (0.82f));
    auto area = b.getLocalBounds().reduced (2, 0);
    if ((bool) b.getProperties().getWithDefault ("led", false)) area.removeFromLeft (10);
    g.drawText (b.getButtonText(), area, Justification::centred, false);
}

// つまみのキャップ。ring: 黒い胴に白いリング(EQ/TRIMなど)、knurl: 滑り止めの溝付き(COLOR・エフェクト選択)
static void drawCap (Graphics& g, Point<float> c, float rBody, float angle, bool knurl, bool dimmed)
{
    g.setColour (ui::well);                                  // 取り付け穴の影
    g.fillEllipse (c.x - rBody - 2.f, c.y - rBody - 2.f, (rBody + 2.f) * 2, (rBody + 2.f) * 2);
    if (knurl)
    {
        g.setColour (Colour (0xff1b2745));
        g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
        for (int i = 0; i < 36; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 36.f;
            g.setColour (Colour (0xff7d93b3));
            g.drawLine (c.x + std::sin (a) * (rBody - 6.f), c.y - std::cos (a) * (rBody - 6.f),
                        c.x + std::sin (a) * rBody, c.y - std::cos (a) * rBody, 2.f);
        }
        g.setColour (ui::padEdge);
        g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2, 1.f);
        const float rt = rBody - 7.f;
        g.setColour (Colour (0xff0e1528)); g.fillEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2);
        g.setColour (Colour (0xff26355a)); g.drawEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2, 1.f);
    }
    else
    {
        g.setColour (ui::ink);                               // 黒い胴
        g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
        g.setColour (dimmed ? ui::dim : ui::text);           // 白いリング
        g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2, 2.4f);
        g.setColour (Colour (0xff17213b));
        g.drawEllipse (c.x - rBody + 4.f, c.y - rBody + 4.f, (rBody - 4.f) * 2, (rBody - 4.f) * 2, 1.f);
    }
    g.setColour (dimmed ? ui::dim : ui::text);               // 白い太いポインタ
    g.drawLine (c.x + std::sin (angle) * (rBody * 0.12f), c.y - std::cos (angle) * (rBody * 0.12f),
                c.x + std::sin (angle) * (rBody - 3.f), c.y - std::cos (angle) * (rBody - 3.f), 3.f);
}

void NamiLnF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const auto area = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = std::min (area.getWidth(), area.getHeight());
    const auto c = area.getCentre();
    const float rOuter = d * 0.5f - 1.f;
    const float rBody = rOuter - 7.f;
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool knurl = (bool) s.getProperties().getWithDefault ("knurl", false);
    const bool dimmed = s.getAlpha() < 0.9f;

    if (knurl)   // COLORのように、左右へ開いた白い弧(上は切れている)
    {
        const float mid = (a0 + a1) * 0.5f, gap = 0.12f;
        for (int side = 0; side < 2; ++side)
        {
            Path arc;
            const float from = side ? mid + gap : a0 - 0.05f, to = side ? a1 + 0.05f : mid - gap;
            arc.addCentredArc (c.x, c.y, rOuter - 1.f, rOuter - 1.f, 0.f, from, to, true);
            g.setColour (ui::text.withAlpha (0.9f));
            g.strokePath (arc, PathStrokeType (1.6f));
        }
    }
    else
        for (int i = 0; i <= 10; ++i)   // 実機のような点の目盛り(中央だけ大きい)
        {
            const float a = a0 + (a1 - a0) * (float) i / 10.f;
            const bool mid = bipolar && i == 5;
            const float rr = rOuter - 1.f;
            g.setColour (mid ? ui::amber : ui::text.withAlpha (0.8f));
            const float rad = (i == 0 || i == 10 || mid) ? 1.7f : 1.2f;
            g.fillEllipse (c.x + std::sin (a) * rr - rad, c.y - std::cos (a) * rr - rad, rad * 2, rad * 2);
        }
    drawCap (g, c, knurl ? rBody + 1.f : rBody, a0 + pos * (a1 - a0), knurl, dimmed);
}

// ------------------------------------------------------------------ Knob
Knob::Knob (AudioProcessorValueTreeState& s, const ParameterID& id, const String& n, std::function<String()> vt, bool bipolar, bool stepped)
    : name (n), valueText (std::move (vt))
{
    slider.setSliderStyle (Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    slider.getProperties().set ("bipolar", bipolar);
    slider.getProperties().set ("stepped", stepped);
    slider.setVelocityBasedMode (false);
    if (bipolar) slider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (slider);
    att = std::make_unique<SliderParameterAttachment> (*s.getParameter (id.getParamID()), slider);
}
void Knob::resized()
{
    const int d = std::min (getWidth(), getHeight() - 42);
    slider.setBounds ((getWidth() - d) / 2, 0, d, d);
}
void Knob::paint (Graphics& g)
{
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    const auto sb = slider.getBounds().toFloat();
    const auto c = sb.getCentre();
    const float r = sb.getWidth() * 0.5f - 1.f;
    g.setFont (l.font (0.78f));
    g.setColour (ui::dim);
    const float a0 = MathConstants<float>::pi * 1.2f, a1 = MathConstants<float>::pi * 2.8f;
    auto endLabel = [&] (const String& t, float a, Justification j)
    {
        if (t.isEmpty()) return;
        const float x = c.x + std::sin (a) * r, y = c.y - std::cos (a) * r + 9.f;
        const int tw = 44;
        g.drawText (t, (int) (j == Justification::centredRight ? x + 6 - tw : x - 6), (int) y - 6, tw, 12, j, false);
    };
    endLabel (minLabel, a0, Justification::centredRight);
    endLabel (maxLabel, a1, Justification::centredLeft);
    g.setFont (l.font (0.9f));
    g.setColour (ui::amber);
    g.drawText (valueText ? valueText() : String(), 0, getHeight() - 28, getWidth(), 14, Justification::centred, false);
    g.setColour (ui::text.withAlpha (0.9f));
    g.drawText (name, 0, getHeight() - 14, getWidth(), 14, Justification::centred, false);
}

// ------------------------------------------------------------------ ChoiceGroup
ChoiceGroup::ChoiceGroup (AudioProcessorValueTreeState& s, const ParameterID& id, StringArray labels, int c, int g, bool led,
                          int indexOffset, bool toggleOff)
    : apvts (s), pid (id.getParamID()), cols (c), gap (g), offset (indexOffset), offToggle (toggleOff)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new TextButton (labels[i]));
        b->getProperties().set ("led", led);
        b->onClick = [this, i]
        {
            auto* p = apvts.getParameter (pid);
            const int cur = (int) apvts.getRawParameterValue (pid)->load();
            const int target = (offToggle && cur == i + offset) ? 0 : i + offset;   // 点灯中をもう一度押すとOFF
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) target));
            p->endChangeGesture();
        };
        addAndMakeVisible (b);
    }
    startTimerHz (30);
}
void ChoiceGroup::resized()
{
    const int n = buttons.size(), rows = (n + cols - 1) / cols;
    const int w = (getWidth() - gap * (cols - 1)) / cols, h = (getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < n; ++i)
        buttons[i]->setBounds ((i % cols) * (w + gap), (i / cols) * (h + gap), w, h);
}
void ChoiceGroup::timerCallback()
{
    const int cur = (int) apvts.getRawParameterValue (pid)->load();
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (i + offset == cur, dontSendNotification);
}

// ------------------------------------------------------------------ X-PAD
XPad::XPad (AudioProcessorValueTreeState& s)
    : pos (*s.getParameter (ids::xpadPos.getParamID()), [this] (float v) { x = v; repaint(); }),
      touch (*s.getParameter (ids::xpadTouch.getParamID()), [this] (float v) { touching = v > 0.5f; repaint(); })
{
    pos.sendInitialUpdate(); touch.sendInitialUpdate();
}
void XPad::setFrom (const MouseEvent& e) { pos.setValueAsPartOfGesture (jlimit (0.f, 1.f, e.position.x / (float) getWidth())); }
void XPad::mouseDown (const MouseEvent& e) { pos.beginGesture(); touch.beginGesture(); touch.setValueAsPartOfGesture (1.f); setFrom (e); }
void XPad::mouseDrag (const MouseEvent& e) { setFrom (e); }
void XPad::mouseUp (const MouseEvent&) { touch.setValueAsPartOfGesture (0.f); touch.endGesture(); pos.endGesture(); }
void XPad::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ui::well);
    g.fillRoundedRectangle (r, 3.f);
    if (touching) { g.setColour (ui::amber.withAlpha (0.12f)); g.fillRoundedRectangle (r, 3.f); }
    g.setColour (Colour (0xff1c2a47));                          // 面のドット格子
    for (int ix = 1; ix < 16; ++ix)
        for (int iy = 1; iy < 7; ++iy)
            g.fillRect (r.getWidth() * (float) ix / 16.f - 0.5f, r.getHeight() * (float) iy / 7.f - 0.5f, 1.5f, 1.5f);
    g.setColour (touching ? ui::amber : ui::padEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    const float px = x * r.getWidth();
    g.setColour (touching ? ui::amber : ui::text.withAlpha (0.8f));
    g.fillRect (px - 1.f, 3.f, 2.f, r.getHeight() - 6.f);
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    g.setFont (l.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("X-PAD", 8, getHeight() - 18, 80, 14, Justification::left, false);
}

// ------------------------------------------------------------------ JOG
JogWheel::JogWheel (AudioProcessorValueTreeState& s)
    : speed (*s.getParameter (ids::jogSpeed.getParamID()), [] (float) {}),
      touch (*s.getParameter (ids::jogScratch.getParamID()), [] (float) {})
{
    startTimerHz (60);
}
float JogWheel::angleAt (Point<float> p) const
{
    auto c = getLocalBounds().toFloat().getCentre();
    return std::atan2 (p.y - c.y, p.x - c.x);
}
void JogWheel::mouseDown (const MouseEvent& e)
{
    dragging = true; touch.beginGesture(); speed.beginGesture();
    touch.setValueAsPartOfGesture (1.f);
    lastAngle = curAngle = angleAt (e.position); speedSmoothed = 0.f;
    lastTime = Time::getMillisecondCounterHiRes();
}
void JogWheel::mouseDrag (const MouseEvent& e) { curAngle = angleAt (e.position); }
void JogWheel::mouseUp (const MouseEvent&)
{
    dragging = false;
    speed.setValueAsPartOfGesture (0.f); touch.setValueAsPartOfGesture (0.f);
    speed.endGesture(); touch.endGesture();
}
void JogWheel::timerCallback()
{
    if (! dragging) return;
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = (float) jmax (0.001, (now - lastTime) * 0.001);
    float d = curAngle - lastAngle;
    while (d > MathConstants<float>::pi) d -= MathConstants<float>::twoPi;
    while (d < -MathConstants<float>::pi) d += MathConstants<float>::twoPi;
    shown += d;
    const float target = jlimit (-4.f, 4.f, d / MathConstants<float>::twoPi / dt / 0.5556f);   // 33 1/3 rpm = 等速
    speedSmoothed += (target - speedSmoothed) * 0.5f;
    speed.setValueAsPartOfGesture (speedSmoothed);
    lastAngle = curAngle; lastTime = now;
    repaint();
}
void JogWheel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.f);
    const auto c = r.getCentre();
    const float R = std::min (r.getWidth(), r.getHeight()) * 0.5f;
    auto circle = [&] (float rad) { return Rectangle<float> (c.x - rad, c.y - rad, rad * 2, rad * 2); };

    g.setColour (ui::well);    g.fillEllipse (circle (R));                       // 外側のベゼル
    g.setColour (ui::padEdge); g.drawEllipse (circle (R), 1.f);
    g.setColour (Colour (0xff0a1226)); g.fillEllipse (circle (R - 5.f));         // ジョグ面
    g.setColour (dragging ? ui::amber.withAlpha (0.8f) : Colour (0xff1c2a47));
    g.drawEllipse (circle (R - 5.f), 1.f);

    for (int i = 0; i < 40; ++i)                                                  // 回るくぼみ
    {
        const float a = shown + MathConstants<float>::twoPi * (float) i / 40.f;
        const float rr = R - 17.f;
        const Rectangle<float> dot (c.x + std::cos (a) * rr - 2.4f, c.y + std::sin (a) * rr - 2.4f, 4.8f, 4.8f);
        g.setColour (Colour (0xff060b1a)); g.fillEllipse (dot);
        g.setColour (i == 0 ? ui::amber : Colour (0xff22314f)); g.drawEllipse (dot, 1.f);
    }
    const float rp = R * 0.66f;                                                   // 内側のプラッター
    g.setColour (Colour (0xff0d1529)); g.fillEllipse (circle (rp));
    g.setColour (ui::padEdge);         g.drawEllipse (circle (rp), 1.f);

    const float ra = rp - 9.f;                                                    // 左右で色が違うアーク(回転に追従)
    for (int side = 0; side < 2; ++side)
    {
        Path arc;
        const float start = shown + (side ? -MathConstants<float>::halfPi : MathConstants<float>::halfPi) + 0.35f;
        arc.addCentredArc (c.x, c.y, ra, ra, 0.f, start, start + MathConstants<float>::pi - 0.7f, true);
        g.setColour ((side ? ui::amber : ui::teal).withAlpha (dragging ? 1.f : 0.8f));
        g.strokePath (arc, PathStrokeType (5.f, PathStrokeType::curved, PathStrokeType::butt));
    }
    const float rc = ra - 10.f;
    g.setColour (ui::panel); g.fillEllipse (circle (rc));
    g.setColour (ui::padEdge); g.drawEllipse (circle (rc), 1.f);
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    g.setFont (l.fontBold (0.95f)); g.setColour (ui::text);
    g.drawText ("SCRATCH", Rectangle<int> ((int) (c.x - rc), (int) (c.y - 14), (int) (rc * 2), 14), Justification::centred, false);
    g.setFont (l.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("DRAG", Rectangle<int> ((int) (c.x - rc), (int) (c.y + 2), (int) (rc * 2), 14), Justification::centred, false);
}

// ------------------------------------------------------------------ 液晶表示
static void setParamValue (AudioProcessorValueTreeState& apvts, const ParameterID& id, float v)
{
    auto* p = apvts.getParameter (id.getParamID());
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (v));
    p->endChangeGesture();
}

FxDisplay::FxDisplay (NamiDJFXProcessor& p) : proc (p) { startTimerHz (20); }

void FxDisplay::setBeatIndex (int idx)
{
    const int type = jlimit (0, djfx::kNumFx - 1, (int) proc.apvts.getRawParameterValue (ids::fxType.getParamID())->load());
    setParamValue (proc.apvts, ids::fxBeat, djfx::beatNormFromIndex (type, jlimit (0, djfx::beatListFor (type).n - 1, idx)));
    setParamValue (proc.apvts, ids::fxSync, 1.f);                        // 実機と同じ: BEATを選ぶと拍同期になる
}
void FxDisplay::stepBeat (int dir)
{
    const int type = jlimit (0, djfx::kNumFx - 1, (int) proc.apvts.getRawParameterValue (ids::fxType.getParamID())->load());
    const float norm = proc.apvts.getRawParameterValue (ids::fxBeat.getParamID())->load();
    if (type == djfx::kReverb) { setParamValue (proc.apvts, ids::fxBeat, jlimit (0.f, 1.f, norm + 0.1f * (float) dir)); return; }
    setBeatIndex (djfx::beatIndex (type, norm) + dir);
}
void FxDisplay::mouseDown (const MouseEvent& e)
{
    const int type = jlimit (0, djfx::kNumFx - 1, (int) proc.apvts.getRawParameterValue (ids::fxType.getParamID())->load());
    const auto p = e.position;
    if (p.y >= 22 && p.y < 68)                                           // 左右のセルで前後の拍へ
    {
        if (p.x >= 150 && p.x < 230) stepBeat (-1);
        else if (p.x >= 310 && p.x < 390) stepBeat (+1);
    }
    else if (p.y >= 76 && p.y < 104 && type != djfx::kReverb)            // 下の拍の一覧から直接選ぶ
    {
        const int n = djfx::beatListFor (type).n;
        const float cw = (float) (getWidth() - 16) / (float) n;
        setBeatIndex ((int) ((p.x - 8.f) / cw));
    }
}

void FxDisplay::paint (Graphics& g)
{
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    auto f = [&] (const ParameterID& id) { return proc.apvts.getRawParameterValue (id.getParamID())->load(); };
    const int type = jlimit (0, djfx::kNumFx - 1, (int) f (ids::fxType));
    const bool sync = f (ids::fxSync) > 0.5f, touch = f (ids::xpadTouch) > 0.5f, on = f (ids::fxOn) > 0.5f || touch;
    const float norm = touch ? f (ids::xpadPos) : f (ids::fxBeat);
    const float bpm = proc.lastBpm.load();
    const auto list = djfx::beatListFor (type);
    const int idx = djfx::beatIndex (type, norm);
    const double beats = std::fabs (list.v[idx]);
    const bool reverb = type == djfx::kReverb;
    const double ms = sync ? beats * 60000.0 / bpm : (touch ? (double) djfx::expMap (jlimit (0.f, 1.f, f (ids::xpadPos)), 10.f, 4000.f) : (double) f (ids::fxTime));

    auto r = getLocalBounds().toFloat();
    g.setColour (ui::well);    g.fillRoundedRectangle (r, 3.f);
    g.setColour (Colour (0xff02050d)); g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
    g.setColour (ui::padEdge); g.drawRoundedRectangle (r.reduced (1.5f), 2.f, 1.f);

    // 上段: 同期の表示とBPM
    const Colour tagCol = sync ? ui::teal : ui::dim;
    g.setColour (tagCol);
    g.drawRoundedRectangle (8.f, 6.f, 46.f, 14.f, 2.f, 1.f);
    g.setFont (l.font (0.8f));
    g.drawText (sync ? "SYNC" : "FREE", 8, 6, 46, 14, Justification::centred, false);
    g.setFont (l.fontBold (1.2f)); g.setColour (ui::text);
    g.drawText (String (bpm, 1), getWidth() - 110, 4, 76, 18, Justification::right, false);
    g.setFont (l.font (0.78f)); g.setColour (ui::dim);
    g.drawText ("BPM", getWidth() - 32, 8, 26, 12, Justification::left, false);

    // 左: エフェクト名とTIME
    g.setFont (l.fontBold (1.5f));
    g.setColour (on ? ui::text : ui::text.withAlpha (0.55f));
    g.drawText (djfx::fxName (type), 8, 24, 150, 26, Justification::left, false);
    g.setFont (l.font (0.78f)); g.setColour (ui::dim);
    g.drawText ("TIME", 8, 54, 36, 12, Justification::left, false);
    g.setFont (l.fontBold (1.1f)); g.setColour (ui::text);
    g.drawText (String ((int) std::lround (ms)), 40, 51, 52, 16, Justification::right, false);
    g.setFont (l.font (0.78f)); g.setColour (ui::dim);
    g.drawText ("msec", 96, 54, 40, 12, Justification::left, false);

    // 右: 前の拍 / 今の拍 / 次の拍
    auto cellText = [&] (int i) -> String
    {
        if (reverb) { const int pc = (int) std::lround (jlimit (0.f, 1.f, norm + 0.1f * (float) (i - idx)) * 99.f + 1.f); return String (pc) + "%"; }
        if (i < 0 || i >= list.n) return {};
        return String (list.label[i]);
    };
    const int curIdx = reverb ? 0 : idx;
    const float dimA = sync || reverb ? 1.f : 0.4f;
    for (int k = -1; k <= 1; ++k)
    {
        Rectangle<float> cell (150.f + (float) (k + 1) * 80.f, 22.f, 78.f, 46.f);
        const String t = reverb ? cellText (curIdx + k) : cellText (curIdx + k);
        if (t.isEmpty() && k != 0) { g.setColour (Colour (0xff0b1224)); g.fillRect (cell); continue; }
        if (k == 0)
        {
            g.setColour (ui::text.withAlpha (dimA)); g.fillRect (cell);
            g.setFont (l.fontBold (1.9f)); g.setColour (ui::ink);
        }
        else
        {
            g.setColour (Colour (0xff111b36)); g.fillRect (cell);
            g.setFont (l.fontBold (1.5f)); g.setColour (ui::text.withAlpha (0.8f * dimA));
        }
        g.drawText (t, cell.toNearestInt(), Justification::centred, false);
    }

    // 下段: 拍の一覧(選択中を白抜き)とBEATの帯
    if (! reverb)
    {
        const float cw = (float) (getWidth() - 16) / (float) list.n;
        g.setFont (l.font (0.82f));
        for (int i = 0; i < list.n; ++i)
        {
            Rectangle<float> chip (8.f + cw * (float) i, 76.f, cw - 2.f, 22.f);
            if (i == idx) { g.setColour (ui::text.withAlpha (dimA)); g.fillRect (chip); g.setColour (ui::ink); }
            else { g.setColour (Colour (0xff0f1930)); g.fillRect (chip); g.setColour (ui::text.withAlpha (0.7f * dimA)); }
            g.drawText (list.label[i], chip.toNearestInt(), Justification::centred, false);
        }
    }
    g.setColour (ui::teal.withAlpha (on ? 1.f : 0.5f));
    g.fillRect (8.f, 102.f, (float) getWidth() - 16.f, 6.f);
}

// ------------------------------------------------------------------ エフェクト選択ロータリー
FxSelector::FxSelector (AudioProcessorValueTreeState& s) : apvts (s) {}

float FxSelector::angleOf (int i) const   // 12時を0とした時計回りの角度。DELAYが左下、MOBIUSが右下
{
    return (-140.f + (float) i * (280.f / (float) (djfx::kNumFx - 1))) * MathConstants<float>::pi / 180.f;
}
Rectangle<float> FxSelector::labelRect (int i) const
{
    const float cx = (float) getWidth() * 0.5f, cy = (float) getHeight() * 0.5f;
    const bool left = i < 7;
    const int k = left ? i : i - 7;
    const float dy = left ? 44.f - (float) k * 15.f : -46.f + (float) k * 15.f;
    const float reach = 5.f + 40.f * std::sqrt (std::max (0.f, 1.f - (dy / 62.f) * (dy / 62.f)));
    const float w = 104.f;
    return left ? Rectangle<float> (cx - reach - w, cy + dy - 7.f, w, 14.f)
                : Rectangle<float> (cx + reach, cy + dy - 7.f, w, 14.f);
}
void FxSelector::select (int i)
{
    setParamValue (apvts, ids::fxType, (float) jlimit (0, djfx::kNumFx - 1, i));
    repaint();
}
void FxSelector::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < djfx::kNumFx; ++i)
        if (labelRect (i).expanded (2.f).contains (e.position)) { select (i); return; }
    dragStartIndex = (int) apvts.getRawParameterValue (ids::fxType.getParamID())->load();
}
void FxSelector::mouseDrag (const MouseEvent& e)                        // つまみを上下にドラッグして回す
{
    if (e.mouseDownPosition.getDistanceFrom ({ (float) getWidth() * 0.5f, (float) getHeight() * 0.5f }) < 30.f)
        select (dragStartIndex - (int) std::lround ((float) e.getDistanceFromDragStartY() / 14.f));
}
void FxSelector::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    const int cur = (int) apvts.getRawParameterValue (ids::fxType.getParamID())->load();
    select (cur + (w.deltaY > 0 ? -1 : 1));
}
void FxSelector::paint (Graphics& g)
{
    auto& l = dynamic_cast<NamiLnF&> (getLookAndFeel());
    const int cur = jlimit (0, djfx::kNumFx - 1, (int) apvts.getRawParameterValue (ids::fxType.getParamID())->load());
    const Point<float> c ((float) getWidth() * 0.5f, (float) getHeight() * 0.5f);

    for (int i = 0; i < djfx::kNumFx; ++i)                              // つまみから各名前へ伸びる短い目盛り
    {
        const float a = angleOf (i);
        g.setColour (i == cur ? ui::amber : ui::dim);
        g.drawLine (c.x + std::sin (a) * 30.f, c.y - std::cos (a) * 30.f, c.x + std::sin (a) * 35.f, c.y - std::cos (a) * 35.f, i == cur ? 2.f : 1.f);
    }
    for (int i = 0; i < djfx::kNumFx; ++i)
    {
        const auto r = labelRect (i);
        g.setFont (i == cur ? l.fontBold (0.92f) : l.font (0.92f));
        g.setColour (i == cur ? ui::amber : ui::text.withAlpha (0.85f));
        g.drawText (djfx::fxName (i), r.toNearestInt(), i < 7 ? Justification::centredRight : Justification::centredLeft, false);
    }
    drawCap (g, c, 23.f, angleOf (cur), true, false);
}

// ------------------------------------------------------------------ Editor
static String pct (float v) { return String ((int) std::lround (v * 100.f)) + "%"; }

TextButton* NamiDJFXEditor::addToggle (const ParameterID& id, const String& label, Colour accent)
{
    auto* b = toggles.add (new TextButton (label));
    b->setClickingTogglesState (true);
    b->getProperties().set ("accent", (int) accent.getARGB());
    btnAtts.add (new ButtonParameterAttachment (*proc.apvts.getParameter (id.getParamID()), *b));
    addAndMakeVisible (b);
    return b;
}
HoldButton* NamiDJFXEditor::addHold (const ParameterID& id, const String& label)
{
    auto* b = holds.add (new HoldButton());
    b->setButtonText (label);
    const String pid = id.getParamID();
    b->onHold = [this, pid] (bool down)
    {
        auto* p = proc.apvts.getParameter (pid);
        if (down) p->beginChangeGesture();
        p->setValueNotifyingHost (down ? 1.f : 0.f);
        if (! down) p->endChangeGesture();
    };
    addAndMakeVisible (b);
    return b;
}

NamiDJFXEditor::NamiDJFXEditor (NamiDJFXProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    StringArray colNames;
    for (int i = 1; i < djfx::kNumColor; ++i) colNames.add (djfx::colorName (i));      // OFFは押し直しで切る

    fxSel = std::make_unique<FxSelector> (s);
    colorTypes = std::make_unique<ChoiceGroup> (s, ids::colorType, colNames, 2, 4, false, 1, true);
    latency = std::make_unique<ChoiceGroup> (s, ids::jogLatency, StringArray { "0", "60", "125", "250", "500" }, 5, 3);
    xpad = std::make_unique<XPad> (s);
    jog = std::make_unique<JogWheel> (s);
    lcd = std::make_unique<FxDisplay> (proc);
    for (auto* c : { (Component*) fxSel.get(), (Component*) colorTypes.get(), (Component*) latency.get(),
                     (Component*) xpad.get(), (Component*) jog.get(), (Component*) lcd.get() })
        addAndMakeVisible (c);

    bOn = addToggle (ids::fxOn, "ON/OFF", ui::amber);
    bOn->getProperties().set ("round", true);
    bSync = addToggle (ids::fxSync, "BPM SYNC");
    bLock = addToggle (ids::centerLock, "CENTER LOCK", ui::amber);
    bLow = addToggle (ids::bandLow, "LOW");
    bMid = addToggle (ids::bandMid, "MID");
    bHi  = addToggle (ids::bandHi,  "HI");
    for (auto* b : { bLow, bMid, bHi }) b->getProperties().set ("bold", true);

    bPrev = new TextButton(); bNext = new TextButton(); bTap = new TextButton ("TAP");
    for (auto* b : { bPrev, bNext, bTap }) { toggles.add (b); addAndMakeVisible (b); }
    bPrev->getProperties().set ("arrow", -1); bNext->getProperties().set ("arrow", 1);
    bPrev->getProperties().set ("bold", true); bNext->getProperties().set ("bold", true);
    bTap->getProperties().set ("round", true);
    bPrev->onClick = [this] { lcd->stepBeat (-1); };
    bNext->onClick = [this] { lcd->stepBeat (+1); };
    bTap->onClick = [this]                                              // タップテンポ(ホストのBPMが無いときの手入力BPMに入る)
    {
        const double now = Time::getMillisecondCounterHiRes();
        if (! tapTimes.empty() && now - tapTimes.back() > 2000.0) tapTimes.clear();
        tapTimes.push_back (now);
        if (tapTimes.size() > 5) tapTimes.erase (tapTimes.begin());
        if (tapTimes.size() >= 2)
        {
            const double avg = (tapTimes.back() - tapTimes.front()) / (double) (tapTimes.size() - 1);
            setParamValue (proc.apvts, ids::bpm, (float) jlimit (30.0, 300.0, 60000.0 / avg));
            setParamValue (proc.apvts, ids::fxSync, 1.f);
        }
    };

    hBack = addHold (ids::jogBackspin, "BACKSPIN");
    hBrake = addHold (ids::jogBrake, "BRAKE");
    hRev = addHold (ids::jogReverse, "REVERSE");
    hLoop = addHold (ids::jogLoop, "LOOP");
    for (auto* h : { hBack, hBrake, hRev, hLoop }) h->getProperties().set ("accent", (int) ui::amber.getARGB());

    auto mk = [&] (const ParameterID& id, const String& name, std::function<String()> vt, const String& mn, const String& mx, bool bipolar = false, bool stepped = false, bool knurl = false)
    {
        auto k = std::make_unique<Knob> (s, id, name, std::move (vt), bipolar, stepped);
        k->minLabel = mn; k->maxLabel = mx;
        k->slider.getProperties().set ("knurl", knurl);
        addAndMakeVisible (*k);
        return k;
    };
    kTime = mk (ids::fxTime, "TIME", [this] { return String ((int) raw (ids::fxTime)) + "ms"; }, "1", "4000");
    kTime->slider.onDragStart = [this] { setParamValue (proc.apvts, ids::fxSync, 0.f); };   // TIMEを回すと自由な時間(ms)になる
    kLevel = mk (ids::fxLevel, "LEVEL/DEPTH", [this] { return pct (raw (ids::fxLevel)); }, "MIN", "MAX");
    kColor = mk (ids::colorKnob, "COLOR", [this]
    {
        const float c = raw (ids::colorKnob);
        return std::fabs (c) < 0.02f ? String ("CENTER") : String (c < 0 ? "L " : "R ") + String ((int) std::lround (std::fabs (c) * 100.f));
    }, "LOW", "HI", true, false, true);
    kParam = mk (ids::colorParam, "PARAMETER", [this] { return pct (raw (ids::colorParam)); }, "MIN", "MAX");
    kBs = mk (ids::bsTime, "SPIN TIME", [this] { return String ((int) raw (ids::bsTime)) + "ms"; }, "0.2s", "4s");
    kRate = mk (ids::bsRate, "SPIN RATE", [this] { return "x" + String (raw (ids::bsRate), 1); }, "x1", "x10");
    kBrake = mk (ids::brakeTime, "BRAKE TIME", [this] { return String ((int) raw (ids::brakeTime)) + "ms"; }, "50", "5s");
    kLoop = mk (ids::loopBeat, "LOOP BEAT", [this] { return String (djfx::beatLabel (djfx::kEcho, raw (ids::loopBeat))); }, "1/16", "16", false, true);
    kLoop->slider.stepCount = [] { return djfx::beatListFor (djfx::kEcho).n; };
    kBpm = mk (ids::bpm, "BPM (NO HOST)", [this] { return String ((int) raw (ids::bpm)); }, "30", "300");
    kOut = mk (ids::outGain, "OUT", [this] { return String (raw (ids::outGain), 1) + "dB"; }, "-24", "+6");

    setSize (1000, 640);
    startTimerHz (30);
}
NamiDJFXEditor::~NamiDJFXEditor() { setLookAndFeel (nullptr); }

void NamiDJFXEditor::timerCallback()
{
    const bool sync = raw (ids::fxSync) > 0.5f;
    kTime->slider.setAlpha (sync ? 0.55f : 1.f);
    for (auto* k : { kTime.get(), kLevel.get(), kColor.get(), kParam.get(), kBs.get(), kRate.get(), kBrake.get(), kLoop.get(), kBpm.get(), kOut.get() })
        k->repaint();
    fxSel->repaint();
    hBack->setToggleState (raw (ids::jogBackspin) > 0.5f, dontSendNotification);
    hBrake->setToggleState (raw (ids::jogBrake) > 0.5f, dontSendNotification);
    hRev->setToggleState (raw (ids::jogReverse) > 0.5f, dontSendNotification);
    hLoop->setToggleState (raw (ids::jogLoop) > 0.5f, dontSendNotification);
}

// 面板の金属テクスチャ(粒状のノイズ + 横方向のヘアライン)。毎回同じ模様になるよう乱数は固定。
void NamiDJFXEditor::buildFace (int w, int h)
{
    faceImage = Image (Image::RGB, w, h, false);
    Image::BitmapData bd (faceImage, Image::BitmapData::writeOnly);
    Random rnd (20261122);
    const float br = ui::face.getFloatRed() * 255.f, bg_ = ui::face.getFloatGreen() * 255.f, bb = ui::face.getFloatBlue() * 255.f;
    float rowBias = 0.f;
    for (int y = 0; y < h; ++y)
    {
        rowBias = rowBias * 0.55f + (rnd.nextFloat() - 0.5f) * 3.2f;
        for (int x = 0; x < w; ++x)
        {
            const float n = (rnd.nextFloat() - 0.5f) * 3.6f + rowBias + std::sin ((float) x * 0.012f + (float) y * 0.05f) * 0.9f;
            auto cl = [] (float v) { return (uint8) jlimit (0.f, 255.f, v); };
            bd.setPixelColour (x, y, Colour (cl (br + n), cl (bg_ + n * 1.05f), cl (bb + n * 1.2f)));
        }
    }
}

void NamiDJFXEditor::sectionHeader (Graphics& g, Rectangle<int> r, const String& title)
{
    g.setFont (lnf.fontBold (0.95f));
    const int tw = (int) textWidth (lnf.fontBold (0.95f), title);
    const int cx = r.getCentreX(), y = r.getY() + 12;
    g.setColour (ui::text);
    g.drawText (title, cx - tw / 2 - 4, r.getY() + 5, tw + 8, 14, Justification::centred, false);
    for (int side = 0; side < 2; ++side)                      // 見出しの両側に刻印の線
    {
        const int x0 = side ? cx + tw / 2 + 12 : r.getX() + 6;
        const int x1 = side ? r.getRight() - 6 : cx - tw / 2 - 12;
        g.setColour (ui::line);         g.drawHorizontalLine (y, (float) x0, (float) x1);
        g.setColour (Colour (0x22ffffff)); g.drawHorizontalLine (y + 1, (float) x0, (float) x1);
    }
}

void NamiDJFXEditor::paint (Graphics& g)
{
    if (! faceImage.isValid() || faceImage.getWidth() != getWidth() || faceImage.getHeight() != getHeight())
        buildFace (getWidth(), getHeight());
    g.drawImageAt (faceImage, 0, 0);

    // 上の帯(タイトル)と下の帯
    g.setColour (ui::bg.withAlpha (0.55f)); g.fillRect (0, 0, getWidth(), 38); g.fillRect (0, getHeight() - 36, getWidth(), 36);
    g.setColour (ui::line); g.drawHorizontalLine (38, 0.f, (float) getWidth()); g.drawHorizontalLine (getHeight() - 36, 0.f, (float) getWidth());
    g.setColour (Colour (0x22ffffff)); g.drawHorizontalLine (39, 0.f, (float) getWidth());

    g.setColour (ui::text);
    g.setFont (lnf.fontBold (1.5f));
    g.drawText ("DJFX Standard", 40, 5, 320, 28, Justification::left, false);
    g.setFont (lnf.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("MIDI  C1 BACKSPIN / C#1 BRAKE / D1 REVERSE / D#1 SCRATCH / E1 LOOP / F1 BEAT FX", 330, 12, 630, 14, Justification::right, false);
    g.drawText ("BEAT FX x14   SOUND COLOR FX x6   JOG", 40, getHeight() - 26, 500, 14, Justification::left, false);
    g.drawText ("DJFX-STD", getWidth() - 140, getHeight() - 26, 100, 14, Justification::right, false);

    for (int dx : { 424, 644 })                                       // 区画の刻印線
    {
        g.setColour (Colour (0xff070c1c)); g.drawVerticalLine (dx, 50.f, 592.f);
        g.setColour (Colour (0x28ffffff)); g.drawVerticalLine (dx + 1, 50.f, 592.f);
    }
    sectionHeader (g, boxA, "BEAT FX");
    sectionHeader (g, boxB, "SOUND COLOR FX");
    sectionHeader (g, boxC, "JOG");

    g.setFont (lnf.font (0.9f)); g.setColour (ui::text);
    g.drawText ("FX FREQUENCY", boxA.getX() + 8, boxA.getY() + 229, boxA.getWidth() - 16, 14, Justification::centred, false);
    g.setColour (ui::text.withAlpha (0.85f));
    g.drawText ("BEAT", boxA.getX() + 8, boxA.getY() + 153, boxA.getWidth() - 16, 14, Justification::centred, false);
    g.setColour (ui::dim); g.setFont (lnf.font (0.85f));
    g.drawText ("LOOKAHEAD ms", boxC.getX() + 8, boxC.getY() + 406, 160, 14, Justification::left, false);
    for (int dx : { 0, 1 }) { g.setColour (dx ? Colour (0x22ffffff) : ui::line); g.drawHorizontalLine (boxA.getY() + 151 + dx, (float) boxA.getX() + 8.f, (float) boxA.getCentreX() - 28.f); g.drawHorizontalLine (boxA.getY() + 151 + dx, (float) boxA.getCentreX() + 28.f, (float) boxA.getRight() - 8.f); }

    for (auto pt : { Point<float> (18.f, 19.f), Point<float> ((float) getWidth() - 18.f, 19.f),
                     Point<float> (18.f, (float) getHeight() - 18.f), Point<float> ((float) getWidth() - 18.f, (float) getHeight() - 18.f) })
    {
        g.setColour (ui::well);   g.fillEllipse (pt.x - 7.f, pt.y - 7.f, 14.f, 14.f);
        g.setColour (Colour (0xff223052)); g.fillEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f);
        g.setColour (ui::padEdge); g.drawEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f, 1.f);
        g.setColour (ui::well);   g.drawLine (pt.x - 3.5f, pt.y + 1.5f, pt.x + 3.5f, pt.y - 1.5f, 1.4f);
    }
}

void NamiDJFXEditor::resized()
{
    boxA = { 8, 46, 412, 546 };
    boxB = { 430, 46, 208, 546 };
    boxC = { 650, 46, 342, 546 };
    auto at = [] (Rectangle<int> b, int x, int y, int w, int h) { return Rectangle<int> (b.getX() + x, b.getY() + y, w, h); };

    // BEAT FX(実機の並び: 液晶 → BEAT ◀▶ → TAP → FX FREQUENCY → 選択ロータリー → X-PAD → TIME / ON-OFF / LEVEL)
    lcd->setBounds (at (boxA, 8, 28, 396, 112));
    bPrev->setBounds (at (boxA, 8, 148, 120, 24));
    bNext->setBounds (at (boxA, 284, 148, 120, 24));
    bSync->setBounds (at (boxA, 8, 188, 104, 26));
    bTap->setBounds (at (boxA, 206 - 22, 178, 44, 44));
    bLow->setBounds (at (boxA, 44, 246, 96, 26));
    bMid->setBounds (at (boxA, 158, 246, 96, 26));
    bHi->setBounds (at (boxA, 272, 246, 96, 26));
    fxSel->setBounds (at (boxA, 8, 274, 396, 118));
    xpad->setBounds (at (boxA, 8, 398, 396, 28));
    kTime->setBounds (at (boxA, 8, 432, 124, 112));
    bOn->setBounds (at (boxA, 206 - 32, 456, 64, 64));
    kLevel->setBounds (at (boxA, 280, 432, 124, 112));

    // SOUND COLOR FX
    colorTypes->setBounds (at (boxB, 6, 30, 196, 3 * 30 + 2 * 4));
    kColor->setBounds (at (boxB, 14, 150, 180, 190));
    kParam->setBounds (at (boxB, 40, 352, 128, 130));
    bLock->setBounds (at (boxB, 6, 500, 196, 28));

    // JOG
    jog->setBounds (at (boxC, 61, 28, 220, 220));
    hBack->setBounds (at (boxC, 6, 262, 80, 34));
    hBrake->setBounds (at (boxC, 90, 262, 80, 34));
    hRev->setBounds (at (boxC, 174, 262, 80, 34));
    hLoop->setBounds (at (boxC, 258, 262, 78, 34));
    kBs->setBounds (at (boxC, 6, 304, 80, 98));
    kRate->setBounds (at (boxC, 90, 304, 80, 98));
    kBrake->setBounds (at (boxC, 174, 304, 80, 98));
    kLoop->setBounds (at (boxC, 258, 304, 78, 98));
    latency->setBounds (at (boxC, 6, 424, 330, 26));
    kBpm->setBounds (at (boxC, 40, 458, 120, 84));
    kOut->setBounds (at (boxC, 182, 458, 120, 84));
}
