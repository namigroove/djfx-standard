#pragma once
#include <memory>
#include "Common.h"
#include "Reverb.h"

namespace djfx
{
enum FxType
{
    kDelay = 0, kEcho, kPingPong, kSpiral, kHelix, kReverb, kFlanger, kPhaser,
    kFilter, kTripletFilter, kTrans, kRoll, kTripletRoll, kMobius, kNumFx
};

struct FxCtx
{
    float sr = 48000.f;
    int   n = 0;
    float level = 0.5f;        // LEVEL/DEPTH 0..1
    float delaySamples = 0.f;  // 時間(サンプル)。ディレイ・ロール系
    float cycleSamples = 0.f;  // 周期(サンプル)。符号付き(MOBIUS)
    float norm = 0.5f;         // BEAT/TIME位置 0..1(REVERBの残響量)
    float xpad = 0.5f;         // X-PAD位置 0..1(REVERBカットオフ / MOBIUS波形)
    const float* phase = nullptr; // 周期位相 0..1(サンプルごと)
    float act = 0.f;           // 平滑化済みのON量
    bool  on = false;          // 今ONか(スムージング前)
    bool  justOn = false;      // このブロックでOFF→ONになった
};

class FxBase
{
public:
    virtual ~FxBase() = default;
    virtual void prepare (float sr) = 0;
    virtual void reset() = 0;
    // 出力はwet信号(処理後の音)。dryとの混ぜ方はEngine側
    virtual void process (const float* inL, const float* inR, float* wL, float* wR, const FxCtx& c) = 0;
    virtual float mixAmount (const FxCtx& c) const { return c.level; }
};

// ------------------------------------------------------------ DELAY / ECHO
class DelayFx : public FxBase
{
public:
    explicit DelayFx (bool isEcho) : echo (isEcho) {}
    void prepare (float sr_) override
    {
        sr = sr_;
        for (auto& d : line) d.prepare ((int) (4.05f * sr));
        ts.setTime (30.f, sr);
        reset();
    }
    void reset() override
    {
        for (auto& d : line) d.clear();
        for (auto& f : damp) f.reset();
        inited = false;
    }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float* in[2] = { in0, in1 };
        float* w[2] = { w0, w1 };
        const float maxD = 4.f * sr;
        for (auto& f : damp) f.set (4500.f, 0.7f, sr);
        const float tgt = clampf (c.delaySamples, 4.f, maxD);
        if (! inited) { ts.reset (tgt); inited = true; }
        for (int i = 0; i < c.n; ++i)
        {
            const float d = ts.tick (tgt);
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = line[ch].read (d);
                float fbv = echo ? damp[ch].lp (y) * 0.55f : 0.f;
                line[ch].push (in[ch][i] + fbv);
                w[ch][i] = y;
            }
        }
    }

private:
    bool echo, inited = false;
    float sr = 48000.f;
    DelayLine line[2];
    SVF damp[2];
    Smoother ts;
};

// ------------------------------------------------------------ PING PONG
class PingPongFx : public FxBase
{
public:
    void prepare (float sr_) override
    {
        sr = sr_;
        for (auto& d : line) d.prepare ((int) (4.05f * sr));
        ts.setTime (30.f, sr);
        reset();
    }
    void reset() override { for (auto& d : line) d.clear(); inited = false; }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float tgt = clampf (c.delaySamples, 8.f, 4.f * sr);
        if (! inited) { ts.reset (tgt); inited = true; }
        for (int i = 0; i < c.n; ++i)
        {
            const float d = ts.tick (tgt);
            const float mono = 0.5f * (in0[i] + in1[i]);
            float yl = line[0].read (d);
            float yr = line[1].read (d * 0.75f); // 左右で違う時間
            line[0].push (mono * 0.8f + yr * 0.55f);
            line[1].push (yl);
            w0[i] = yl; w1[i] = yr;
        }
    }

