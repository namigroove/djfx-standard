#pragma once
#include <array>
#include <memory>
#include <string>
#include "BeatFx.h"
#include "ColorFx.h"
#include "Jog.h"

namespace djfx
{
// ------------------------------------------------------------ 表示名・拍リスト
inline const char* fxName (int t)
{
    static const char* n[kNumFx] = { "DELAY", "ECHO", "PING PONG", "SPIRAL", "HELIX", "REVERB", "FLANGER",
                                     "PHASER", "FILTER", "TRIPLET FILTER", "TRANS", "ROLL", "TRIPLET ROLL", "MOBIUS" };
    return n[t < 0 ? 0 : (t >= kNumFx ? kNumFx - 1 : t)];
}
inline const char* colorName (int t)
{
    static const char* n[kNumColor] = { "OFF", "SPACE", "DUB ECHO", "SWEEP", "NOISE", "CRUSH", "FILTER" };
    return n[t < 0 ? 0 : (t >= kNumColor ? kNumColor - 1 : t)];
}

struct BeatList { const double* v; const char* const* label; int n; };

inline BeatList beatListFor (int type)
{
    static const double echoV[] = { 1. / 16, 1. / 8, 1. / 4, 1. / 2, 3. / 4, 1, 2, 4, 8, 16 };
    static const char* const echoL[] = { "1/16", "1/8", "1/4", "1/2", "3/4", "1", "2", "4", "8", "16" };
    static const double lfoV[] = { 1. / 16, 1. / 8, 1. / 4, 1. / 2, 3. / 4, 1, 2, 4, 8, 16, 32, 64 };
    static const char* const lfoL[] = { "1/16", "1/8", "1/4", "1/2", "3/4", "1", "2", "4", "8", "16", "32", "64" };
    static const double tripV[] = { 1. / 12, 1. / 6, 1. / 3, 2. / 3, 4. / 3, 8. / 3, 12. / 3 };
    static const char* const tripL[] = { "1/12", "1/6", "1/3", "2/3", "4/3", "8/3", "12/3" };
    static const double mobV[] = { -64, -32, -16, -8, -4, -2, -1, -3. / 4, -1. / 2, -1. / 4, -1. / 8, -1. / 16,
                                    1. / 16, 1. / 8, 1. / 4, 1. / 2, 3. / 4, 1, 2, 4, 8, 16, 32, 64 };
    static const char* const mobL[] = { "-64", "-32", "-16", "-8", "-4", "-2", "-1", "-3/4", "-1/2", "-1/4", "-1/8", "-1/16",
                                         "1/16", "1/8", "1/4", "1/2", "3/4", "1", "2", "4", "8", "16", "32", "64" };
    switch (type)
    {
        case kFlanger: case kPhaser: case kFilter: return { lfoV, lfoL, 12 };
        case kTripletFilter: case kTripletRoll: return { tripV, tripL, 7 };
        case kMobius: return { mobV, mobL, 24 };
        default: return { echoV, echoL, 10 };
    }
}
inline int beatIndex (int type, float norm)
{
    const int n = beatListFor (type).n;
    return std::max (0, std::min (n - 1, (int) std::lround (clampf (norm, 0.f, 1.f) * (float) (n - 1))));
}
inline float beatNormFromIndex (int type, int idx)
{
    const int n = beatListFor (type).n;
    return n > 1 ? (float) idx / (float) (n - 1) : 0.f;
}
inline std::string beatLabel (int type, float norm)
{
    if (type == kReverb) return std::to_string ((int) std::lround (1.f + 99.f * clampf (norm, 0.f, 1.f))) + "%";
    auto l = beatListFor (type);
    return l.label[beatIndex (type, norm)];
}

// ------------------------------------------------------------ パラメータ(エンジン用の素のデータ)
struct Params
{
    bool  fxOn = false;
    int   fxType = kEcho;
    int   fxBandMask = 7;      // FX FREQUENCY: LOW=1 MID=2 HI=4 の組み合わせ(7=全帯域)
    bool  fxSync = true;
    float fxBeat = 0.5f;       // 0..1(リスト内の位置)
    float fxTimeMs = 500.f;
    float fxLevel = 0.5f;
    bool  xpadTouch = false;
    float xpad = 0.5f;

    int   colorType = 0;
    float colorKnob = 0.f;     // -1..1
    float colorParam = 0.5f;
    bool  centerLock = false;

    bool  jogBackspin = false, jogBrake = false, jogReverse = false, jogScratch = false, jogLoop = false;
    float jogSpeed = 0.f;
    float backspinTimeMs = 1200.f, backspinRate = 5.f, brakeTimeMs = 800.f;
    float backspinBeats = 0.f, brakeBeats = 0.f;   // >0ならBPMから長さを決める(拍数)
    float loopBeat = 0.5f;     // 0..1(リスト内の位置)
    float jogLatencyMs = 0.f;  // 前方向スクラッチ用の先読み(0なら遅延なし)

