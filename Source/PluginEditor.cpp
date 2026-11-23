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
    face = Typeface::createSystemTypefaceFor (BinaryData::RobotoRegular_ttf, (size_t) BinaryData::RobotoRegular_ttfSize);
    faceBold = face;                                                  // Robotoは1ウェイトだけ。強調は色と大きさで付ける
    faceJp = Typeface::createSystemTypefaceFor (BinaryData::NotoSansJPMediumsubset_ttf, (size_t) BinaryData::NotoSansJPMediumsubset_ttfSize);
    embedded = face != nullptr;
#endif
    setColour (Label::textColourId, ui::text);
}

Font NamiLnF::font (float scale) const
{
    if (face != nullptr) return Font (FontOptions (face).withHeight (13.f * scale));
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f * scale, Font::plain));
}
Font NamiLnF::fontJp (float scale) const
{
    if (faceJp != nullptr) return Font (FontOptions (faceJp).withHeight (13.f * scale));
    return font (scale);
}
Font NamiLnF::fontBold (float scale) const
{
    if (faceBold != nullptr) return Font (FontOptions (faceBold).withHeight (13.f * scale));
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), 11.f * scale, Font::bold));
}

// 点灯したボタンの縁からにじむ光(内側へ薄くなっていく)
static void innerGlow (Graphics& g, Rectangle<float> r, float corner, Colour col, float strength = 1.f)
{
    for (int i = 0; i < 7; ++i)
    {
        const float k = (float) i;
        g.setColour (col.withAlpha (strength * 0.34f * std::pow (1.f - k / 7.f, 2.2f)));
        g.drawRoundedRectangle (r.reduced (0.5f + k), std::max (1.f, corner - k * 0.5f), 1.4f);
    }
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
        if (lit) for (int i = 4; i >= 1; --i) { g.setColour (accent.withAlpha (0.10f)); g.fillEllipse (c.x - d / 2 - (float) i - 3.f, c.y - d / 2 - (float) i - 3.f, d + 2.f * (float) i + 6.f, d + 2.f * (float) i + 6.f); }
        g.setColour (lit ? accent : ui::line);
        g.fillEllipse (c.x - d / 2, c.y - d / 2, d, d);                 // リングのLED
        const float di = d - 7.f;
        g.setColour (ui::padOff);
        g.fillEllipse (c.x - di / 2, c.y - di / 2, di, di);
        g.setColour (ui::padEdge);
        g.drawEllipse (c.x - di / 2, c.y - di / 2, di, di, 1.f);
        return;
    }

    if ((bool) b.getProperties().getWithDefault ("slide", false))
    {
        // 実機のCENTER LOCK: OFF ○━ ON のスライドスイッチ + 点灯する小窓
        const float cy = r.getCentreY();
        g.setColour (on ? ui::amber : Colour (0xff4a2d0c));
        g.fillRoundedRectangle (r.getX() + 4.f, cy - 3.f, 14.f, 6.f, 1.5f);
        g.setColour (ui::padEdge); g.drawRoundedRectangle (r.getX() + 4.f, cy - 3.f, 14.f, 6.f, 1.5f, 1.f);
        auto tr = Rectangle<float> (r.getRight() - 40.f, cy - 6.f, 28.f, 12.f);
        g.setColour (ui::well); g.fillRoundedRectangle (tr, 6.f);
        g.setColour (ui::padEdge); g.drawRoundedRectangle (tr, 6.f, 1.f);
        const float kx = on ? tr.getRight() - 6.f : tr.getX() + 6.f;
        g.setColour (Colour (0xffd0d0d3)); g.fillEllipse (kx - 5.5f, cy - 5.5f, 11.f, 11.f);
        return;
    }
    if ((bool) b.getProperties().getWithDefault ("silver", false))
    {
        // 実機のSOUND COLOR FXボタン: 銀の縁取りの四角いパッド。選択中は中が点く
        r = r.reduced (1.f);
        g.setColour (Colour (0xffc4c4c8)); g.fillRoundedRectangle (r, 3.f);
        auto in = r.reduced (2.5f);
        g.setColour (down ? Colour (0xff070708) : ui::padOff); g.fillRoundedRectangle (in, 2.f);
        if (on) { g.setColour (accent.withAlpha (0.30f)); g.fillRoundedRectangle (in, 2.f); innerGlow (g, in, 2.f, accent, 1.3f); g.setColour (accent.withAlpha (0.9f)); g.drawRoundedRectangle (in, 2.f, 1.f); }
        return;
    }
    if ((bool) b.getProperties().getWithDefault ("cue", false))
    {
        // 実機のCUEボタン: 橙の塗りの角丸。押している間は明るく点く
        r = r.reduced (0.5f);
        const bool lit = on || down;
        g.setColour (lit ? Colour (0xffffa63a) : Colour (0xffc9731a)); g.fillRoundedRectangle (r, 3.f);
        g.setColour (Colour (0xff3a2208)); g.drawRoundedRectangle (r, 3.f, 1.f);
        return;
    }
    r = r.reduced (0.5f);
    g.setColour (down ? ui::panel : ui::padOff);
    g.fillRoundedRectangle (r, bold ? 3.f : 2.f);
    if (on) { g.setColour (accent.withAlpha (bold ? 0.22f : 0.14f)); g.fillRoundedRectangle (r, bold ? 3.f : 2.f); }
    if (bold)
    {
        if (on) innerGlow (g, r.reduced (1.f), 3.f, accent, 1.4f);
        g.setColour (on ? accent : ui::padEdge);
        g.drawRoundedRectangle (r, 3.f, 2.f);
        g.setColour (on ? accent.withAlpha (0.45f) : Colour (0xff1e1e20));
        g.drawRoundedRectangle (r.reduced (3.f), 2.f, 1.f);
    }
    else
    {
        if (on) innerGlow (g, r, 2.f, accent, 1.f);
        g.setColour (on ? accent : (hover ? ui::dim : ui::padEdge));
        g.drawRoundedRectangle (r, 2.f, 1.f);
    }
    if ((bool) b.getProperties().getWithDefault ("led", false))
    {
        const float cy = r.getCentreY();
        g.setColour (on ? accent : Colour (0xff2a2a2d));
        g.fillEllipse (r.getX() + 7.f, cy - 2.5f, 5.f, 5.f);
    }
}