private:
    bool inited = false;
    float sr = 48000.f;
    DelayLine line[2];
    Smoother ts;
};

// ------------------------------------------------------------ SPIRAL (ピッチが上がっていくフィードバック残響)
class SpiralFx : public FxBase
{
public:
    void prepare (float sr_) override
    {
        sr = sr_;
        for (int c = 0; c < 2; ++c) { line[c].prepare ((int) (4.05f * sr)); ps[c].prepare (sr); }
        ts.setTime (30.f, sr);
        reset();
    }
    void reset() override
    {
        for (int c = 0; c < 2; ++c) { line[c].clear(); ps[c].clear(); lp[c].reset(); }
        inited = false;
    }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float* in[2] = { in0, in1 };
        float* w[2] = { w0, w1 };
        const float tgt = clampf (c.delaySamples, 64.f, 4.f * sr);
        if (! inited) { ts.reset (tgt); inited = true; }
        const float fb = 0.35f + 0.5f * c.level;
        for (auto& f : lp) f.set (6000.f, 0.7f, sr);
        for (int i = 0; i < c.n; ++i)
        {
            const float d = ts.tick (tgt);
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = line[ch].read (d);
                float shifted = ps[ch].tick (lp[ch].lp (y), 1.1225f);
                line[ch].push (in[ch][i] + shifted * fb);
                w[ch][i] = y;
            }
        }
    }
    float mixAmount (const FxCtx& c) const override { return 0.5f + 0.5f * c.level; }

private:
    bool inited = false;
    float sr = 48000.f;
    DelayLine line[2];
    PitchShifter ps[2];
    SVF lp[2];
    Smoother ts;
};

// ------------------------------------------------------------ HELIX (録音して重ねるループ)
class HelixFx : public FxBase
{
public:
    void prepare (float sr_) override
    {
        sr = sr_;
        for (auto& d : line) d.prepare ((int) (4.05f * sr));
        reset();
    }
    void reset() override { for (auto& d : line) d.clear(); }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float* in[2] = { in0, in1 };
        float* w[2] = { w0, w1 };
        if (c.justOn) reset();
        const float len = clampf (c.delaySamples, 64.f, 4.f * sr);
        const bool freeze = c.level >= 0.97f;
        const float inGain = freeze ? 0.f : 1.f;
        const float fb = freeze ? 1.f : 0.5f + 0.49f * (c.level / 0.97f);
        for (int i = 0; i < c.n; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                float y = line[ch].read (len);
                float nx = in[ch][i] * inGain + y * fb;
                line[ch].push (freeze ? nx : std::tanh (nx));
                w[ch][i] = y * c.act;
            }
    }
    float mixAmount (const FxCtx&) const override { return 1.f; }

private:
    float sr = 48000.f;
    DelayLine line[2];
};

// ------------------------------------------------------------ REVERB
class ReverbFx : public FxBase
{
public:
    void prepare (float sr_) override { sr = sr_; rv.prepare (sr); reset(); }
    void reset() override { rv.clear(); lp[0].reset(); lp[1].reset(); }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        rv.setParams (c.norm, 0.25f);
        const float fc = expMap (clampf (c.xpad, 0.f, 1.f), 1200.f, 18000.f);
        lp[0].set (fc, 0.7f, sr); lp[1].set (fc, 0.7f, sr);
        for (int i = 0; i < c.n; ++i)
        {
            float l, r;
            rv.tick (0.5f * (in0[i] + in1[i]), l, r);
            w0[i] = lp[0].lp (l);
            w1[i] = lp[1].lp (r);
        }
    }

private:
    float sr = 48000.f;
    Reverb rv;
    SVF lp[2];
};

