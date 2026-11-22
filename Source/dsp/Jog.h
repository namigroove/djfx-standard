#pragma once
#include "Common.h"

namespace djfx
{
struct JogParams
{
    bool backspin = false, brake = false, reverse = false, scratch = false, loop = false;
    float speed = 0.f;            // スクラッチ時の再生速度(-4..4)。1=等速
    float backspinTimeMs = 1200.f, backspinRate = 5.f, brakeTimeMs = 800.f;
    float releaseTimeMs = 400.f;  // 止めた盤を放して等速に戻るまでの時間(BRAKEの逆)
    float loopBeats = 1.f;
    double bpm = 120.0;
    int latencySamples = 0;       // 先読み用の固定レイテンシ(前方向スクラッチの余白)

    bool slip = true;             // ON: 離すと裏で進んでいたライブ位置へ戻る / OFF: 離した位置から続ける(ライブより遅れる)
    bool loopRecall = false;      // ループに入るとき、直前のループの始点から入り直す(RELOOP)
    bool resync = false;          // 遅れたまま続けている再生を、ライブ位置へ戻す
    float snapBeats = 0.f;        // >0: SLIP OFFで続けるとき、遅れを拍(この拍数)の整数倍に揃える(QUANTIZE)
};

// CDJのSLIPモード相当。常に直近10秒を録音しておき、押している間だけ再生ヘッドを操作する。
// SLIP ON : 離すと(裏で進んでいた)ライブ位置へ短いクロスフェードで戻る。BRAKEだけは放すと等速まで加速(RELEASE)してから戻る。
// SLIP OFF: 離すとRELEASEで等速まで加速し、その位置から続ける。ライブより遅れた状態になり、RESYNCかSLIP ONで戻る。
// DAWが再生を止めた/シークした(transportJump)ときは、それ以前の録音を使わない。
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
        wp = 0; state = Idle; g = 0.f; gT = 0.f; v = 1.f; rd = 0.0; xf = 1.f;
        validFrom = 0; haveLoop = false; lag = 0.0;
    }
    bool engaged() const { return state != Idle; }
    double lagSamples() const { return lag; }

    // DAWの停止/再生開始/シークで音が途切れる。以前の録音は別の時間のものなので、巻き戻しの対象から外す。
    void transportJump()
    {
        state = Idle; g = 0.f; gT = 0.f; v = 1.f; xf = 1.f; lag = 0.0; followFade = false;
        validFrom = wp; haveLoop = false;
    }

    void process (float* L, float* R, int n, const JogParams& p)
    {
        State want = p.scratch ? Scratch : p.backspin ? BackSpin : p.brake ? Brake : p.reverse ? Reverse : p.loop ? Loop : Idle;
        if (want != Idle)
        {
            if (state == Idle)
            {
                rd = std::max ((double) (wp - std::max (4, p.latencySamples)), (double) validFrom);
                v = 1.f; t = 0; xf = 1.f; enter (want, p);
            }
            else if (want != state) enter (want, p);
            else if (state == Loop) resizeLoop (p);
            gT = 1.f;
        }
        else
        {
            switch (state)
            {
                case Idle:    gT = 0.f; break;
                case Release: if (p.resync) relSlipFade = true; gT = 1.f; break;
                case Follow:  gT = (p.slip || p.resync || followFade) ? 0.f : 1.f; break;
                default:      // 操作を離した
                    if (! p.slip || state == Brake) { state = Release; t = 0.f; vEnter = v; relSlipFade = p.slip || p.resync; followFade = false; gT = 1.f; }
                    else gT = 0.f;
                    break;
            }
        }

        const float ca = 1.f - std::exp (-1.f / (0.002f * sr));
        const float cr = 1.f - std::exp (-1.f / (0.015f * sr));
        const float slewScratch = 1.f - std::exp (-1.f / (0.003f * sr));
        const float slewRev = 1.f - std::exp (-1.f / (0.006f * sr));
        const float bsT = std::max (1.f, p.backspinTimeMs * 0.001f * sr);
        const float brT = std::max (1.f, p.brakeTimeMs * 0.001f * sr);
        const float reT = std::max (1.f, p.releaseTimeMs * 0.001f * sr);

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
                case Release:
                {
                    const float tt = t / reT;
                    if (tt >= 1.f)
                    {
                        v = 1.f;
                        if (relSlipFade) gT = 0.f;                 // SLIP ON: 加速し終えたらライブへ戻る
                        else { state = Follow; snapLag (newest, p); }
                    }
                    else { const float k = 1.f - tt; v = 1.f - (1.f - vEnter) * k * std::sqrt (k); t += 1.f; }   // BRAKEの逆カーブ
                    break;
                }
                case Follow:   v = 1.f; break;
                default: break;
            }
            rd += (double) v;
            if (xf < 1.f) rdPrev += (double) v;
            if (state == Loop && rd >= loopEnd) jumpHead (rd - loopLen, std::min (0.01 * sr, loopLen * 0.25));

            const double hi = (double) (newest - 3);
            const double lo = std::min (hi, (double) std::max<int64_t> (newest - N + 8, validFrom));
            const bool atEdge = rd <= lo && v < 0.f;                // DAWで再生してきた最初まで巻き戻した
            if (rd > hi) rd = hi;
            if (rd < lo) rd = lo;
            if (rdPrev > hi) rdPrev = hi;
            if (rdPrev < lo) rdPrev = lo;

            g += (gT - g) * (gT > g ? ca : cr);
            const float va = atEdge ? 0.f : clampf (std::fabs (v) * 10.f, 0.f, 1.f);   // 止まった盤は無音(DCを出さない)
            float jl = readAt (0, rd), jr = readAt (1, rd);
            if (xf < 1.f)
            {
                jl = jl * xf + readAt (0, rdPrev) * (1.f - xf);
                jr = jr * xf + readAt (1, rdPrev) * (1.f - xf);
                xf = std::min (1.f, xf + xfStep);
            }
            jl *= va; jr *= va;
            L[i] = L[i] * (1.f - g) + jl * g;
            R[i] = R[i] * (1.f - g) + jr * g;
            lag = (double) newest - rd;

            if (gT == 0.f && g < 0.0005f) { state = Idle; g = 0.f; lag = 0.0; }
        }
    }