void NamiLnF::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const Colour accent ((uint32) (int) b.getProperties().getWithDefault ("accent", (int) ui::teal.getARGB()));
    const bool on = b.getToggleState();
    const bool round = (bool) b.getProperties().getWithDefault ("round", false);
    const int arrow = (int) b.getProperties().getWithDefault ("arrow", 0);
    if ((bool) b.getProperties().getWithDefault ("slide", false))
    {
        auto a = b.getLocalBounds();
        g.setFont (fontBold (0.8f)); g.setColour (ui::text.withAlpha (0.85f));
        g.drawText (b.getButtonText(), a.withTrimmedLeft (22).withTrimmedRight (46), Justification::centredLeft, false);
        g.setFont (font (0.65f)); g.setColour (ui::dim);
        g.drawText ("OFF", a.getRight() - 62, a.getY(), 20, a.getHeight(), Justification::centredRight, false);
        g.drawText ("ON",  a.getRight() - 11, a.getY(), 16, a.getHeight(), Justification::centredLeft, false);
        return;
    }
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
    if ((bool) b.getProperties().getWithDefault ("cue", false)) g.setColour (Colour (0xff141414));
    auto area = b.getLocalBounds().reduced (2, 0);
    if ((bool) b.getProperties().getWithDefault ("led", false)) area.removeFromLeft (10);
    if (on && ! round && ! (bool) b.getProperties().getWithDefault ("cue", false))      // 文字もにじませる
    {
        g.setColour (accent.withAlpha (0.28f));
        for (auto o : { Point<int> (-1, 0), Point<int> (1, 0), Point<int> (0, -1), Point<int> (0, 1) })
            g.drawText (b.getButtonText(), area.translated (o.x, o.y), Justification::centred, false);
        g.setColour (on ? accent : ui::text.withAlpha (0.82f));
    }
    g.drawText (b.getButtonText(), area, Justification::centred, false);
}