// ------------------------------------------------------------ FLANGER
class FlangerFx : public FxBase
{
public:
    void prepare (float sr_) override { sr = sr_; for (auto& d : line) d.prepare ((int) (0.02f * sr)); reset(); }
    void reset() override { for (auto& d : line) d.clear(); }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float* in[2] = { in0, in1 };
        float* w[2] = { w0, w1 };
        for (int i = 0; i < c.n; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                float p = c.phase[i] + (ch ? 0.25f : 0.f); p -= std::floor (p);
                float tri = 1.f - std::fabs (2.f * p - 1.f);
                float d = (0.0003f + 0.0065f * tri) * sr;
                float y = line[ch].read (d);
                line[ch].push (in[ch][i] + y * 0.55f);
                w[ch][i] = (in[ch][i] + y) * 0.7f;
            }
    }

private:
    float sr = 48000.f;
    DelayLine line[2];
};

// ------------------------------------------------------------ PHASER
class PhaserFx : public FxBase
{
public:
    void prepare (float sr_) override { sr = sr_; reset(); }
    void reset() override { for (auto& c : ap) for (auto& a : c) a.z = 0.f; fbz[0] = fbz[1] = 0.f; }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float* in[2] = { in0, in1 };
        float* w[2] = { w0, w1 };
        for (int i = 0; i < c.n; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                float p = c.phase[i] + (ch ? 0.25f : 0.f); p -= std::floor (p);
                float l = 0.5f - 0.5f * std::cos (2.f * (float) kPi * p);
                float fc = expMap (l, 250.f, 5000.f);
                float t = std::tan ((float) kPi * fc / sr);
                float a = (t - 1.f) / (t + 1.f);
                float x = in[ch][i] + fbz[ch] * 0.55f;
                float y = x;
                for (int s = 0; s < 6; ++s) y = ap[ch][s].tick (y, a);
                fbz[ch] = y;
                w[ch][i] = (in[ch][i] + y) * 0.5f;
            }
    }

private:
    float sr = 48000.f, fbz[2] = { 0.f, 0.f };
    AllpassFirst ap[2][6];
};

// ------------------------------------------------------------ FILTER (周期的に動くレゾナントLPF)
class SweepFilterFx : public FxBase
{
public:
    void prepare (float sr_) override { sr = sr_; reset(); }
    void reset() override { f[0].reset(); f[1].reset(); }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float* in[2] = { in0, in1 };
        float* w[2] = { w0, w1 };
        for (int i = 0; i < c.n; ++i)
        {
            float l = 0.5f - 0.5f * std::cos (2.f * (float) kPi * c.phase[i]);
            float fc = expMap (l, 150.f, 10000.f);
            for (int ch = 0; ch < 2; ++ch)
            {
                f[ch].set (fc, 2.5f, sr);
                w[ch][i] = f[ch].lp (in[ch][i]);
            }
        }
    }

private:
    float sr = 48000.f;
    SVF f[2];
};

// ------------------------------------------------------------ TRANS (周期的に音を切る)
class TransFx : public FxBase
{
public:
    void prepare (float sr_) override { sr = sr_; sm.setTime (1.5f, sr); reset(); }
    void reset() override { sm.reset (1.f); }
    static float dutyOf (float level) { return level <= 0.5f ? 0.5f : 0.5f - (level - 0.5f) * 0.8f; }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float duty = dutyOf (c.level);
        for (int i = 0; i < c.n; ++i)
        {
            float g = sm.tick (c.phase[i] < duty ? 1.f : 0.f);
            w0[i] = in0[i] * g; w1[i] = in1[i] * g;
        }
    }
    float mixAmount (const FxCtx& c) const override { return c.level <= 0.5f ? c.level * 2.f : 1.f; }

private:
    float sr = 48000.f;
    Smoother sm;
};

