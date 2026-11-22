#pragma once
#include "Common.h"
#include "Reverb.h"

namespace djfx
{
enum ColorType { kColorOff = 0, kSpace, kDubEcho, kSweep, kNoise, kCrush, kColorFilter, kNumColor };

// SOUND COLOR FX。COLORノブ(-1..1,中央=バイパス)とPARAMETERノブ(0..1)の2つで操作する。
class ColorFx
{
public:
    void prepare (float sr_)
    {
        sr = sr_;
        rv.prepare (sr);
        for (auto& d : dub) d.prepare ((int) (2.2f * sr));
        sc.setTime (25.f, sr); sp.setTime (25.f, sr); dts.setTime (60.f, sr);
        reset();
    }
    void reset()
    {
        rv.clear();
        for (auto& d : dub) d.clear();
        for (auto& f : s1) f.reset();
        for (auto& f : s2) f.reset();
        for (auto& f : s3) f.reset();
        for (auto& f : s4) f.reset();
        hold[0] = hold[1] = 0.f; holdCnt = 0.f;
        dInit = false;
    }

    void process (float* L, float* R, int n, int type, float colorKnob, float param, double bpm, bool centerLock)
    {
        if (type != lastType) { reset(); lastType = type; sc.reset (colorKnob); sp.reset (param); }
        if (type == kColorOff) return;

        const float dz = centerLock ? 0.07f : 0.015f;
        float* io[2] = { L, R };
        const float beatSamples = clampf ((float) (60.0 / std::max (bpm, 30.0)) * sr, 64.f, 2.f * sr);
        if (! dInit) { dts.reset (beatSamples); dInit = true; }

        // 固定カットオフのフィルタ(ブロック単位)
        if (type == kSpace) { for (int c = 0; c < 2; ++c) { s1[c].set (3000.f, 0.7f, sr); s2[c].set (350.f, 0.7f, sr); } }
        if (type == kDubEcho) { for (int c = 0; c < 2; ++c) { s1[c].set (3000.f, 0.7f, sr); s2[c].set (300.f, 0.7f, sr); s3[c].set (2000.f, 0.7f, sr); } }

        for (int i = 0; i < n; ++i)
        {
            float c = sc.tick (colorKnob);
            const float p = sp.tick (param);
            const float ac = std::fabs (c);
            c = ac < dz ? 0.f : (c < 0.f ? -1.f : 1.f) * (ac - dz) / (1.f - dz);
            const float a = std::fabs (c);
            const float x[2] = { io[0][i], io[1][i] };
            float y[2] = { x[0], x[1] };

            switch (type)
            {
                case kSpace:
                {
                    rv.setParams (p, 0.3f);
                    float wl, wr; rv.tick (0.5f * (x[0] + x[1]), wl, wr);
                    float w[2] = { wl, wr };
                    const float amt = smooth01 (a * 4.f);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float f = c < 0.f ? s1[ch].lp (w[ch]) : s2[ch].hp (w[ch]);
                        y[ch] = x[ch] + f * amt * 1.5f;
                    }
                    break;
                }
                case kDubEcho:
                {
                    const float d = dts.tick (beatSamples);
                    const float fb = 0.2f + 0.7f * p;
                    const float amt = smooth01 (a * 4.f);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float e = dub[ch].read (d);
                        float f = c < 0.f ? s1[ch].lp (s2[ch].hp (e)) : s3[ch].hp (e);
                        dub[ch].push (x[ch] * 0.8f + std::tanh (f * fb));
                        y[ch] = x[ch] + f * amt;
                    }
                    break;
                }
                case kSweep:
                {
                    const float fc = expMap (p, 150.f, 10000.f);
                    const float amt = smooth01 (a * 6.f);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float lp, bp, hp;
                        if (c < 0.f)
                        {
                            s1[ch].set (fc, lerpf (6.f, 0.35f, a), sr);
                            s1[ch].tick (x[ch], lp, bp, hp);
                            y[ch] = lerpf (x[ch], x[ch] - s1[ch].k * bp, amt);   // ノッチ
                        }
                        else
                        {
                            s1[ch].set (fc, lerpf (0.6f, 8.f, a), sr);
                            s1[ch].tick (x[ch], lp, bp, hp);
                            y[ch] = lerpf (x[ch], bp * s1[ch].k, amt);           // バンドパス
                        }
                    }
                    break;
                }
                case kNoise:
                {
                    const float g = p * p * 1.2f;
                    const float fc = expMap (c * 0.5f + 0.5f, 80.f, 14000.f);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        s1[ch].set (fc, 1.2f, sr);
                        float lp, bp, hp;
                        s1[ch].tick (rng[ch].white(), lp, bp, hp);
                        y[ch] = x[ch] + bp * s1[ch].k * g;
                    }
                    break;
                }
                case kCrush:
                {
                    const float strength = c < 0.f ? p : lerpf (0.5f, p, 0.5f);
                    const float levels = std::exp2 (lerpf (15.f, 2.f, strength));
                    const float down = 1.f + strength * 23.f;
                    const float amt = smooth01 (a * (c < 0.f ? 5.f : 8.f));
                    holdCnt += 1.f;
                    if (holdCnt >= down)
                    {
                        holdCnt -= down;
                        for (int ch = 0; ch < 2; ++ch) hold[ch] = std::round (x[ch] * levels) / levels;
                    }
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float w = hold[ch];
                        if (c > 0.f) { s1[ch].set (expMap (c, 30.f, 6000.f), 0.7f, sr); w = s1[ch].hp (w); }
                        y[ch] = lerpf (x[ch], w, amt);
                    }
                    break;
                }
                case kColorFilter:
                {
                    const float amt = smooth01 (a * 30.f);
                    const float q = lerpf (0.7f, 10.f, p);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float w;
                        if (c < 0.f)
                        {
                            const float fc = expMap (a, 20000.f, 60.f);
                            s1[ch].set (fc, q, sr); s2[ch].set (fc, 0.7f, sr);
                            w = s2[ch].lp (s1[ch].lp (x[ch]));
                        }
                        else
                        {
                            const float fc = expMap (a, 30.f, 8000.f);
                            s1[ch].set (fc, q, sr); s2[ch].set (fc, 0.7f, sr);
                            w = s2[ch].hp (s1[ch].hp (x[ch]));
                        }
                        y[ch] = lerpf (x[ch], w, amt);
                    }
                    break;
                }
                default: break;
            }
            io[0][i] = y[0]; io[1][i] = y[1];
        }
    }

private:
    float sr = 48000.f, hold[2] = { 0.f, 0.f }, holdCnt = 0.f;
    int lastType = -1;
    bool dInit = false;
    Reverb rv;
    DelayLine dub[2];
    SVF s1[2], s2[2], s3[2], s4[2];
    Smoother sc, sp, dts;
    Rng rng[2] = { Rng(), Rng() };
};
} // namespace djfx