// つまみのキャップ(立体風)。ring: 黒い胴に銀のリング(EQ/TRIMなど)、knurl: 銀の削り出し+溝付き(COLOR)。
// 光は左上から。影・グラデーション・ハイライトで丸みを出す。
static void softShadow (Graphics& g, Point<float> c, float r)
{
    for (int i = 4; i >= 1; --i)
    {
        const float k = (float) i;
        g.setColour (Colours::black.withAlpha (0.16f));
        g.fillEllipse (c.x - r - k + 1.5f, c.y - r - k + 2.8f, (r + k) * 2, (r + k) * 2);
    }
}
static void drawCap (Graphics& g, Point<float> c, float rBody, float angle, bool knurl, bool dimmed)
{
    softShadow (g, c, rBody);
    const float ca = std::sin (angle), sa = -std::cos (angle);       // ポインタの向き
    auto pointer = [&] (float r0, float r1, Colour col, float w)
    {
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawLine (c.x + ca * r0 + 1.f, c.y + sa * r0 + 1.6f, c.x + ca * r1 + 1.f, c.y + sa * r1 + 1.6f, w);
        g.setColour (col);
        g.drawLine (c.x + ca * r0, c.y + sa * r0, c.x + ca * r1, c.y + sa * r1, w);
    };

    if (knurl)
    {
        // 銀: 外周の溝つき胴 → 放射状のヘアラインの天面
        ColourGradient body (Colour (0xfff0f0f3), c.x - rBody * 0.7f, c.y - rBody * 0.8f, Colour (0xff6c6c72), c.x + rBody * 0.8f, c.y + rBody * 0.9f, false);
        g.setGradientFill (body); g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
        for (int i = 0; i < 72; ++i)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 72.f;
            const float lit = 0.5f + 0.5f * std::cos (a + 2.35f);        // 左上ほど明るい
            g.setColour (Colour::fromFloatRGBA (0.f, 0.f, 0.f, 0.22f + 0.28f * (1.f - lit)));
            g.drawLine (c.x + std::sin (a) * (rBody - 5.f), c.y - std::cos (a) * (rBody - 5.f),
                        c.x + std::sin (a) * (rBody - 0.4f), c.y - std::cos (a) * (rBody - 0.4f), 1.5f);
        }
        g.setColour (Colour (0xff4a4a4f)); g.drawEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2, 1.f);
        const float rt = rBody - 6.f;
        g.setColour (Colour (0xff2c2c30)); g.fillEllipse (c.x - rt - 1.f, c.y - rt - 1.f, rt * 2 + 2.f, rt * 2 + 2.f);   // 胴と天面の段差
        for (int i = 0; i < 180; ++i)                                     // 削り出しの放射状の艶(2か所の反射)
        {
            const float a = MathConstants<float>::twoPi * (float) i / 180.f;
            const float v = 0.60f + 0.32f * std::pow (std::fabs (std::cos (a - 0.75f)), 3.f) + 0.015f * std::sin ((float) i * 7.f);
            g.setColour (Colour::fromFloatRGBA (v, v, v * 1.02f, 1.f));
            Path w; w.startNewSubPath (c.x, c.y);
            w.lineTo (c.x + std::sin (a) * rt, c.y - std::cos (a) * rt);
            g.strokePath (w, PathStrokeType (2.2f));
        }
        g.setColour (Colour (0xff5a5a60)); g.drawEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2, 1.f);
        pointer (rBody * 0.18f, rt - 1.f, Colour (0xff111112), 3.2f);
        return;
    }

    // 黒い胴 + 銀のリング
    const float ringW = 2.6f;
    ColourGradient ring (Colour (0xfffafafb), c.x - rBody, c.y - rBody, Colour (0xff808086), c.x + rBody, c.y + rBody, false);
    g.setGradientFill (ring); g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2);
    if (dimmed) { g.setColour (Colour (0xaa000000)); g.fillEllipse (c.x - rBody, c.y - rBody, rBody * 2, rBody * 2); }
    const float rb = rBody - ringW;
    ColourGradient body (Colour (0xff3e3e42), c.x - rb * 0.55f, c.y - rb * 0.7f, Colour (0xff0d0d0e), c.x + rb * 0.9f, c.y + rb * 1.0f, true);
    g.setGradientFill (body); g.fillEllipse (c.x - rb, c.y - rb, rb * 2, rb * 2);
    const float rt = rb * 0.68f;                                       // 天面のくぼみ
    ColourGradient top (Colour (0xff202023), c.x - rt * 0.4f, c.y - rt * 0.6f, Colour (0xff3a3a3e), c.x + rt * 0.7f, c.y + rt * 0.9f, false);
    g.setGradientFill (top); g.fillEllipse (c.x - rt, c.y - rt, rt * 2, rt * 2);
    g.setColour (Colour (0x22ffffff)); g.drawEllipse (c.x - rb + 0.5f, c.y - rb + 0.5f, rb * 2 - 1.f, rb * 2 - 1.f, 1.f);   // 縁の反射
    pointer (rb * 0.14f, rBody - 3.f, dimmed ? ui::dim : ui::text, 3.f);
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
void Knob::enableCenterLock()
{
    // 実機: ロックONで速く回しても中央で止まる。手を離すと解除され、止まった側へ戻せば続けて回せる
    auto sideOf = [] (double v) { return v > 0.004 ? 1 : (v < -0.004 ? -1 : 0); };
    slider.onValueChange = [this, sideOf]
    {
        if (guard > 0 || ! slider.isMouseButtonDown() || ! (lockOn && lockOn())) { lastSide = sideOf (slider.getValue()); return; }
        const int side = sideOf (slider.getValue());
        if (locked)
        {
            if (side == lockSide) { locked = false; lastSide = side; return; }     // 来た側へ戻した
            ++guard; slider.setValue (0.0, sendNotificationSync); --guard;
            return;
        }
        if (lastSide != 0 && side == -lastSide)                                    // 中央をまたいだ
        {
            locked = true; lockSide = lastSide;
            ++guard; slider.setValue (0.0, sendNotificationSync); --guard;
            lastSide = 0;
            return;
        }
        lastSide = side;
    };
    slider.onDragEnd = [this, sideOf] { locked = false; lastSide = sideOf (slider.getValue()); };
}
void Knob::resized()
{
    const int d = std::min (getWidth(), getHeight() - 42);
    slider.setBounds ((getWidth() - d) / 2, 0, d, d);
    layoutEdit();
}
void Knob::layoutEdit()
{
    // [札][数字欄] 単位   札があるときは左に寄せて並べる
    const bool tag = tagText && tagText().isNotEmpty();
    const int fw = tag ? 36 : 44, uw = unit.isEmpty() ? 0 : 14, tw = tag ? 28 : 0;
    const int total = tw + fw + (uw > 0 ? uw + 2 : 0);
    const int x0 = (getWidth() - total) / 2 + tw;
    const Rectangle<int> r (x0, getHeight() - 29, fw, 15);
    if (edit.getBounds() != r) edit.setBounds (r);
}
void Knob::makeEditable (const String& unitText, std::function<String()> numText, std::function<void (double)> onTyped)
{
    unit = unitText; numberText = std::move (numText);
    edit.setEditable (false, true);                                    // ダブルクリックで入力
    edit.setJustificationType (Justification::centred);
    edit.setColour (Label::textColourId, ui::amber);
    edit.setColour (Label::backgroundColourId, ui::well);
    edit.setColour (Label::outlineColourId, ui::padEdge);
    edit.setColour (Label::textWhenEditingColourId, ui::text);
    edit.setColour (TextEditor::backgroundColourId, ui::well);
    edit.setColour (TextEditor::highlightColourId, ui::teal.withAlpha (0.4f));
    edit.setTooltip (TRANS ("double-click to type"));
    edit.onEditorShow = [this]
    {
        if (auto* ed = edit.getCurrentTextEditor()) { ed->setInputRestrictions (7, "0123456789."); ed->selectAll(); }
    };
    edit.onTextChange = [this, onTyped]
    {
        const double v = edit.getText().getDoubleValue();
        if (v > 0.0 && onTyped) onTyped (v);
        timerCallback();
    };
    addAndMakeVisible (edit);
    startTimerHz (15);
    timerCallback();
}
void Knob::timerCallback()
{
    if (numberText == nullptr || edit.isBeingEdited()) return;
    const String t = numberText();
    if (edit.getText() != t) edit.setText (t, dontSendNotification);
    layoutEdit();
    repaint();
    edit.setFont (dynamic_cast<NamiLnF*> (&getLookAndFeel()) != nullptr ? dynamic_cast<NamiLnF&> (getLookAndFeel()).font (0.9f) : Font (FontOptions (12.f)));
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
    if (numberText != nullptr)
    {
        const auto e = edit.getBounds();
        g.setFont (l.font (0.75f)); g.setColour (ui::dim);
        g.drawText (unit, e.getRight() + 2, e.getY(), 16, e.getHeight(), Justification::centredLeft, false);   // 単位は欄の外
        const String tag = tagText ? tagText() : String();
        if (tag.isNotEmpty())
        {
            auto tr = Rectangle<float> ((float) e.getX() - 28.f, (float) e.getY() + 1.f, 26.f, (float) e.getHeight() - 2.f);
            g.setColour (ui::teal.withAlpha (0.25f)); g.fillRoundedRectangle (tr, 2.f);
            g.setColour (ui::teal); g.drawRoundedRectangle (tr, 2.f, 1.f);
            g.setFont (l.fontBold (0.7f)); g.drawText (tag, tr.toNearestInt(), Justification::centred, false);
        }
    }
    else
        g.drawText (valueText ? valueText() : String(), 0, getHeight() - 28, getWidth(), 14, Justification::centred, false);
    g.setFont (l.font (0.9f));
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
            const int target = (offToggle && cur == valueOf (i)) ? 0 : valueOf (i);   // 点灯中をもう一度押すとOFF
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
    for (int i = 0; i < buttons.size(); ++i) buttons[i]->setToggleState (valueOf (i) == cur, dontSendNotification);
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
    g.setColour (Colour (0xff2a2a2d));                          // 面のドット格子
    for (int ix = 1; ix < 16; ++ix)
        for (int iy = 1; iy < 7; ++iy)
            g.fillRect (r.getWidth() * (float) ix / 16.f - 0.5f, r.getHeight() * (float) iy / 7.f - 0.5f, 1.5f, 1.5f);
    {   // 実機のX-PADの青い発光(下から立ち上がる)。触っている間は触れた位置が強く光る
        g.setColour (ui::teal.withAlpha (touching ? 0.22f : 0.12f)); g.fillRect (3.f, r.getHeight() - 9.f, r.getWidth() - 6.f, 6.f);
        g.setColour (ui::teal.withAlpha (0.8f)); g.fillRect (3.f, r.getHeight() - 4.f, r.getWidth() - 6.f, 1.5f);
        if (touching) { g.setColour (ui::teal.withAlpha (0.35f)); g.fillRect (x * r.getWidth() - 24.f, 3.f, 48.f, r.getHeight() - 6.f); }
    }
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

    // CDJ-3000風: 銀の外周リング / くぼみが並ぶ黒いリング / つやのある黒い盤 / 中央の色付きリング
    g.setColour (Colour (0xffb8b8bc)); g.fillEllipse (circle (R));
    g.setColour (Colour (0xff6a6a6f)); g.drawEllipse (circle (R), 1.f);
    g.setColour (dragging ? ui::amber : ui::teal.withAlpha (0.9f)); g.drawEllipse (circle (R - 7.f), 2.f);   // 周りのLEDリング
    g.setColour (Colour (0xff0b0b0c)); g.fillEllipse (circle (R - 6.f));
    const float rr = R - 17.f;
    for (int i = 0; i < 40; ++i)                                                  // 回るくぼみ
    {
        const float a = shown + MathConstants<float>::twoPi * (float) i / 40.f;
        const Rectangle<float> dot (c.x + std::cos (a) * rr - 3.2f, c.y + std::sin (a) * rr - 3.2f, 6.4f, 6.4f);
        g.setColour (Colour (0xff040405)); g.fillEllipse (dot);
        g.setColour (i == 0 ? ui::amber : Colour (0xff303033)); g.drawEllipse (dot, 1.f);
    }
    const float rp = R * 0.66f;                                                   // 内側の盤
    g.setColour (Colour (0xff050505)); g.fillEllipse (circle (rp));
    g.setColour (Colour (0xff2a2a2d)); g.drawEllipse (circle (rp), 1.f);
    for (int i = 1; i <= 3; ++i)                                                  // 盤のうっすらした同心円
    {
        g.setColour (Colour (0xff0e0e0f)); g.drawEllipse (circle (rp - 8.f * (float) i), 1.f);
    }

    const float ra = rp - 9.f;                                                    // 回転に追従する色付きアーク
    for (int side = 0; side < 2; ++side)
    {
        Path arc;
        const float start = shown + (side ? -MathConstants<float>::halfPi : MathConstants<float>::halfPi) + 0.35f;
        arc.addCentredArc (c.x, c.y, ra, ra, 0.f, start, start + MathConstants<float>::pi - 0.7f, true);
        g.setColour ((side ? ui::amber : ui::teal).withAlpha (dragging ? 1.f : 0.85f));
        g.strokePath (arc, PathStrokeType (3.5f, PathStrokeType::curved, PathStrokeType::butt));
    }
    const float rc = ra - 10.f;
    g.setColour (Colour (0xff0b0b0c)); g.fillEllipse (circle (rc));
    {   // 中央の色付きリング(CDJ-3000のジョグディスプレイ風)。回転に合わせて色が回る
        const Colour cols[4] = { Colour (0xffe0409a), Colour (0xff3d7bff), Colour (0xfff08a1c), Colour (0xff35c2b0) };
        for (int i = 0; i < 4; ++i)
        {
            Path seg; const float st = shown + MathConstants<float>::halfPi * (float) i;
            seg.addCentredArc (c.x, c.y, rc - 2.f, rc - 2.f, 0.f, st + 0.06f, st + MathConstants<float>::halfPi - 0.06f, true);
            g.setColour (cols[i]); g.strokePath (seg, PathStrokeType (3.f));
        }
    }
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
    g.setColour (Colour (0xff010101)); g.drawRoundedRectangle (r.reduced (0.5f), 3.f, 1.f);
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
        if (t.isEmpty() && k != 0) { g.setColour (Colour (0xff0e0e0f)); g.fillRect (cell); continue; }
        if (k == 0)
        {
            g.setColour (ui::text.withAlpha (dimA)); g.fillRect (cell);
            g.setFont (l.fontBold (1.9f)); g.setColour (ui::ink);
        }
        else
        {
            g.setColour (Colour (0xff181819)); g.fillRect (cell);
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
            else { g.setColour (Colour (0xff161617)); g.fillRect (chip); g.setColour (ui::text.withAlpha (0.7f * dimA)); }
            g.drawText (list.label[i], chip.toNearestInt(), Justification::centred, false);
        }
    }
    g.setColour (Colour (0xff6c4cff).withAlpha (on ? 1.f : 0.5f));
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
TextButton* NamiDJFXEditor::addAction (const String& label, std::function<void()> onClick)
{
    auto* b = toggles.add (new TextButton (label));
    b->onClick = std::move (onClick);
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

    // 実機の並び: SPACE DUB ECHO / CRUSH SWEEP / NOISE FILTER (パラメータ順とは違うので対応表を持つ)
    const int hwOrder[] = { djfx::kSpace, djfx::kDubEcho, djfx::kCrush, djfx::kSweep, djfx::kNoise, djfx::kColorFilter };
    StringArray colNames;
    for (int t : hwOrder) colNames.add (djfx::colorName (t));      // OFFは押し直しで切る

    fxSel = std::make_unique<FxSelector> (s);
    colorTypes = std::make_unique<ChoiceGroup> (s, ids::colorType, colNames, 2, 4, false, 1, true);
    for (int t : hwOrder) colorTypes->values.add (t);
    for (auto* b : colorTypes->getChildren()) b->getProperties().set ("silver", true);
    latency = std::make_unique<ChoiceGroup> (s, ids::jogLatency, StringArray { "0", "60", "125", "250", "500" }, 5, 3);
    xpad = std::make_unique<XPad> (s);
    jog = std::make_unique<JogWheel> (s);
    waveIn = std::make_unique<WaveStrip> (proc, false); waveOut = std::make_unique<WaveStrip> (proc, true);
    addAndMakeVisible (*waveIn); addAndMakeVisible (*waveOut);
    lcd = std::make_unique<FxDisplay> (proc);
    for (auto* c : { (Component*) fxSel.get(), (Component*) colorTypes.get(), (Component*) latency.get(),
                     (Component*) xpad.get(), (Component*) jog.get(), (Component*) lcd.get() })
        addAndMakeVisible (c);

    bOn = addToggle (ids::fxOn, "ON/OFF", ui::amber);
    bOn->getProperties().set ("round", true);
    bSync = addToggle (ids::fxSync, "BPM SYNC");
    bLock = addToggle (ids::centerLock, "CENTER LOCK", ui::amber);
    bLock->getProperties().set ("slide", true);
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
    hLive = addHold (ids::jogResync, "RESYNC");
    for (auto* hb : { hBack, hBrake, hRev, hLoop }) hb->getProperties().set ("cue", true);
    hLive->getProperties().set ("accent", (int) ui::teal.getARGB());
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
    kColor->lockOn = [this] { return raw (ids::centerLock) > 0.5f; };
    kColor->enableCenterLock();
    kParam = mk (ids::colorParam, "PARAMETER", [this] { return pct (raw (ids::colorParam)); }, "MIN", "MAX");
    kBs = mk (ids::bsTime, "SPIN TIME", [this] { return String ((int) raw (ids::bsTime)) + "ms"; }, "0.2s", "4s");
    kRate = mk (ids::bsRate, "SPIN RATE", [this] { return "x" + String (raw (ids::bsRate), 1); }, "x1", "x10");
    kRelease = mk (ids::releaseTime, "RELEASE", [this] { return String ((int) raw (ids::releaseTime)) + "ms"; }, "50", "5s");
    kBrake = mk (ids::brakeTime, "BRAKE TIME", [this] { return String ((int) raw (ids::brakeTime)) + "ms"; }, "50", "5s");
    kLoop = mk (ids::loopBeat, "LOOP BEAT", [this] { return String (djfx::beatLabel (djfx::kEcho, raw (ids::loopBeat))); }, "1/16", "16", false, true);
    kLoop->slider.stepCount = [] { return djfx::beatListFor (djfx::kEcho).n; };
    kBpm = mk (ids::bpm, "BPM (NO HOST)", [this] { return String ((int) raw (ids::bpm)); }, "30", "300");
    kInertia = mk (ids::inertia, "INERTIA", [this] { return raw (ids::inertia) < 1.f ? String ("OFF") : String ((int) raw (ids::inertia)) + "ms"; }, "OFF", "3s");
    kOut = mk (ids::outGain, "OUT", [this] { return String (raw (ids::outGain), 1) + "dB"; }, "-24", "+6");

    // ---- 数字入力欄(単位は外) と 右クリックの音符メニュー(その時のBPMでmsに直す)
    auto bpmNow = [this] { return (double) std::max (30.f, proc.lastBpm.load()); };
    auto noteIdx = [this] (const ParameterID& id) { return jlimit (0, kNumNotes - 1, (int) raw (id)); };
    auto noteMenu = [this, bpmNow, noteIdx] (Knob& k, const ParameterID& msId, const ParameterID& beatId)
    {
        k.slider.onDragStart = [this, beatId] { setParamValue (proc.apvts, beatId, 0.f); };   // 回したら自由なmsに戻る
        k.tagText = [noteIdx, beatId] { const int i = noteIdx (beatId); return i > 0 ? String (kNoteNames[i]) : String(); };
        k.makeEditable ("ms",
            [this, bpmNow, noteIdx, msId, beatId] { const int i = noteIdx (beatId); return String ((int) std::lround (i > 0 ? noteMs (i, bpmNow()) : raw (msId))); },
            [this, msId, beatId] (double v) { setParamValue (proc.apvts, beatId, 0.f); setParamValue (proc.apvts, msId, (float) v); });
        k.slider.onPopup = [this, &k, bpmNow, noteIdx, msId, beatId]
        {
            const double bpm = bpmNow();
            PopupMenu m;
            m.addSectionHeader ("BPM " + String (bpm, 1) + " で合わせる");
            for (int i = 1; i < kNumNotes; ++i)
                m.addItem (i, String (kNoteNames[i]).paddedRight (' ', 6) + "= " + String ((int) std::lround (noteMs (i, bpm))) + " ms", true, noteIdx (beatId) == i);
            m.addSeparator();
            m.addItem (100, "FREE (ms)", true, noteIdx (beatId) == 0);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (&k.slider),
                [this, msId, beatId, bpm] (int r)
                {
                    if (r <= 0) return;
                    if (r == 100) { setParamValue (proc.apvts, beatId, 0.f); return; }
                    auto* prm = proc.apvts.getParameter (msId.getParamID());
                    const auto rng = prm->getNormalisableRange();
                    setParamValue (proc.apvts, msId, jlimit (rng.start, rng.end, noteMs (r, bpm)));   // つまみも換算した位置へ
                    setParamValue (proc.apvts, beatId, (float) r);                                   // 以後BPMに追従
                });
        };
    };
    noteMenu (*kBs, ids::bsTime, ids::bsBeat);
    noteMenu (*kRelease, ids::releaseTime, ids::releaseBeat);
    noteMenu (*kBrake, ids::brakeTime, ids::brakeBeat);

    // BEAT FX の TIME: 欄で ms を打てる / 右クリックで拍を選ぶ(BEATボタンと同じく拍同期になる)
    kTime->makeEditable ("ms", [this] { return String ((int) raw (ids::fxTime)); },
                         [this] (double v) { setParamValue (proc.apvts, ids::fxSync, 0.f); setParamValue (proc.apvts, ids::fxTime, (float) v); });
    kTime->slider.onPopup = [this]
    {
        const int type = (int) raw (ids::fxType);
        const auto& bl = djfx::beatListFor (type);
        if (bl.n < 2) return;
        const double bpm = std::max (30.f, proc.lastBpm.load());
        PopupMenu m;
        m.addSectionHeader (String (djfx::fxName (type)) + "  BPM " + String (bpm, 1));
        for (int i = 0; i < bl.n; ++i)
        {
            const float nrm = djfx::beatNormFromIndex (type, i);
            m.addItem (i + 1, String (djfx::beatLabel (type, nrm)).paddedRight (' ', 6) + "= " + String ((int) std::lround (std::fabs (bl.v[i]) * 60000.0 / bpm)) + " ms");
        }
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&kTime->slider), [this, type] (int r)
        {
            if (r <= 0) return;
            setParamValue (proc.apvts, ids::fxBeat, djfx::beatNormFromIndex (type, r - 1));
            setParamValue (proc.apvts, ids::fxSync, 1.f);
        });
    };
    kInertia->makeEditable ("ms", [this] { return String ((int) raw (ids::inertia)); },
                           [this] (double v) { setParamValue (proc.apvts, ids::inertia, (float) jlimit (0.0, 3000.0, v)); });
    kBpm->makeEditable ("", [this] { return String ((int) std::lround (raw (ids::bpm))); },
                        [this] (double v) { setParamValue (proc.apvts, ids::bpm, (float) jlimit (30.0, 300.0, v)); });

    // ---- ループ操作(1/2X・2X・RELOOP/EXIT)、SLIP、QUANTIZE
    auto stepLoop = [this] (double mul)
    {
        const auto list = djfx::beatListFor (djfx::kEcho);
        const int cur = djfx::beatIndex (djfx::kEcho, raw (ids::loopBeat));
        const double target = list.v[cur] * mul;
        int best = cur; double bd = 1e9;
        for (int i = 0; i < list.n; ++i) { const double d = std::fabs (std::log (list.v[i] / target)); if (d < bd) { bd = d; best = i; } }
        setParamValue (proc.apvts, ids::loopBeat, djfx::beatNormFromIndex (djfx::kEcho, best));
    };
    bHalf = addAction ("1/2X", [stepLoop] { stepLoop (0.5); });
    bDouble = addAction ("2X", [stepLoop] { stepLoop (2.0); });
    bReloop = addToggle (ids::loopLatch, "RELOOP / EXIT", ui::amber);
    tSlip = addToggle (ids::slip, "SLIP", ui::amber);
    tQuant = addToggle (ids::quantize, "QUANTIZE", ui::amber);
    tStopQ = addToggle (ids::stopQuant, "STOP Q", ui::amber);
    tKeyLock = addToggle (ids::pitchLock, "KEY LOCK", ui::teal);
    bRec = addAction ("REC", [this] { proc.setRecording (! proc.recording.load()); });
    bRec->getProperties().set ("accent", (int) ui::amber.getARGB());
    bRec->getProperties().set ("bold", true);
    quantBeats = std::make_unique<ChoiceGroup> (proc.apvts, ids::quantBeat, StringArray { "1/8", "1/4", "1/2", "1" }, 4, 3);
    addAndMakeVisible (*quantBeats);

    setSize (1000, 640);
    startTimerHz (30);
}
NamiDJFXEditor::~NamiDJFXEditor() { setLookAndFeel (nullptr); }