// ------------------------------------------------------------ ROLL (ONした瞬間の音を繰り返す)
class RollFx : public FxBase
{
public:
    void prepare (float sr_) override
    {
        sr = sr_;
        for (int c = 0; c < 2; ++c) { hist[c].prepare ((int) (4.05f * sr)); loop[c].assign ((size_t) (4.05f * sr), 0.f); }
        reset();
    }
    void reset() override { for (auto& h : hist) h.clear(); pos = 0; len = 0; }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float* in[2] = { in0, in1 };
        float* w[2] = { w0, w1 };
        const int want = (int) clampf (c.delaySamples, 32.f, 4.f * sr);
        if (c.on && (c.justOn || std::abs (want - len) > 1)) capture (want);
        for (int i = 0; i < c.n; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                hist[ch].push (in[ch][i]);
                w[ch][i] = len > 0 ? loop[ch][(size_t) pos] : in[ch][i];
                if (ch == 1 && len > 0 && ++pos >= len) pos = 0;
            }
    }

private:
    void capture (int n)
    {
        len = n; pos = 0;
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < n; ++k)
                loop[ch][(size_t) k] = hist[ch].read ((float) (n - 1 - k));
    }
    float sr = 48000.f;
    DelayLine hist[2];
    std::vector<float> loop[2];
    int pos = 0, len = 0;
};

// ------------------------------------------------------------ MOBIUS (上がり続ける/下がり続けるシェパードトーン)
class MobiusFx : public FxBase
{
public:
    void prepare (float sr_) override { sr = sr_; reset(); }
    void reset() override { p = 0.f; env = 0.f; for (auto& t : th) t = 0.f; }
    void process (const float* in0, const float* in1, float* w0, float* w1, const FxCtx& c) override
    {
        const float cyc = std::max (std::fabs (c.cycleSamples), 16.f);
        const float dir = c.cycleSamples < 0.f ? -1.f : 1.f;
        const float atk = 1.f - std::exp (-1.f / (0.003f * sr));
        const float rel = 1.f - std::exp (-1.f / (0.12f * sr));
        const float wf = clampf (c.xpad, 0.f, 1.f);
        const float twoPi = 2.f * (float) kPi;
        for (int i = 0; i < c.n; ++i)
        {
            float a = std::fabs (0.5f * (in0[i] + in1[i]));
            env += (a > env ? atk : rel) * (a - env);
            p += dir / cyc; p -= std::floor (p);
            float sum = 0.f;
            for (int k = 0; k < kPartials; ++k)
            {
                float pos = (float) k + p;                       // オクターブ位置
                float f = 16.f * std::exp2 (pos);
                float amp = std::exp (-0.5f * (pos - 5.f) * (pos - 5.f) / (1.7f * 1.7f));
                th[k] += twoPi * f / sr;
                if (th[k] > twoPi) th[k] -= twoPi;
                float s = std::sin (th[k]);
                float sq = std::tanh (3.f * s) * 1.0045f;
                sum += amp * lerpf (s, sq, wf);
            }
            float o = sum * 0.35f * std::min (env * 4.f, 1.f);
            w0[i] = o; w1[i] = o;
        }
    }

private:
    static constexpr int kPartials = 10;
    float sr = 48000.f, p = 0.f, env = 0.f, th[kPartials] = {};
};

inline std::unique_ptr<FxBase> makeFx (int type)
{
    switch (type)
    {
        case kDelay:         return std::make_unique<DelayFx> (false);
        case kEcho:          return std::make_unique<DelayFx> (true);
        case kPingPong:      return std::make_unique<PingPongFx>();
        case kSpiral:        return std::make_unique<SpiralFx>();
        case kHelix:         return std::make_unique<HelixFx>();
        case kReverb:        return std::make_unique<ReverbFx>();
        case kFlanger:       return std::make_unique<FlangerFx>();
        case kPhaser:        return std::make_unique<PhaserFx>();
        case kFilter:
        case kTripletFilter: return std::make_unique<SweepFilterFx>();
        case kTrans:         return std::make_unique<TransFx>();
        case kRoll:
        case kTripletRoll:   return std::make_unique<RollFx>();
        case kMobius:        return std::make_unique<MobiusFx>();
        default:             return std::make_unique<DelayFx> (false);
    }
}
} // namespace djfx
