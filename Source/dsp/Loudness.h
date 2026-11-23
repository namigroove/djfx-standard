#pragma once
// ラウドネス測定(ITU-R BS.1770 / EBU R128): K特性フィルタ → 400ms窓(100msごと)のモメンタリー、3秒のショートターム、
// ゲート付きのインテグレート(絶対-70LUFS、相対-10LU)。JUCEに依存しない。
#include <cmath>
#include <vector>
#include <algorithm>

namespace djfx
{
class Loudness
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        // ステージ1: 高域を持ち上げるシェルフ / ステージ2: 低域を切るハイパス
        { const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
          const double K = std::tan (kPi * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
          const double a0 = 1.0 + K / Q + K * K;
          sh = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 }; }
        { const double f0 = 38.13547087602444, Q = 0.5003270373238773;
          const double K = std::tan (kPi * f0 / sr), a0 = 1.0 + K / Q + K * K;
          hp = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 }; }
        sub = std::max (1, (int) std::lround (sr * 0.1));
        reset();
    }
    void reset()
    {
        for (auto& c : st) c = {};
        acc = 0.0; accN = 0; ring.assign (30, 0.0); ringW = 0; filled = 0;
        hist.assign (kBins, 0); histE.assign (kBins, 0.0); histN = 0;
        m = s = i = -200.f; peak = 0.f;
    }
    void process (const float* L, const float* R, int n)
    {
        for (int k = 0; k < n; ++k)
        {
            const double l = L[k], r = R != nullptr ? R[k] : L[k];
            peak = std::max (peak, (float) std::max (std::fabs (l), std::fabs (r)));
            const double a = filt (0, l), b = filt (1, r);
            acc += a * a + b * b;
            if (++accN >= sub) endSub();
        }
    }
    float momentary() const { return m; }
    float shortTerm() const { return s; }
    float integrated() const { return i; }
    float peakDb() const { return peak > 1e-6f ? 20.f * std::log10 (peak) : -120.f; }
    void resetPeak() { peak = 0.f; }

private:
    static constexpr double kPi = 3.14159265358979323846;
    struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct St { double s1[2] = { 0, 0 }, s2[2] = { 0, 0 }, h1[2] = { 0, 0 }, h2[2] = { 0, 0 }; };
    static constexpr int kBins = 900;                         // -70..+20 LUFS を0.1刻み
    double filt (int ch, double x)
    {
        auto& z = st[ch];
        double y = sh.b0 * x + z.s1[0]; z.s1[0] = sh.b1 * x - sh.a1 * y + z.s2[0]; z.s2[0] = sh.b2 * x - sh.a2 * y;   // 転置直接形II
        double o = hp.b0 * y + z.h1[0]; z.h1[0] = hp.b1 * y - hp.a1 * o + z.h2[0]; z.h2[0] = hp.b2 * y - hp.a2 * o;
        return o;
    }
    static float toLufs (double e) { return e > 1e-12 ? (float) (-0.691 + 10.0 * std::log10 (e)) : -200.f; }
    void endSub()
    {
        ring[(size_t) (ringW % 30)] = acc / (double) sub; ++ringW; acc = 0.0; accN = 0; filled = std::min (filled + 1, 30);
        auto mean = [&] (int count) { double t = 0; for (int k = 1; k <= count; ++k) t += ring[(size_t) ((ringW - k + 3000) % 30)]; return t / count; };
        if (filled >= 4)
        {
            const double e = mean (4); m = toLufs (e);
            if (m > -70.f)
            {
                const int bin = std::clamp ((int) std::floor ((m + 70.f) * 10.f), 0, kBins - 1);
                ++hist[(size_t) bin]; histE[(size_t) bin] += e; ++histN;
                // 相対ゲート: 絶対ゲートを通ったブロックの平均 -10LU
                double sumE = 0; long long cnt = 0;
                for (int b = 0; b < kBins; ++b) { sumE += histE[(size_t) b]; cnt += hist[(size_t) b]; }
                const float rel = toLufs (sumE / (double) cnt) - 10.f;
                double gE = 0; long long gN = 0;
                for (int b = std::max (0, (int) std::floor ((rel + 70.f) * 10.f)); b < kBins; ++b) { gE += histE[(size_t) b]; gN += hist[(size_t) b]; }
                if (gN > 0) i = toLufs (gE / (double) gN);
            }
        }
        if (filled >= 30) s = toLufs (mean (30));
    }
    double sr = 48000.0; Bq sh, hp; St st[2]; int sub = 4800;
    double acc = 0.0; int accN = 0; std::vector<double> ring; long long ringW = 0; int filled = 0;
    std::vector<long long> hist; std::vector<double> histE; long long histN = 0;
    float m = -200.f, s = -200.f, i = -200.f, peak = 0.f;
};
}