private:
    enum State { Idle, BackSpin, Brake, Reverse, Scratch, Loop, Release, Follow };

    double loBound (int64_t newest) const { return (double) std::max<int64_t> (newest - N + 8, validFrom); }

    void jumpHead (double newRd, double xfSamples)
    {
        rdPrev = rd; rd = newRd;
        xf = 0.f; xfStep = (float) (1.0 / std::max (8.0, xfSamples));
    }

    // SLIP OFFで続けるとき、ライブとの遅れを拍の整数倍に揃える(DAWの拍に乗ったまま遅れる)
    void snapLag (int64_t newest, const JogParams& p)
    {
        if (p.snapBeats <= 0.f) return;
        const double beatS = 60.0 / std::max (30.0, p.bpm) * (double) sr * (double) p.snapBeats;
        const double cur = (double) newest - rd;
        const double tgt = std::round (cur / beatS) * beatS;
        if (tgt < 4.0) { gT = 0.f; followFade = true; return; }                        // ほぼライブ: そのままライブへ戻る
        if (std::fabs (tgt - cur) > 8.0) jumpHead ((double) newest - tgt, 0.01 * sr);
    }

    void enter (State s, const JogParams& p)
    {
        vEnter = v; t = 0.f; state = s; followFade = false;
        if (s == Loop)
        {
            const int64_t newest = wp - 1;
            const double len = std::max (64.0, std::min (8.0 * sr, 60.0 / std::max (p.bpm, 30.0) * p.loopBeats * sr));
            loopLen = len; loopBeatsCur = p.loopBeats;
            const double lo = loBound (newest);
            if (p.loopRecall && haveLoop && lastLoopStart >= lo + 16.0)
            {
                loopStart = lastLoopStart;                         // 直前のループの始点から入り直す(RELOOP)
                loopEnd = loopStart + loopLen;
                const double hiEnd = (double) (newest - 3);
                if (loopEnd > hiEnd) { loopEnd = hiEnd; loopStart = loopEnd - loopLen; }
            }
            else
            {
                loopStart = std::max (rd - loopLen, lo + 16.0);
                loopEnd = loopStart + loopLen;
            }
            if (xf >= 1.f && g > 0.001f) jumpHead (loopStart, 0.01 * sr); else rd = loopStart;
            lastLoopStart = loopStart; haveLoop = true;
            v = 1.f;
        }
    }

    // ループ中にLOOP BEAT(1/2X・2X)が変わったとき: 終点を保って始点を動かす
    void resizeLoop (const JogParams& p)
    {
        if (p.loopBeats == loopBeatsCur) return;
        loopBeatsCur = p.loopBeats;
        const int64_t newest = wp - 1;
        double len = std::max (64.0, std::min (8.0 * sr, 60.0 / std::max (p.bpm, 30.0) * p.loopBeats * sr));
        const double lo = loBound (newest) + 16.0;
        if (loopEnd - len < lo) len = std::max (64.0, loopEnd - lo);
        loopLen = len; loopStart = loopEnd - len;
        if (rd < loopStart || rd >= loopEnd)
        {
            double x = std::fmod (rd - loopStart, loopLen);
            if (x < 0.0) x += loopLen;
            jumpHead (loopStart + x, std::min (0.01 * sr, loopLen * 0.25));
        }
        lastLoopStart = loopStart; haveLoop = true;
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

    float sr = 48000.f, g = 0.f, gT = 0.f, v = 1.f, vEnter = 1.f, t = 0.f, xf = 1.f, xfStep = 0.01f, loopBeatsCur = 0.f;
    int N = 1 << 19, mask = (1 << 19) - 1;
    int64_t wp = 0, validFrom = 0;
    double rd = 0.0, rdPrev = 0.0, loopLen = 0.0, loopStart = 0.0, loopEnd = 0.0, lastLoopStart = 0.0, lag = 0.0;
    bool relSlipFade = true, haveLoop = false, followFade = false;
    State state = Idle;
    std::vector<float> hist[2];
};
} // namespace djfx
