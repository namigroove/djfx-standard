#pragma once
// 依存なしの小さなDSP部品。JUCEには依存しない(オフラインテストで単体ビルドするため)。
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace djfx
{
constexpr double kPi = 3.14159265358979323846;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }
inline float smooth01 (float x) { x = clampf (x, 0.f, 1.f); return x * x * (3.f - 2.f * x); }
inline float expMap (float t, float lo, float hi) { return lo * std::pow (hi / lo, t); } // t:0..1

// 1極スムーサー(時定数ms)
struct Smoother
{
    float z = 0.f, a = 0.f;
    void setTime (float ms, float sr) { a = std::exp (-1.f / (0.001f * ms * sr)); }
    void reset (float v) { z = v; }
    inline float tick (float target) { z = target + a * (z - target); return z; }
};

// 乱数(ノイズ用)
struct Rng
{
    uint32_t s = 0x1234abcd;
    inline float white() // -1..1
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.f / 2147483648.f);
    }
};

// 小数ディレイ(Hermite補間)。read(d): 最新サンプルからdサンプル前。d>=1推奨。
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare (int maxSamples)
    {
        int n = 1;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float d) const
    {
        int i = (int) d;
        float f = d - (float) i;
        int base = w - 1 - i;
        float y0 = buf[(size_t) ((base + 1) & mask)];
        float y1 = buf[(size_t) ((base) & mask)];
        float y2 = buf[(size_t) ((base - 1) & mask)];
        float y3 = buf[(size_t) ((base - 2) & mask)];
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    inline float readLin (float d) const
    {
        int i = (int) d;
        float f = d - (float) i;
        int base = w - 1 - i;
        return buf[(size_t) (base & mask)] * (1.f - f) + buf[(size_t) ((base - 1) & mask)] * f;
    }
};

// TPT State Variable Filter
struct SVF
{
    float s1 = 0.f, s2 = 0.f, g = 0.f, k = 1.f, a1 = 0.f, a2 = 0.f, a3 = 0.f;
    void set (float fc, float q, float sr)
    {
        fc = clampf (fc, 10.f, 0.45f * sr);
        g = std::tan ((float) kPi * fc / sr);
        k = 1.f / std::max (q, 0.05f);
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void reset() { s1 = s2 = 0.f; }
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        float v3 = x - s2;
        float v1 = a1 * s1 + a2 * v3;
        float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.f * v1 - s1;
        s2 = 2.f * v2 - s2;
        bp = v1; lp = v2; hp = x - k * v1 - v2;
    }
    inline float lp (float x) { float l, b, h; tick (x, l, b, h); return l; }
    inline float hp (float x) { float l, b, h; tick (x, l, b, h); return h; }
};

// 1次オールパス(フェイザー用)
struct AllpassFirst
{
    float z = 0.f;
    inline float tick (float x, float a) { float y = a * x + z; z = x - a * y; return y; }
};

// ピッチシフタ(2タップ・ディレイ変調式)
struct PitchShifter
{
    DelayLine dl;
    float phase = 0.f, win = 4096.f;
    void prepare (float sr)
    {
        win = 0.06f * sr;
        dl.prepare ((int) win + 16);
        phase = 0.f;
    }
    void clear() { dl.clear(); phase = 0.f; }
    inline float tick (float x, float ratio)
    {
        dl.push (x);
        phase += (1.f - ratio) / win;
        phase -= std::floor (phase);
        float p2 = phase + 0.5f; p2 -= std::floor (p2);
        float s1 = std::sin ((float) kPi * phase);
        float s2 = std::sin ((float) kPi * p2);
        return dl.read (phase * win + 2.f) * s1 * s1 + dl.read (p2 * win + 2.f) * s2 * s2;
    }
};
} // namespace djfx