void NamiDJFXEditor::timerCallback()
{
    const bool sync = raw (ids::fxSync) > 0.5f;
    kTime->slider.setAlpha (sync ? 0.55f : 1.f);
    for (auto* k : { kTime.get(), kLevel.get(), kColor.get(), kParam.get(), kBs.get(), kRate.get(), kBrake.get(), kRelease.get(), kInertia.get(), kLoop.get(), kBpm.get(), kOut.get() })
        k->repaint();
    fxSel->repaint();
    hBack->setToggleState (raw (ids::jogBackspin) > 0.5f, dontSendNotification);
    hBrake->setToggleState (raw (ids::jogBrake) > 0.5f, dontSendNotification);
    hRev->setToggleState (raw (ids::jogReverse) > 0.5f, dontSendNotification);
    hLoop->setToggleState (raw (ids::jogLoop) > 0.5f, dontSendNotification);
    hLive->setToggleState (raw (ids::jogResync) > 0.5f, dontSendNotification);
    bRec->setToggleState (proc.recording.load(), dontSendNotification);
    quantBeats->setAlpha (raw (ids::quantize) > 0.5f ? 1.f : 0.5f);
    repaint (boxC.withTrimmedTop (250));      // 遅れ表示
}

void WaveStrip::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (ui::well); g.fillRoundedRectangle (b, 3.f);
    g.setColour (ui::padEdge); g.drawRoundedRectangle (b.reduced (0.5f), 3.f, 1.f);
    const auto in = b.reduced (3.f, 14.f);
    const float cx = in.getCentreX();
    const auto& ring = out ? proc.waveOut : proc.waveIn;
    const int w = (out ? proc.waveWOut : proc.waveWIn).load (std::memory_order_acquire);
    const int rows = (int) in.getHeight();
    const float binsPerRow = (kSpanMs / 10.f) / (float) rows;
    const float lagMs = proc.jogLagMs.load();
    const float nearMs = proc.loopNearMs.load(), farMs = proc.loopFarMs.load();
    auto yOf = [&] (float ms) { return in.getY() + ms / kSpanMs * in.getHeight(); };

    if (! out && nearMs >= 0.f)                                        // ループ範囲
    {
        const float y0 = yOf (nearMs), y1 = yOf (farMs);
        g.setColour (ui::teal.withAlpha (0.18f));
        g.fillRect (in.getX(), std::max (in.getY(), y0), in.getWidth(), std::min (in.getBottom(), y1) - std::max (in.getY(), y0));
    }
    g.setColour (out ? ui::amber.withAlpha (0.85f) : ui::teal.withAlpha (0.85f));
    for (int r = 0; r < rows; ++r)
    {
        float pk = 0.f;
        const int b0 = (int) ((float) r * binsPerRow), b1 = std::max (b0 + 1, (int) ((float) (r + 1) * binsPerRow));
        for (int k = b0; k < b1; ++k)
        {
            const int idx = w - 1 - k;
            if (idx < 0) break;
            pk = std::max (pk, ring[idx & (NamiDJFXProcessor::kWaveSize - 1)].load (std::memory_order_relaxed));
        }
        const float half = std::min (1.f, std::sqrt (pk)) * (in.getWidth() * 0.5f - 1.f);
        if (half > 0.2f) g.drawHorizontalLine ((int) (in.getY() + (float) r), cx - half, cx + half);
    }
    if (! out)                                                         // 再生ヘッド(ライブからの遅れ)
    {
        const float y = std::min (in.getBottom(), yOf (lagMs));
        g.setColour (ui::amber); g.drawHorizontalLine ((int) y, in.getX(), in.getRight());
    }
    g.setColour (ui::dim); if (auto* l = dynamic_cast<NamiLnF*> (&getLookAndFeel())) g.setFont (l->font (0.8f)); else g.setFont (Font (11.f));
    g.drawText (out ? "OUT" : "IN", getLocalBounds().removeFromTop (14), Justification::centred, false);
    g.drawText ("-6s", getLocalBounds().removeFromBottom (14), Justification::centred, false);
}

