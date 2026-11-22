// DSPエンジンの単体テスト(JUCE不要)。全エフェクトにテスト信号を通して、数値で挙動を確認する。
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include "../Source/dsp/Engine.h"
using namespace djfx;

static const double SR = 48000.0;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("  FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// ドラム風テスト信号: 120BPM(0.5s)ごとにキック(減衰サイン)+ノイズバースト、上にサイン
static void makeSignal (std::vector<float>& L, std::vector<float>& R, int n)
{
    L.assign ((size_t) n, 0.f); R = L;
    Rng rng;
    for (int i = 0; i < n; ++i)
    {
        double t = i / SR, ph = std::fmod (t, 0.5);
        float kick = (float) (std::sin (2 * kPi * (50 + 120 * std::exp (-ph * 30)) * ph) * std::exp (-ph * 9));
        float hat = (std::fmod (t, 0.25) < 0.02) ? 0.2f * rng.white() : 0.f;
        float tone = 0.15f * (float) std::sin (2 * kPi * 440.0 * t);
        L[(size_t) i] = 0.6f * kick + hat + tone;
        R[(size_t) i] = 0.6f * kick + hat * 0.8f + tone * 0.9f;
    }
}

struct Stat { float peak = 0; double rms = 0; bool bad = false; };
static Stat stat (const std::vector<float>& a, int from, int to)
{
    Stat s; double e = 0;
    for (int i = from; i < to; ++i) { float v = a[(size_t) i]; if (!std::isfinite (v)) s.bad = true; s.peak = std::max (s.peak, std::fabs (v)); e += (double) v * v; }
    s.rms = std::sqrt (e / std::max (1, to - from)); return s;
}

static void run (Engine& e, std::vector<float>& L, std::vector<float>& R, Params p, HostInfo h, int block = 512,
                 const std::vector<std::pair<int, Params>>* changes = nullptr)
{
    int n = (int) L.size(); size_t ci = 0;
    for (int off = 0; off < n; off += block)
    {
        int m = std::min (block, n - off);
        if (changes) while (ci < changes->size() && (*changes)[ci].first <= off) p = (*changes)[ci++].second;
        h.ppq = h.playing ? off * 120.0 / (60.0 * SR) : -1.0;
        e.process (L.data() + off, R.data() + off, m, p, h, 0);
    }
}

static double zeroCrossRate (const std::vector<float>& a, int from, int to)
{
    int z = 0; for (int i = from + 1; i < to; ++i) if ((a[(size_t) i - 1] < 0) != (a[(size_t) i] < 0)) ++z;
    return z / ((to - from) / SR) / 2.0; // Hz相当
}


// 出力と入力の相関が最大になる遅れ(サンプル)を探す
static int bestLag (const std::vector<float>& in, const std::vector<float>& out, int at, int win, int maxLag, double* corr = nullptr)
{
    double eo = 0; for (int i = 0; i < win; ++i) eo += (double) out[(size_t) (at + i)] * out[(size_t) (at + i)];
    double best = -1e9; int bl = 0;
    for (int lag = 0; lag <= maxLag && at - lag >= 0; ++lag)
    {
        double c = 0, ei = 0;
        for (int i = 0; i < win; ++i) { const double x = in[(size_t) (at + i - lag)]; c += (double) out[(size_t) (at + i)] * x; ei += x * x; }
        const double r = c / std::sqrt(eo * ei + 1e-30);
        if (r > best) { best = r; bl = lag; }
    }
    if (corr) *corr = best;
    return bl;
}
// 信号a(at位置)と信号b(at位置)を、bをずらして比べたときの最大相関
static double bestShiftCorr (const std::vector<float>& a, int atA, const std::vector<float>& b, int atB, int win, int maxShift)
{
    double ea = 0; for (int i = 0; i < win; ++i) ea += (double) a[(size_t) (atA + i)] * a[(size_t) (atA + i)];
    double best = -1e9;
    for (int sh = 0; sh < maxShift; ++sh)
    {
        double c = 0, eb = 0;
        for (int i = 0; i < win; ++i) { const double y = b[(size_t) (atB + sh + i)]; c += (double) a[(size_t) (atA + i)] * y; eb += y * y; }
        best = std::max (best, c / std::sqrt (ea * eb + 1e-30));
    }
    return best;
}
static std::vector<float> noiseSig (int n, float amp = 0.3f) { std::vector<float> v ((size_t) n); Rng r; for (auto& x : v) x = amp * r.white(); return v; }

int main()
{
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    const int N = (int) (SR * 6);
    HostInfo host; host.bpm = 120; host.playing = true;

    std::printf("[1] バイパス(全部OFF)は入力と完全一致するか\n");
    {
        std::vector<float> L, R; makeSignal (L, R, N); auto L0 = L, R0 = R;
        Engine e; e.prepare (SR); Params p; run (e, L, R, p, host);
        double maxd = 0; for (int i = 0; i < N; ++i) maxd = std::max (maxd, (double) std::fabs (L[(size_t) i] - L0[(size_t) i]));
        std::printf("  max diff = %g\n", maxd);
        CHECK (maxd < 1e-5, "bypass is not transparent");
    }

    std::printf("[2] 帯域選択(LOW/MID/HI)でFX OFFでも信号が壊れないか\n");
    for (int band : { 1, 2, 4, 3, 6, 5 })
    {
        std::vector<float> L, R; makeSignal (L, R, N); auto L0 = L;
        Engine e; e.prepare (SR); Params p; p.fxBandMask = band; p.fxType = kFlanger; run (e, L, R, p, host);
        double maxd = 0; for (int i = 0; i < N; ++i) maxd = std::max (maxd, (double) std::fabs (L[(size_t) i] - L0[(size_t) i]));
        std::printf("  band %d: max diff = %g\n", band, maxd);
        CHECK (maxd < 1e-4, "band split not reconstructing");
    }

    std::printf("[3] BEAT FX 14種 x (同期/ms) x (帯域 全/LOW/HI/MID+HI) : 有限値・ピーク・無音でないこと\n");
    for (int t = 0; t < kNumFx; ++t)
        for (int sync = 0; sync < 2; ++sync)
            for (int band : { 7, 1, 4, 6 })
            {
                std::vector<float> L, R; makeSignal (L, R, N); auto L0 = L;
                Engine e; e.prepare (SR);
                Params p; p.fxOn = true; p.fxType = t; p.fxSync = sync; p.fxBandMask = band; p.fxLevel = 0.8f; p.fxBeat = 0.5f; p.fxTimeMs = 375.f;
                run (e, L, R, p, host);
                Stat s = stat (L, (int) (SR * 1), N);
                double diff = 0; for (int i = (int) SR; i < N; ++i) diff += std::fabs (L[(size_t) i] - L0[(size_t) i]);
                bool ok = ! s.bad && s.peak < 6.f && diff > 1.0;
                if (!ok || (sync == 1 && band == 7)) std::printf("  %-14s sync=%d band=%d peak=%.2f rms=%.3f diff=%.1f %s\n", fxName (t), sync, band, s.peak, s.rms, diff, ok ? "" : "<<");
                CHECK (ok, "%s sync=%d band=%d", fxName (t), sync, band);
            }

    std::printf("[4] ECHO: 1/4拍@120BPM=125msで最初の反復が来るか(インパルス)\n");
    {
        std::vector<float> L ((size_t) (SR * 2), 0.f), R = L; L[4800] = 1.f; R[4800] = 1.f;
        Engine e; e.prepare (SR);
        Params p; p.fxOn = true; p.fxType = kEcho; p.fxBeat = beatNormFromIndex (kEcho, 2); p.fxLevel = 1.f; // 1/4
        run (e, L, R, p, host);
        int best = -1; float bv = 0; for (int i = 4801; i < (int) L.size(); ++i) if (std::fabs (L[(size_t) i]) > bv) { bv = std::fabs (L[(size_t) i]); best = i; }
        int lag = best - 4800;
        std::printf("  first repeat lag = %d samples (expected 6000), amp %.3f\n", lag, bv);
        CHECK (std::abs (lag - 6000) < 40, "echo lag %d", lag);
    }

    std::printf("[5] DELAY: 反復は1回だけか / ECHOは減衰しながら複数回\n");
    for (int type : { (int) kDelay, (int) kEcho })
    {
        std::vector<float> L ((size_t) (SR * 3), 0.f), R = L; L[4800] = 1.f; R[4800] = 1.f;
        Engine e; e.prepare (SR);
        Params p; p.fxOn = true; p.fxType = type; p.fxBeat = beatNormFromIndex (kEcho, 3); p.fxLevel = 1.f; // 1/2拍=250ms
        run (e, L, R, p, host);
        int peaks = 0; for (int i = 5000; i < (int) L.size(); ++i) if (std::fabs (L[(size_t) i]) > 0.05f && std::fabs (L[(size_t) i]) >= std::fabs (L[(size_t) i - 1]) && std::fabs (L[(size_t) i]) >= std::fabs (L[(size_t) i + 1])) { ++peaks; i += 2000; }
        std::printf("  %s: repeats(>0.05) = %d\n", fxName (type), peaks);
        if (type == kDelay) CHECK (peaks == 1, "delay should repeat once, got %d", peaks); else CHECK (peaks >= 3, "echo repeats %d", peaks);
    }

    std::printf("[6] TRANS: 1/4拍周期(125ms)・デューティ50%%でゲートがかかるか\n");
    {
        std::vector<float> L ((size_t) (SR * 2), 0.5f), R = L;
        Engine e; e.prepare (SR);
        Params p; p.fxOn = true; p.fxType = kTrans; p.fxBeat = beatNormFromIndex (kTrans, 2); p.fxLevel = 0.5f; // level0.5 -> mix 1, duty .5
        run (e, L, R, p, host);
        int zeros = 0, cnt = 0; for (int i = (int) SR; i < (int) L.size(); ++i) { ++cnt; if (std::fabs (L[(size_t) i]) < 0.02f) ++zeros; }
        double ratio = (double) zeros / cnt; std::printf("  muted ratio = %.3f (expected ~0.5)\n", ratio);
        CHECK (std::fabs (ratio - 0.5) < 0.1, "trans duty %.3f", ratio);
    }

    std::printf("[7] ROLL: ONの瞬間の音を1/4拍(6000smp)周期で繰り返すか\n");
    {
        std::vector<float> L, R; makeSignal (L, R, (int) (SR * 4)); auto in = L;
        Engine e; e.prepare (SR);
        Params off; off.fxType = kRoll; Params on = off; on.fxOn = true; on.fxBeat = beatNormFromIndex (kRoll, 2); on.fxLevel = 1.f;
        std::vector<std::pair<int, Params>> ch = { { 0, off }, { (int) (SR * 1.5), on } };
        run (e, L, R, off, host, 512, &ch);
        int s = (int) (SR * 2.5); double err = 0, ref = 0;
        for (int i = s; i < s + 12000; ++i) { err += std::fabs (L[(size_t) i] - L[(size_t) i - 6000]); ref += std::fabs (L[(size_t) i]); }
        std::printf("  periodicity error ratio = %.4f\n", err / std::max (ref, 1e-9));
        CHECK (err / ref < 0.02, "roll not periodic");
    }

    std::printf("[8] バックスピン: 逆再生→減速→離すとライブに戻る\n");
    {
        std::vector<float> L ((size_t) (SR * 6)), R;
        for (int i = 0; i < (int) L.size(); ++i) L[(size_t) i] = 0.5f * (float) std::sin (2 * kPi * 440.0 * i / SR);
        R = L; auto in = L;
        Engine e; e.prepare (SR);
        Params off; Params bs; bs.jogBackspin = true; bs.backspinTimeMs = 1500.f; bs.backspinRate = 4.f;
        std::vector<std::pair<int, Params>> ch = { { 0, off }, { (int) (SR * 2), bs }, { (int) (SR * 4), off } };
        run (e, L, R, off, host, 256, &ch);
        double f0 = zeroCrossRate (L, (int) (SR * 2.02), (int) (SR * 2.12));
        double f1 = zeroCrossRate (L, (int) (SR * 2.9), (int) (SR * 3.0));
        double f2 = zeroCrossRate (L, (int) (SR * 3.3), (int) (SR * 3.4));
        std::printf("  freq @0.05s=%.0fHz (expected ~%.0f)  @0.95s=%.0fHz  @1.35s=%.0fHz  (減速していくこと)\n", f0, 440.0 * 4 * 0.93, f1, f2);
        CHECK (f0 > f1 && f1 > f2, "backspin not decelerating");
        double d = 0; for (int i = (int) (SR * 4.2); i < (int) (SR * 5.5); ++i) d = std::max (d, (double) std::fabs (L[(size_t) i] - in[(size_t) i]));
        std::printf("  after release: max diff vs live = %g\n", d);
        CHECK (d < 1e-3, "did not return to live");
        Stat s = stat (L, 0, (int) L.size()); CHECK (!s.bad && s.peak <= 0.6f, "backspin peak %.2f", s.peak);
        // クリック検出(隣接サンプル差の最大)
        double maxStep = 0; for (int i = (int) (SR * 1.9); i < (int) (SR * 5); ++i) maxStep = std::max (maxStep, (double) std::fabs (L[(size_t) i] - L[(size_t) i - 1]));
        std::printf("  max sample step = %.3f (sine 440Hz@x4 speed would be ~%.3f)\n", maxStep, 0.5 * 2 * kPi * 440 * 4 / SR);
    }

    std::printf("[9] ブレーキ(VINYL STOP): 周波数が0へ落ちる\n");
    {
        std::vector<float> L ((size_t) (SR * 5)), R;
        for (int i = 0; i < (int) L.size(); ++i) L[(size_t) i] = 0.5f * (float) std::sin (2 * kPi * 440.0 * i / SR);
        R = L; Engine e; e.prepare (SR);
        Params off; Params br; br.jogBrake = true; br.brakeTimeMs = 1000.f;
        std::vector<std::pair<int, Params>> ch = { { 0, off }, { (int) (SR * 1), br } };
        run (e, L, R, off, host, 256, &ch);
        double a = zeroCrossRate (L, (int) (SR * 1.05), (int) (SR * 1.15));
        double b = zeroCrossRate (L, (int) (SR * 1.6), (int) (SR * 1.7));
        double c = zeroCrossRate (L, (int) (SR * 1.9), (int) (SR * 1.98));
        Stat s = stat (L, (int) (SR * 2.2), (int) (SR * 4.9));
        std::printf("  freq: %.0f -> %.0f -> %.0f Hz, rms after stop = %.5f\n", a, b, c, s.rms);
        CHECK (a > b && b > c, "brake not slowing"); CHECK (s.rms < 0.01, "should be silent after stop (held)");
    }

    std::printf("[10] スクラッチ: jogSpeedに追従(+1/-1/0)・リバース・ループ\n");
    {
        std::vector<float> L ((size_t) (SR * 6)), R;
        for (int i = 0; i < (int) L.size(); ++i) L[(size_t) i] = 0.5f * (float) std::sin (2 * kPi * 440.0 * i / SR);
        R = L; Engine e; e.prepare (SR);
        Params off, s1, s2, s3, rv, lp;
        off.jogLatencyMs = 125.f;
        s1 = off; s1.jogScratch = true; s1.jogSpeed = 2.f; s2 = s1; s2.jogSpeed = -2.f; s3 = s1; s3.jogSpeed = 0.f;
        rv = off; rv.jogReverse = true; lp = off; lp.jogLoop = true; lp.loopBeat = beatNormFromIndex (kEcho, 4); // 3/4拍
        std::vector<std::pair<int, Params>> ch = { { 0, off }, { (int) (SR * 1), s1 }, { (int) (SR * 1.08), off }, { (int) (SR * 1.5), s2 }, { (int) (SR * 2), s3 }, { (int) (SR * 2.5), off },
                                                   { (int) (SR * 3), rv }, { (int) (SR * 4), off }, { (int) (SR * 4.5), lp }, { (int) (SR * 5.5), off } };
        run (e, L, R, off, host, 256, &ch);
        double f1 = zeroCrossRate (L, (int) (SR * 1.02), (int) (SR * 1.075));
        double f2 = zeroCrossRate (L, (int) (SR * 1.65), (int) (SR * 1.95));
        Stat hold = stat (L, (int) (SR * 2.2), (int) (SR * 2.45));
        double f3 = zeroCrossRate (L, (int) (SR * 3.3), (int) (SR * 3.8));
        std::printf("  (先読み125ms) speed+2 -> %.0f Hz (880)  speed-2 -> %.0f Hz (880)  held rms=%.5f  reverse -> %.0f Hz (440)\n", f1, f2, hold.rms, f3);
        CHECK (std::fabs (f1 - 880) < 40, "scratch +2"); CHECK (std::fabs (f2 - 880) < 40, "scratch -2");
        CHECK (hold.rms < 0.01, "scratch hold not silent-ish"); CHECK (std::fabs (f3 - 440) < 30, "reverse pitch");
        Stat all = stat (L, 0, (int) L.size()); CHECK (!all.bad && all.peak < 0.6f, "jog peak %.2f", all.peak);
    }

    std::printf("[11] SOUND COLOR FX 6種: 中央=バイパス、左右で効果が出る\n");
    for (int t = 1; t < kNumColor; ++t)
    {
        std::vector<float> L, R; makeSignal (L, R, N); auto L0 = L;
        Engine e; e.prepare (SR); Params p; p.colorType = t; p.colorParam = 0.6f; p.colorKnob = 0.f;
        run (e, L, R, p, host);
        double dC = 0; for (int i = 0; i < N; ++i) dC = std::max (dC, (double) std::fabs (L[(size_t) i] - L0[(size_t) i]));
        double dl = 0, dr = 0;
        for (int side = 0; side < 2; ++side)
        {
            std::vector<float> A, B; makeSignal (A, B, N);
            Engine e2; e2.prepare (SR); Params q = p; q.colorKnob = side ? 0.7f : -0.7f;
            run (e2, A, B, q, host);
            Stat s = stat (A, (int) SR, N); double d = 0; for (int i = (int) SR; i < N; ++i) d += std::fabs (A[(size_t) i] - L0[(size_t) i]);
            (side ? dr : dl) = d; CHECK (!s.bad && s.peak < 6.f, "%s side %d peak %.2f", colorName (t), side, s.peak);
        }
        std::printf("  %-9s center maxdiff=%.2g  left diff=%.0f  right diff=%.0f\n", colorName (t), dC, dl, dr);
        CHECK (dC < 1e-3, "%s center not bypass", colorName (t));
        CHECK (dl > 1.0 && dr > 1.0, "%s no effect", colorName (t));
    }


    std::printf("[11b] SOUND COLOR FX: 説明書どおりの向きか(正弦波を通して測る)\n");
    {
        auto sine = [] (int n, double f, float amp) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2 * kPi * f * i / SR); return v; };
        auto rmsOf = [] (const std::vector<float>& a) { return stat (a, (int) SR, (int) a.size()).rms; };
        auto runColor = [&] (int type, float knob, float param, std::vector<float> in) { auto R2 = in; Engine e; e.prepare (SR); Params p; p.colorType = type; p.colorKnob = knob; p.colorParam = param; run (e, in, R2, p, host); return in; };
        const int n = (int) (SR * 3);

        // FILTER: 左=ローパス(高い音が減る) / 右=ハイパス(低い音が減る)
        double hiL = rmsOf (runColor (kColorFilter, -0.8f, 0.3f, sine (n, 6000, 0.5f))), hiR = rmsOf (runColor (kColorFilter, 0.8f, 0.3f, sine (n, 6000, 0.5f)));
        double loL = rmsOf (runColor (kColorFilter, -0.8f, 0.3f, sine (n, 100, 0.5f))), loR = rmsOf (runColor (kColorFilter, 0.8f, 0.3f, sine (n, 100, 0.5f)));
        std::printf ("  FILTER  6kHz: L=%.3f R=%.3f   100Hz: L=%.3f R=%.3f (入力 0.354)\n", hiL, hiR, loL, loR);
        CHECK (hiL < hiR * 0.2 && loR < loL * 0.2, "filter direction");

        // CRUSH: 右だけハイパスが加わる(低音が減る)
        double cL = rmsOf (runColor (kCrush, -0.8f, 0.5f, sine (n, 100, 0.5f))), cR = rmsOf (runColor (kCrush, 0.8f, 0.5f, sine (n, 100, 0.5f)));
        std::printf ("  CRUSH   100Hz: L=%.3f R=%.3f\n", cL, cR);
        CHECK (cR < cL * 0.5, "crush highpass on right only");

        // SWEEP: PARAMETERで決めた中心周波数で、左=ノッチ(減る) / 右=バンドパス(通る)
        const double fc = 150.0 * std::pow (10000.0 / 150.0, 0.5);
        double sL = rmsOf (runColor (kSweep, -0.9f, 0.5f, sine (n, fc, 0.5f))), sR = rmsOf (runColor (kSweep, 0.9f, 0.5f, sine (n, fc, 0.5f)));
        std::printf ("  SWEEP   at fc=%.0fHz: notch(L)=%.3f bandpass(R)=%.3f\n", fc, sL, sR);
        CHECK (sL < 0.05 && sR > 0.2, "sweep notch/bandpass");

        // NOISE: 入力が無音でも出る。左は低域寄り、右は高域寄り(ゼロクロスで比較)。中央は無音
        std::vector<float> z ((size_t) n, 0.f);
        auto nl = runColor (kNoise, -0.8f, 0.8f, z), nr = runColor (kNoise, 0.8f, 0.8f, z), nc = runColor (kNoise, 0.f, 0.8f, z);
        double zl = zeroCrossRate (nl, (int) SR, n), zr = zeroCrossRate (nr, (int) SR, n);
        std::printf ("  NOISE   zero-cross: L=%.0f R=%.0f Hz-ish, rms center=%.5f, rms L=%.3f\n", zl, zr, rmsOf (nc), rmsOf (nl));
        CHECK (zl < zr * 0.4 && rmsOf (nc) < 1e-4 && rmsOf (nl) > 0.01, "noise direction");

        // DUB ECHO: 左は中音域(高音は反響しない)/ 右は高音域(中音は反響しにくい)。インパルスの後の尾の帯域で確認
        auto tail = [&] (float knob, double f)
        {
            auto in = sine (n, f, 0.0f); for (int i = 0; i < 2400; ++i) in[(size_t) (48000 + i)] = 0.5f * (float) std::sin (2 * kPi * f * i / SR);
            auto out = runColor (kDubEcho, knob, 0.6f, in);
            return stat (out, 48000 + 9000, n).rms;     // 元の音が止んだ後のエコー
        };
        double mL = tail (-0.9f, 800), mR = tail (0.9f, 800), hL = tail (-0.9f, 8000), hR = tail (0.9f, 8000);
        std::printf ("  DUBECHO tail 800Hz: L=%.4f R=%.4f   8kHz: L=%.4f R=%.4f\n", mL, mR, hL, hR);
        CHECK (mL > mR * 3 && hR > hL * 3, "dub echo band");

        // SPACE: 左は中+低、右は中+高 (リバーブの尾の帯域)
        auto stail = [&] (float knob, double f)
        {
            auto in = sine (n, f, 0.0f); for (int i = 0; i < 2400; ++i) in[(size_t) (48000 + i)] = 0.5f * (float) std::sin (2 * kPi * f * i / SR);
            auto out = runColor (kSpace, knob, 0.6f, in);
            return stat (out, 48000 + 6000, n).rms;
        };
        double lL = stail (-0.9f, 100), lR = stail (0.9f, 100), qL = stail (-0.9f, 8000), qR = stail (0.9f, 8000);
        std::printf ("  SPACE   tail 100Hz: L=%.4f R=%.4f   8kHz: L=%.4f R=%.4f\n", lL, lR, qL, qR);
        CHECK (lL > lR * 2 && qR > qL * 2, "space band");
    }

    std::printf("[12] MOBIUS: 上昇/下降の向き(300-900Hzのスペクトル重心の変化方向)\n");
    for (int dirn = 0; dirn < 2; ++dirn)
    {
        std::vector<float> L ((size_t) (SR * 6)), R; Rng rng; for (auto& v : L) v = 0.3f * rng.white(); R = L;
        std::vector<float> in = L;
        Engine e; e.prepare (SR);
        Params p; p.fxOn = true; p.fxType = kMobius; p.fxBeat = beatNormFromIndex (kMobius, dirn ? 20 : 3); p.fxLevel = 1.f; // +4拍(2秒/oct) or -8拍
        run (e, L, R, p, host);
        std::vector<float> osc (L.size()); for (size_t i = 0; i < L.size(); ++i) osc[i] = L[i] - in[i];
        Stat s = stat (osc, (int) SR, (int) (SR * 6));
        // 0.25s窓の重心
        std::vector<double> cen;
        for (int w0 = (int) SR; w0 + 12000 <= (int) (SR * 6); w0 += 6000)
        {
            double num = 0, den = 0;
            for (int b = 0; b < 60; ++b)
            {
                double f = 300.0 * std::pow (3.0, b / 59.0), re = 0, im = 0;
                for (int i = 0; i < 12000; ++i) { double ph = 2 * kPi * f * i / SR; double wnd = 0.5 - 0.5 * std::cos (2 * kPi * i / 12000.0); re += osc[(size_t) (w0 + i)] * wnd * std::cos (ph); im += osc[(size_t) (w0 + i)] * wnd * std::sin (ph); }
                double a = std::sqrt (re * re + im * im); num += a * f; den += a;
            }
            cen.push_back (num / den);
        }
        int up = 0, dn = 0; for (size_t i = 1; i < cen.size(); ++i) { double d = cen[i] - cen[i - 1]; if (d > 0) ++up; else ++dn; }
        std::printf("  %s: rms=%.3f peak=%.2f  centroid up-steps=%d down-steps=%d\n", dirn ? "UP  (+4拍)" : "DOWN(-8拍)", s.rms, s.peak, up, dn);
        CHECK (!s.bad && s.rms > 0.01 && s.peak < 3.f, "mobius level");
        if (dirn) CHECK (up > dn * 2, "mobius up direction"); else CHECK (dn > up * 2, "mobius down direction");
    }

    std::printf("[13] ホストBPM不明時はフォールバックBPMで動く / 小さいブロック(1,7,64)でも同じ結果\n");
    {
        std::vector<float> Lr, Rr; makeSignal (Lr, Rr, (int) (SR * 3));
        std::vector<std::vector<float>> outs;
        for (int blk : { 64, 7, 480, 4096 })
        {
            auto L = Lr, R = Rr; Engine e; e.prepare (SR);
            Params p; p.fxOn = true; p.fxType = kEcho; p.fxLevel = 0.7f; p.fxSync = false; p.fxTimeMs = 200.f;
            HostInfo h; run (e, L, R, p, h, blk); outs.push_back (L);
        }
        double d = 0; for (size_t k = 1; k < outs.size(); ++k) for (size_t i = 0; i < outs[0].size(); ++i) d = std::max (d, (double) std::fabs (outs[k][i] - outs[0][i]));
        std::printf("  max diff across block sizes = %g\n", d);
        CHECK (d < 1e-4, "block size dependence");
    }


    // ---------------------------------------------------------------- 新機能: RELEASE / SLIP / QUANTIZE / ループ操作 / DAW連動
    std::printf("[15] RELEASE: 止めた盤を放すと、設定時間で等速まで立ち上がる(周波数が上がっていく)\n");
    {
        const int len = (int) (SR * 8), tOn = 94 * 512, tOff = 282 * 512;
        std::vector<float> L ((size_t) len), R;
        for (int i = 0; i < len; ++i) L[(size_t) i] = 0.5f * (float) std::sin (2 * kPi * 1000.0 * i / SR);
        R = L;
        Params off; off.brakeTimeMs = 500; off.releaseTimeMs = 400; off.slip = false;
        Params on = off; on.jogBrake = true;
        Engine e; e.prepare (SR);
        run (e, L, R, off, host, 512, new std::vector<std::pair<int, Params>> { { tOn, on }, { tOff, off } });
        const double early = zeroCrossRate (L, tOff, tOff + 4800), late = zeroCrossRate (L, tOff + (int) (0.6 * SR), tOff + (int) (0.9 * SR));
        const double held = stat (L, tOff - 9600, tOff).rms;
        std::printf("  止めている間 rms=%.4f / 放した直後100ms=%.0fHz / 0.6〜0.9秒後=%.0fHz\n", held, early, late);
        CHECK (held < 0.01, "brake hold not silent"); CHECK (early < late * 0.5, "no spin-up"); CHECK (std::fabs (late - 1000.0) < 60.0, "does not reach normal speed");
    }

    std::printf("[16] SLIP: OFFは離した位置から続ける(遅れが残る・式どおり) / ONはライブへ戻る\n");
    {
        const int len = (int) (SR * 8), tOn = 94 * 512, tOff = 282 * 512;
        auto in = noiseSig (len);
        Params base; base.brakeTimeMs = 500; base.releaseTimeMs = 400;
        for (int slip = 0; slip < 2; ++slip)
        {
            auto L = in, R = in; Params off = base, on = base; off.slip = on.slip = slip != 0; on.jogBrake = true;
            Engine e; e.prepare (SR);
            run (e, L, R, off, host, 512, new std::vector<std::pair<int, Params>> { { tOn, on }, { tOff, off } });
            if (! slip)
            {
                const double hold = tOff - tOn, T = 0.5 * SR, relT = 0.4 * SR;
                const double expect = hold - 0.4 * T + 0.4 * relT;
                double c; const int lag = bestLag (in, L, tOff + (int) (0.9 * SR), 2048, (int) (3 * SR), &c);
                std::printf("  SLIP OFF: 遅れ=%d サンプル(式では%.0f) corr=%.3f\n", lag, expect, c);
                CHECK (std::fabs (lag - expect) < 80, "lag %d vs %.0f", lag, expect); CHECK (c > 0.95, "lag corr");
            }
            else
            {
                double d = 0; for (int i = tOff + (int) (1.0 * SR); i < tOff + (int) (2.0 * SR); ++i) d = std::max (d, (double) std::fabs (L[(size_t) i] - in[(size_t) i]));
                std::printf("  SLIP ON : 放して1秒後の入力との差 = %g\n", d);
                CHECK (d < 1e-4, "slip on did not return to live");
            }
        }
    }

    std::printf("[17] QUANTIZE: 開始がDAWの拍の頭までちょうど待たされる / SLIP OFFの遅れが拍の整数倍に揃う\n");
    {
        const int len = (int) (SR * 8), tOn = 94 * 512;
        auto in = noiseSig (len);
        for (int q = 0; q < 2; ++q)
        {
            auto L = in, R = in; Params off; off.backspinTimeMs = 1200; off.quantize = q != 0; off.quantBeats = 1.f;
            Params on = off; on.jogBackspin = true;
            Engine e; e.prepare (SR);
            run (e, L, R, off, host, 512, new std::vector<std::pair<int, Params>> { { tOn, on } });
            int first = -1; for (int i = 0; i < len; ++i) if (std::fabs (L[(size_t) i] - in[(size_t) i]) > 1e-7f) { first = i; break; }
            const int expect = q ? 72000 : tOn;   // tOnは2.0027拍。次の拍の頭は3.0拍=72000サンプル
            std::printf("  QUANTIZE %s: 効きはじめ=%d (期待 %d)\n", q ? "ON " : "OFF", first, expect);
            CHECK (first >= expect && first <= expect + 1, "onset %d vs %d", first, expect);
        }
        // 遅れの拍揃え: 72000(3拍)で押して、放した位置から続ける
        Params off; off.brakeTimeMs = 500; off.releaseTimeMs = 400; off.slip = false; off.quantize = true; off.quantBeats = 1.f;
        Params on = off; on.jogBrake = true;
        auto L = in, R = in; Engine e; e.prepare (SR);
        run (e, L, R, off, host, 512, new std::vector<std::pair<int, Params>> { { tOn, on }, { 282 * 512, off } });
        double c; const int lag = bestLag (in, L, 282 * 512 + (int) (1.2 * SR), 2048, (int) (3 * SR), &c);
        std::printf("  SLIP OFF + QUANTIZE: 遅れ=%d (1拍=24000の整数倍) corr=%.3f\n", lag, c);
        CHECK (std::abs (lag % 24000) <= 4 || std::abs (lag % 24000 - 24000) <= 4, "lag not on beat: %d", lag); CHECK (c > 0.9, "lag corr %.2f", c);
    }

    std::printf("[18] ループ: 1/2Xで長さが半分になる / EXITで抜けてライブへ / RELOOPで直前と同じ区間に入り直す\n");
    {
        const int len = (int) (SR * 8);
        auto in = noiseSig (len); auto L = in, R = in;
        Params base; base.loopBeat = beatNormFromIndex (kEcho, 5);     // 1拍(120BPMで24000サンプル)
        Params a = base; a.loopLatch = true;
        Params half = a; half.loopBeat = beatNormFromIndex (kEcho, 3); // 1/2拍(12000)
        Params ex = base; ex.loopBeat = half.loopBeat;
        Params again = ex; again.loopLatch = true;
        Engine e; e.prepare (SR);
        const int t1 = 94 * 512, t2 = 282 * 512, t3 = 375 * 512, t4 = 422 * 512;
        run (e, L, R, base, host, 512, new std::vector<std::pair<int, Params>> { { t1, a }, { t2, half }, { t3, ex }, { t4, again } });
        const int w = 2048, at1 = (int) (2.5 * SR), at2 = (int) (3.6 * SR);
        auto corrAt = [&] (int at, int lag) { double c = 0, e1 = 0, e2 = 0; for (int i = 0; i < w; ++i) { double x = L[(size_t) (at + i)], y = L[(size_t) (at + i - lag)]; c += x * y; e1 += x * x; e2 += y * y; } return c / std::sqrt (e1 * e2 + 1e-30); };
        const double c24 = corrAt (at1, 24000), c12 = corrAt (at1, 12000), d12 = corrAt (at2, 12000);
        std::printf("  1拍ループ中: 周期24000の相関=%.3f 12000の相関=%.3f / 1/2X後: 周期12000の相関=%.3f\n", c24, c12, d12);
        CHECK (c24 > 0.98 && std::fabs (c12) < 0.3, "1beat loop"); CHECK (d12 > 0.98, "half loop");
        double d = 0; for (int i = t3 + (int) (0.3 * SR); i < t3 + (int) (0.5 * SR); ++i) d = std::max (d, (double) std::fabs (L[(size_t) i] - in[(size_t) i]));
        std::printf("  EXIT後: 入力との差=%g\n", d); CHECK (d < 1e-4, "exit did not return to live");
        const double rc = bestShiftCorr (L, at2, L, (int) (6.0 * SR), w, 12000);
        std::printf("  RELOOP: 最初の1/2Xループと同じ区間か(相関)=%.3f\n", rc); CHECK (rc > 0.98, "reloop segment differs");
    }

    std::printf("[19] DAW連動: 停止/シークで遅れを捨てる、シーク前の録音は巻き戻しに使わない\n");
    {
        const int blk = 512; const int total = 40; Engine e; e.prepare (SR);
        Params off; off.slip = false; off.brakeTimeMs = 300; off.releaseTimeMs = 200;
        auto feed = [&] (int blocks, double ppq0, const Params& p, bool silent)
        {
            std::vector<float> L, R;
            for (int b = 0; b < blocks; ++b)
            {
                L = silent ? std::vector<float> ((size_t) blk, 0.f) : noiseSig (blk, 0.5f); R = L; HostInfo h = host; h.ppq = ppq0 + b * blk * 2.0 / SR;
                e.process (L.data(), R.data(), blk, p, h, 0);
            }
            return L;
        };
        Params on = off; on.jogBrake = true;
        feed (total, 0.0, off, false); feed (total, total * blk * 2.0 / SR, on, false);
        const double ppqAfter = 2 * total * blk * 2.0 / SR;
        feed (60, ppqAfter, off, false);
        const double lagBefore = e.jogLagSamples();
        std::printf("  シーク前: 遅れ=%.0fサンプル engaged=%d\n", lagBefore, (int) e.jogEngaged());
        CHECK (lagBefore > 1000 && e.jogEngaged(), "no lag to test");
        feed (1, ppqAfter + 500.0, off, true);                              // DAWがシークした
        std::printf("  シーク後: 遅れ=%.0f engaged=%d\n", e.jogLagSamples(), (int) e.jogEngaged());
        CHECK (! e.jogEngaged() && e.jogLagSamples() == 0.0, "lag not cleared on seek");
        Params bs = off; bs.jogBackspin = true; bs.backspinRate = 5.f;
        feed (4, ppqAfter + 500.0 + 0.1, off, true);                        // シーク後は無音を少し流す
        auto out = feed (8, ppqAfter + 500.2, bs, true);                    // 巻き戻す: シーク前の音(うるさい)を拾ってはいけない
        const double r = stat (out, 0, blk).rms;
        std::printf("  シーク後の巻き戻し出力 rms=%.5f (シーク前の音は拾わない)\n", r);
        CHECK (r < 0.01, "backspin read pre-seek audio");
        // 対照: シークしなければ、同じ操作で巻き戻した先の音が出る(上の無音がたまたまでない証拠)
        Engine e2; e2.prepare (SR);
        auto feed2 = [&] (int blocks, double ppq0, const Params& p, bool silent)
        {
            std::vector<float> L, R;
            for (int b = 0; b < blocks; ++b)
            {
                L = silent ? std::vector<float> ((size_t) blk, 0.f) : noiseSig (blk, 0.5f); R = L; HostInfo h = host; h.ppq = ppq0 + b * blk * 2.0 / SR;
                e2.process (L.data(), R.data(), blk, p, h, 0);
            }
            return L;
        };
        feed2 (total, 0.0, off, false);
        const double pp = total * blk * 2.0 / SR;
        feed2 (4, pp, off, true);
        auto out2 = feed2 (8, pp + 4 * blk * 2.0 / SR, bs, true);
        const double r2 = stat (out2, 0, blk).rms;
        std::printf("  対照(シークなし)の巻き戻し出力 rms=%.4f\n", r2);
        CHECK (r2 > 0.05, "control: backspin should read earlier audio");
    }

    std::printf("[14] パラメータを激しく動かしても爆発しない(ランダム操作ファズ)\n");
    {
        std::vector<float> L, R; makeSignal (L, R, (int) (SR * 20)); Engine e; e.prepare (SR);
        std::srand (7); Params p; HostInfo h = host; int bad = 0; float peak = 0;
        const int blk = 256;
        for (int off = 0; off + blk <= (int) L.size(); off += blk)
        {
            if ((off / blk) % 20 == 0)
            {
                auto rnd = [] { return (float) std::rand() / RAND_MAX; };
                p.fxOn = rnd() > 0.3f; p.fxType = std::rand() % kNumFx; p.fxBandMask = std::rand() % 8; p.fxSync = rnd() > 0.3f;
                p.fxBeat = rnd(); p.fxTimeMs = 1.f + rnd() * 31999.f; p.fxLevel = rnd(); p.xpadTouch = rnd() > 0.7f; p.xpad = rnd();
                p.colorType = std::rand() % kNumColor; p.colorKnob = rnd() * 2 - 1; p.colorParam = rnd(); p.centerLock = rnd() > 0.5f;
                p.jogBackspin = rnd() > 0.85f; p.jogBrake = rnd() > 0.85f; p.jogReverse = rnd() > 0.85f; p.jogScratch = rnd() > 0.85f; p.jogLoop = rnd() > 0.85f;
                p.jogSpeed = rnd() * 8 - 4; p.loopBeat = rnd(); p.backspinTimeMs = 200 + rnd() * 3800; p.brakeTimeMs = 50 + rnd() * 4950; p.backspinRate = 1 + rnd() * 9;
                p.slip = rnd() > 0.4f; p.releaseTimeMs = 50 + rnd() * 4950; p.releaseBeats = rnd() > 0.6f ? 0.25f * (float) (1 + std::rand() % 16) : 0.f;
                p.loopLatch = rnd() > 0.8f; p.jogResync = rnd() > 0.9f; p.quantize = rnd() > 0.5f; p.quantBeats = 0.125f * (float) (1 << (std::rand() % 4));
                h.bpm = rnd() > 0.2f ? 60 + rnd() * 140 : 0;
            }
            h.ppq = off * 2.0 / SR * 1.0;
            e.process (L.data() + off, R.data() + off, blk, p, h, 0);
            for (int i = off; i < off + blk; ++i) { if (!std::isfinite (L[(size_t) i]) || !std::isfinite (R[(size_t) i])) ++bad; peak = std::max (peak, std::max (std::fabs (L[(size_t) i]), std::fabs (R[(size_t) i]))); }
        }
        std::printf("  non-finite=%d peak=%.2f\n", bad, peak);
        CHECK (bad == 0, "NaN/Inf"); CHECK (peak < 30.f, "runaway peak %.1f", peak);
    }

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