    float bpmFallback = 120.f;
    float outGainDb = 0.f;
};

struct HostInfo
{
    double bpm = 0.0;     // 0以下=不明
    double ppq = -1.0;    // 負=不明
    bool   playing = false;
};

enum MidiBits { kMidiBackspin = 1, kMidiBrake = 2, kMidiReverse = 4, kMidiScratch = 8, kMidiLoop = 16, kMidiFx = 32 };

// ------------------------------------------------------------ エンジン
class Engine
{
public:
    static constexpr int kMaxChunk = 2048;

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (int t = 0; t < kNumFx; ++t) { fx[t] = makeFx (t); fx[t]->prepare (sr); }
        color.prepare (sr);
        jog.prepare (sr);
        for (auto& f : bandLo) f.reset();
        actSm.setTime (8.f, sr);
        gainSm.setTime (20.f, sr); gainSm.reset (1.f);
        for (auto* b : { &wl, &wr, &gl, &gr, &dl, &dr, &ph }) b->assign (kMaxChunk, 0.f);
        ppqInternal = 0.0; freePhase = 0.0; lastType = -1; lastOn = false; idleCount = 0;
        reset();
    }
    void reset()
    {
        for (auto& f : fx) if (f) f->reset();
        color.reset(); jog.reset();
        for (auto& f : bandLo) f.reset();
        for (auto& f : bandMid) f.reset();
    }

    void process (float* L, float* R, int n, const Params& pIn, const HostInfo& host, int midiMask)
    {
        Params p = pIn;
        if (midiMask & kMidiBackspin) p.jogBackspin = true;
        if (midiMask & kMidiBrake)    p.jogBrake = true;
        if (midiMask & kMidiReverse)  p.jogReverse = true;
        if (midiMask & kMidiScratch)  p.jogScratch = true;
        if (midiMask & kMidiLoop)     p.jogLoop = true;
        if (midiMask & kMidiFx)       p.fxOn = true;

        for (int off = 0; off < n; off += kMaxChunk)
            processChunk (L + off, R + off, std::min (kMaxChunk, n - off), p, host);
    }

    bool jogEngaged() const { return jog.engaged(); }
    static int latencyFor (float ms, double sr) { return (int) (ms * 0.001 * sr); }

private:
    void processChunk (float* L, float* R, int n, const Params& p, const HostInfo& host)
    {
        const double bpm = host.bpm > 1.0 ? host.bpm : (double) std::max (30.f, p.bpmFallback);

        // ① ジョグ(バックスピン等)
        JogParams jp;
        jp.backspin = p.jogBackspin; jp.brake = p.jogBrake; jp.reverse = p.jogReverse;
        jp.scratch = p.jogScratch;   jp.loop = p.jogLoop;   jp.speed = p.jogSpeed;
        auto beatMs = [bpm] (float beats, float ms) { return beats > 0.f ? (float) std::clamp (beats * 60000.0 / bpm, 20.0, 30000.0) : ms; };
        jp.backspinTimeMs = beatMs (p.backspinBeats, p.backspinTimeMs); jp.backspinRate = p.backspinRate;
        jp.brakeTimeMs = beatMs (p.brakeBeats, p.brakeTimeMs);
        jp.loopBeats = (float) beatListFor (kEcho).v[beatIndex (kEcho, p.loopBeat)];
        jp.bpm = bpm;
        jp.latencySamples = (int) (p.jogLatencyMs * 0.001f * sr);
        jog.process (L, R, n, jp);

        // ② SOUND COLOR FX
        color.process (L, R, n, p.colorType, p.colorKnob, p.colorParam, bpm, p.centerLock);

        // ③ BEAT FX
        beatStage (L, R, n, p, bpm, host);

        // 出力ゲイン
        const float g = std::pow (10.f, p.outGainDb * 0.05f);
        for (int i = 0; i < n; ++i)
        {
            const float gs = gainSm.tick (g);
            L[i] *= gs; R[i] *= gs;
        }
    }

    static bool isAddType (int t)
    {
        return t == kDelay || t == kEcho || t == kPingPong || t == kSpiral || t == kHelix || t == kReverb || t == kMobius;
    }