// 面板の金属テクスチャ(粒状のノイズ + 横方向のヘアライン)。毎回同じ模様になるよう乱数は固定。
// 面板は単色(グラデーション・質感なし)
void NamiDJFXEditor::buildFace (int w, int h)
{
    faceImage = Image (Image::RGB, w, h, false);
    Graphics g (faceImage);
    g.fillAll (ui::face);
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
    }
}

void NamiDJFXEditor::paint (Graphics& g)
{
    if (! faceImage.isValid() || faceImage.getWidth() != getWidth() || faceImage.getHeight() != getHeight())
        buildFace (getWidth(), getHeight());
    g.drawImageAt (faceImage, 0, 0);

    // 上の帯(タイトル)と下の帯
    g.setColour (ui::bg); g.fillRect (0, 0, getWidth(), 38); g.fillRect (0, getHeight() - 36, getWidth(), 36);
    g.setColour (ui::line); g.drawHorizontalLine (38, 0.f, (float) getWidth()); g.drawHorizontalLine (getHeight() - 36, 0.f, (float) getWidth());

    g.setColour (ui::text);
    g.setFont (lnf.fontBold (1.5f));
    g.drawText ("DJFX Standard", 40, 5, 320, 28, Justification::left, false);
    g.setFont (lnf.font (0.85f)); g.setColour (ui::dim);
    g.drawText ("MIDI  C1 BACKSPIN / C#1 BRAKE / D1 REVERSE / D#1 SCRATCH / E1 LOOP / F1 BEAT FX / F#1 RESYNC / G1 REC", 330, 12, 630, 14, Justification::right, false);
    g.drawText ("BEAT FX x14   SOUND COLOR FX x6   JOG", 40, getHeight() - 26, 500, 14, Justification::left, false);
    g.drawText ("DJFX-STD", getWidth() - 140, getHeight() - 26, 100, 14, Justification::right, false);

    for (int dx : { 424, 644 })                                       // 区画の刻印線
    {
        g.setColour (ui::line); g.drawVerticalLine (dx, 50.f, 592.f);
    }
    sectionHeader (g, boxA, "BEAT FX");
    sectionHeader (g, boxB, "SOUND COLOR FX");
    sectionHeader (g, boxC, "JOG");

    g.setFont (lnf.font (0.9f)); g.setColour (ui::text);
    g.drawText ("FX FREQUENCY", boxA.getX() + 8, boxA.getY() + 229, boxA.getWidth() - 16, 14, Justification::centred, false);
    g.setColour (ui::text.withAlpha (0.85f));
    g.drawText ("BEAT", boxA.getX() + 8, boxA.getY() + 153, boxA.getWidth() - 16, 14, Justification::centred, false);
    g.setColour (ui::dim); g.setFont (lnf.font (0.85f));
    g.drawText ("LOOKAHEAD ms", boxC.getX() + 8, boxC.getY() + 515, 160, 14, Justification::left, false);
    {   // SLIP OFFでライブより遅れている長さ(DAWの拍に直して表示)
        const float lagMs = proc.jogLagMs.load();
        const double beatMs = 60000.0 / std::max (30.f, proc.lastBpm.load());
        const int lx = boxC.getX() + 258, ly = boxC.getY() + 304;
        g.setFont (lnf.font (0.85f)); g.setColour (lagMs < 1.f ? ui::dim : ui::amber);
        g.drawText (lagMs < 1.f ? String ("LAG LIVE") : "LAG " + String ((int) lagMs) + "ms", lx, ly, 78, 14, Justification::centred, false);
        g.setFont (lnf.font (0.75f)); g.setColour (ui::dim);
        g.drawText (lagMs < 1.f ? String() : String (lagMs / beatMs, 2) + " beat", lx, ly + 14, 78, 12, Justification::centred, false);
    }
    for (int dx : { 0 }) { g.setColour (ui::line); g.drawHorizontalLine (boxA.getY() + 151 + dx, (float) boxA.getX() + 8.f, (float) boxA.getCentreX() - 28.f); g.drawHorizontalLine (boxA.getY() + 151 + dx, (float) boxA.getCentreX() + 28.f, (float) boxA.getRight() - 8.f); }

    for (auto pt : { Point<float> (18.f, 19.f), Point<float> ((float) getWidth() - 18.f, 19.f),
                     Point<float> (18.f, (float) getHeight() - 18.f), Point<float> ((float) getWidth() - 18.f, (float) getHeight() - 18.f) })
    {
        g.setColour (ui::well);   g.fillEllipse (pt.x - 7.f, pt.y - 7.f, 14.f, 14.f);
        g.setColour (Colour (0xff2e2e31)); g.fillEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f);
        g.setColour (ui::padEdge); g.drawEllipse (pt.x - 5.f, pt.y - 5.f, 10.f, 10.f, 1.f);
        g.setColour (ui::well);   g.drawLine (pt.x - 3.5f, pt.y + 1.5f, pt.x + 3.5f, pt.y - 1.5f, 1.4f);
    }
}

