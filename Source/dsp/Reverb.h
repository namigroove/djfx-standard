#pragma once
#include "Common.h"

namespace djfx
{
// Freeverb系のステレオリバーブ(モノ入力→ステレオ出力)
class Reverb
{
public:
    void prepare (float sr)
    {
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apT[4]   = { 556, 441, 341, 225 };
        const float k = sr / 44100.f;
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 8; ++i) comb[c][i].init ((int) ((combT[i] + 23 * c) * k));
            for (int i = 0; i < 4; ++i) ap[c][i].init ((int) ((apT[i] + 23 * c) * k));
        }
    }
    void clear()
    {
        for (auto& ch : comb) for (auto& c : ch) c.clear();
        for (auto& ch : ap) for (auto& a : ch) a.clear();
    }
    // decay:0..1, damp:0..1
    void setParams (float decay, float damp) { fb = 0.70f + 0.28f * clampf (decay, 0.f, 1.f); dp = damp; }
    inline void tick (float in, float& outL, float& outR)
    {
        const float x = in * 0.015f;
        float o[2] = { 0.f, 0.f };
        for (int c = 0; c < 2; ++c)
        {
            float s = 0.f;
            for (int i = 0; i < 8; ++i) s += comb[c][i].tick (x, fb, dp);
            for (int i = 0; i < 4; ++i) s = ap[c][i].tick (s);
            o[c] = s * 3.f;
        }
        outL = o[0]; outR = o[1];
    }

private:
    struct Comb
    {
        std::vector<float> b; int i = 0; float z = 0.f;
        void init (int n) { b.assign ((size_t) std::max (n, 1), 0.f); i = 0; z = 0.f; }
        void clear() { std::fill (b.begin(), b.end(), 0.f); z = 0.f; }
        inline float tick (float x, float fbk, float damp)
        {
            float o = b[(size_t) i];
            z = o * (1.f - damp) + z * damp;
            b[(size_t) i] = x + z * fbk;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    struct AP
    {
        std::vector<float> b; int i = 0;
        void init (int n) { b.assign ((size_t) std::max (n, 1), 0.f); i = 0; }
        void clear() { std::fill (b.begin(), b.end(), 0.f); }
        inline float tick (float x)
        {
            float bo = b[(size_t) i];
            float o = -x + bo;
            b[(size_t) i] = x + bo * 0.5f;
            if (++i >= (int) b.size()) i = 0;
            return o;
        }
    };
    Comb comb[2][8];
    AP ap[2][4];
    float fb = 0.8f, dp = 0.25f;
};
} // namespace djfx
