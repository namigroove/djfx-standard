#pragma once
#include "Common.h"

namespace djfx
{
struct JogParams
{
    bool backspin = false, brake = false, reverse = false, scratch = false, loop = false;
    float speed = 0.f;            // スクラッチ時の再生速度(-4..4)。1=等速
    float backspinTimeMs = 1200.f, backspinRate = 5.f, brakeTimeMs = 800.f;
    float loopBeats = 1.f;
    double bpm = 120.0;
    int latencySamples = 0;       // 先読み用の固定レイテンシ(前方向スクラッチの余白)
};

// CDJのSLIPモード相当。常に直近10秒を録音しておき、押している間だけ再生ヘッドを操作する。
// 離すと(裏で進んでいた)ライブ位置へ短いクロスフェードで戻る。
class Jog
{
public:
    void prepare (float sr_)
    {
        sr = sr_;
        int n = 1;
        while (n < (int) (10.f * sr)) n <<= 1;
        N = n; mask = n - 1;
        for (auto& h : hist) h.assign ((size_t) n, 0.f);
        reset();
    }
    void reset()
    {
        for (auto& h : hist) std::fill (h.begin(), h.end(), 0.f);
        wp = 0; state = Idle; g = 0.f; v = 1.f; rd = 0.0;
    }
    bool engaged() const { return state != Idle; }

    void process (float* L, float* R, int n, const JogParams& p)
    {
        State want = p.scratch ? Scratch : p.backspin ? BackSpin : p.brake ? Brake : p.reverse ? Reverse : p.loop ? Loop : Idle;
        if (want != Idle)
        {
            if (state == Idle) { rd = (double) (wp - std::max (4, p.latencySamples)); v = 1.f; t = 0; enter (want, p); }
            else if (want != state) enter (want, p);
            gT = 1.f;
        }
        else gT = 0.f;

        const float ca = 1.f - std::exp (-1.f / (0.002f * sr));
        const float cr = 1.f - std::exp (-1.f / (0.015f * sr));
        const float slewScratch = 1.f - std::exp (-1.f / (0.003f * sr));
        const float slewRev = 1.f - std::exp (-1.f / (0.006f * sr));
        const float bsT = std::max (1.f, p.backspinTimeMs * 0.001f * sr);
        const float brT = std::max (1.f, p.brakeTimeMs * 0.001f * sr);

        for (int i = 0; i < n; ++i)
        {
            hist[0][(size_t) (wp & mask)] = L[i];
            hist[1][(size_t) (wp & mask)] = R[i];
            const int64_t newest = wp;
            ++wp;
            if (p.latencySamples > 0)   // ライブ音も同じだけ遅らせる(PDCで補正される)
            {
                const int64_t li = (newest - p.latencySamples) & mask;
                L[i] = hist[0][(size_t) li]; R[i] = hist[1][(size_t) li];
            }
            if (state == Idle) continue;

            switch (state)
            {
                case BackSpin: { float tt = t / bsT; float k = tt >= 1.f ? 0.f : (1.f - tt); v = -p.backspinRate * k * k; t += 1.f; break; }
                case Brake:    { float tt = t / brT; float k = tt >= 1.f ? 0.f : (1.f - tt); v = vEnter * k * std::sqrt (k); t += 1.f; break; }
                case Reverse:  v += (-1.f - v) * slewRev; break;
                case Scratch:  v += (p.speed - v) * slewScratch; break;
                case Loop:     v = 1.f; break;
                default: break;
            }
            rd += (double) v;
            if (state == Loop && rd >= loopEnd) rd -= (double) loopLen;
            const double hi = (double) (newest - 3), lo = (double) (newest - N + 8);
            if (rd > hi) rd = hi;
            if (rd < lo) rd = lo;

            g += (gT - g) * (gT > g ? ca : cr);
            const float va = clampf (std::fabs (v) * 10.f, 0.f, 1.f);   // 止まった盤は無音(DCを出さない)
            float jl = readAt (0, rd) * va, jr = readAt (1, rd) * va;
            L[i] = L[i] * (1.f - g) + jl * g;
            R[i] = R[i] * (1.f - g) + jr * g;

            if (gT == 0.f && g < 0.0005f) { state = Idle; g = 0.f; }
        }
    }

private:
    enum State { Idle, BackSpin, Brake, Reverse, Scratch, Loop };

    void enter (State s, const JogParams& p)
    {
        vEnter = v; t = 0.f; state = s;
        if (s == Loop)
        {
            loopLen = std::max (64.0, std::min (8.0 * sr, 60.0 / std::max (p.bpm, 30.0) * p.loopBeats * sr));
            rd = std::max (rd - loopLen, (double) (wp - N + 16));
            loopEnd = rd + loopLen;
            v = 1.f;
        }
    }
    float readAt (int ch, double pos) const
    {
        const int64_t i = (int64_t) std::floor (pos);
        const float f = (float) (pos - (double) i);
        const auto& b = hist[ch];
        const float y0 = b[(size_t) ((i - 1) & mask)], y1 = b[(size_t) (i & mask)];
        const float y2 = b[(size_t) ((i + 1) & mask)], y3 = b[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    float sr = 48000.f, g = 0.f, gT = 0.f, v = 1.f, vEnter = 1.f, t = 0.f;
    int N = 1 << 19, mask = (1 << 19) - 1;
    int64_t wp = 0;
    double rd = 0.0, loopLen = 0.0, loopEnd = 0.0;
    State state = Idle;
    std::vector<float> hist[2];
};
} // namespace djfx