void NamiDJFXEditor::resized()
{
    boxA = { 8, 46, 412, 546 };
    boxB = { 430, 46, 208, 546 };
    boxC = { 650, 46, 342, 556 };
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

    bRec->setBounds (190, 9, 74, 22);

    // JOG
    jog->setBounds (at (boxC, 76, 26, 190, 190));
    waveIn->setBounds (at (boxC, 6, 26, 62, 190));
    waveOut->setBounds (at (boxC, 274, 26, 62, 190));
    hBack->setBounds (at (boxC, 6, 222, 80, 28));
    hBrake->setBounds (at (boxC, 90, 222, 80, 28));
    hRev->setBounds (at (boxC, 174, 222, 80, 28));
    hLoop->setBounds (at (boxC, 258, 222, 78, 28));
    bHalf->setBounds (at (boxC, 6, 254, 80, 22));
    bDouble->setBounds (at (boxC, 90, 254, 80, 22));
    bReloop->setBounds (at (boxC, 174, 254, 162, 22));
    tSlip->setBounds (at (boxC, 6, 280, 80, 22));
    tQuant->setBounds (at (boxC, 90, 280, 80, 22));
    quantBeats->setBounds (at (boxC, 174, 280, 162, 22));
    tStopQ->setBounds (at (boxC, 6, 306, 80, 22));
    tKeyLock->setBounds (at (boxC, 90, 306, 80, 22));
    hLive->setBounds (at (boxC, 174, 306, 80, 22));
    kBs->setBounds (at (boxC, 6, 334, 80, 96));
    kRate->setBounds (at (boxC, 90, 334, 80, 96));
    kBrake->setBounds (at (boxC, 174, 334, 80, 96));
    kRelease->setBounds (at (boxC, 258, 334, 80, 96));
    kLoop->setBounds (at (boxC, 6, 432, 80, 80));
    kBpm->setBounds (at (boxC, 90, 432, 80, 80));
    kOut->setBounds (at (boxC, 174, 432, 80, 80));
    kInertia->setBounds (at (boxC, 258, 432, 80, 80));
    latency->setBounds (at (boxC, 6, 530, 330, 24));
}