    void beatStage (float* L, float* R, int n, const Params& p, double bpm, const HostInfo& host)
    {
        const int type = std::max (0, std::min (kNumFx - 1, p.fxType));
        const bool on = p.fxOn || p.xpadTouch;
        if (type != lastType) { fx[type]->reset(); lastType = type; idleCount = 0; }
        const bool justOn = on && ! lastOn;
        lastOn = on;

        // ---- 時間・周期の解決
        const float norm = p.xpadTouch ? p.xpad : p.fxBeat;
        const auto list = beatListFor (type);
        const double beatsVal = list.v[beatIndex (type, norm)];
        double ms;
        if (p.fxSync) ms = std::fabs (beatsVal) * 60000.0 / bpm;
        else if (p.xpadTouch) ms = (double) expMap (clampf (p.xpad, 0.f, 1.f), 10.f, 4000.f);
        else ms = p.fxTimeMs;
        double minMs = 1.0, maxMs = 4000.0;
        switch (type)
        {
            case kFlanger: case kPhaser: case kFilter: case kTripletFilter: case kMobius: minMs = 10.0; maxMs = 32000.0; break;
            case kTrans: minMs = 10.0; maxMs = 16000.0; break;
            case kRoll: case kTripletRoll: minMs = 10.0; break;
            default: break;
        }
        ms = std::max (minMs, std::min (maxMs, ms));
        const float delaySamples = (float) (ms * 0.001 * sr);
        const float sign = (type == kMobius && p.fxSync && beatsVal < 0.0) ? -1.f : 1.f;

        // ---- 周期位相(LFO/TRANS用)
        const double ppq0 = (host.playing && host.ppq >= 0.0) ? host.ppq : ppqInternal;
        const double perSample = bpm / (60.0 * (double) sr);
        const double cycleBeats = std::fabs (beatsVal);
        for (int i = 0; i < n; ++i)
        {
            double ph01;
            if (p.fxSync) { double x = (ppq0 + i * perSample) / cycleBeats; ph01 = x - std::floor (x); }
            else { freePhase += 1.0 / std::max (1.0, (double) delaySamples); freePhase -= std::floor (freePhase); ph01 = freePhase; }
            ph[(size_t) i] = (float) ph01;
        }
        ppqInternal = ppq0 + n * perSample;

        // ---- 帯域選択(LOW/MID/HIは選んだ帯域だけにFXをかける)
        const int bandMask = p.fxBandMask & 7;
        const bool all = bandMask == 0 || bandMask == 7;
        if (! all)
        {
            for (auto& f : bandLo) f.set (250.f, 0.7071f, sr);
            for (auto& f : bandMid) f.set (2500.f, 0.7071f, sr);
        }
        float* io[2] = { L, R };
        std::vector<float>* dry[2] = { &gl, &gr };   // FXに入れる帯域
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                float x = io[c][i], sel = x;
                if (! all)
                {
                    float low = bandLo[c * 2 + 1].lp (bandLo[c * 2].lp (x));
                    float rest = x - low;
                    float mid = bandMid[c * 2 + 1].lp (bandMid[c * 2].lp (rest));
                    float hi = rest - mid;
                    sel = ((bandMask & 1) ? low : 0.f) + ((bandMask & 2) ? mid : 0.f) + ((bandMask & 4) ? hi : 0.f);
                }
                (*dry[c])[(size_t) i] = sel;
                io[c][i] = x - sel;   // バイパスされる帯域
            }

        // ---- ON/OFFのスムージングと、テール処理のスキップ判定
        float actEnd = 0.f;
        for (int i = 0; i < n; ++i) { actEnd = actSm.tick (on ? 1.f : 0.f); act[(size_t) i] = actEnd; }
        const bool add = isAddType (type);
        if (! on && actEnd < 1e-4f) idleCount += n; else idleCount = 0;
        if (add && idleCount > (int) (12.f * sr))
        {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) io[c][i] += (*dry[c])[(size_t) i];
            return;
        }

        // ---- FX本体
        for (int i = 0; i < n; ++i)
        {
            const float a = add ? act[(size_t) i] : 1.f;
            dl[(size_t) i] = gl[(size_t) i] * a;
            dr[(size_t) i] = gr[(size_t) i] * a;
        }
        FxCtx ctx;
        ctx.sr = sr; ctx.n = n; ctx.level = p.fxLevel; ctx.delaySamples = delaySamples;
        ctx.cycleSamples = delaySamples * sign; ctx.norm = clampf (norm, 0.f, 1.f); ctx.xpad = p.xpad;
        ctx.phase = ph.data(); ctx.act = actEnd; ctx.on = on; ctx.justOn = justOn;
        const float* inL = add ? dl.data() : gl.data();
        const float* inR = add ? dr.data() : gr.data();
        fx[type]->process (inL, inR, wl.data(), wr.data(), ctx);
        const float mixAmt = fx[type]->mixAmount (ctx);

        for (int i = 0; i < n; ++i)
        {
            const float d0 = gl[(size_t) i], d1 = gr[(size_t) i];
            float o0, o1;
            if (add) { o0 = d0 + wl[(size_t) i] * mixAmt; o1 = d1 + wr[(size_t) i] * mixAmt; }
            else
            {
                const float m = act[(size_t) i] * mixAmt;
                o0 = d0 + (wl[(size_t) i] - d0) * m; o1 = d1 + (wr[(size_t) i] - d1) * m;
            }
            io[0][i] += o0; io[1][i] += o1;
        }
    }

    float sr = 48000.f;
    std::array<std::unique_ptr<FxBase>, kNumFx> fx;
    ColorFx color;
    Jog jog;
    SVF bandLo[4], bandMid[4];
    Smoother actSm, gainSm;
    std::vector<float> wl, wr, gl, gr, dl, dr, ph;
    std::vector<float> act = std::vector<float> (kMaxChunk, 0.f);
    double ppqInternal = 0.0, freePhase = 0.0;
    int lastType = -1, idleCount = 0;
    bool lastOn = false;
};
} // namespace djfx
