// DevilutionX GPU scaler - Metal Shading Language sources for the Metal
// backend (macOS). Compiled at runtime by SDL from MSL source.
//
// Binding conventions follow SDL_CreateGPUShader for MSL fragment shaders:
//   textures: [[texture(0)]..], samplers: [[sampler(0)]..]
//   uniform buffers: first in the [[buffer]] argument table by SDL slot
//
// Ports of the shaders in shaders/vulkan/:
//  - hqx: Copyright (C) 2003 Maxim Stepin, (C) 2010 Cameron Zemek,
//         (C) 2014 Jules Blok. LGPL 2.1 or later.
//  - ScaleFX: Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de. MIT.
// GLSL column-major matrices have been transposed for the row-major
// constructors of MSL.

#pragma once

namespace devilution::gpu::msl {

static constexpr const char *kVertex = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct VSOut {
    float4 pos [[position]];
    float2 uv [[user(locn0)]];
};

vertex VSOut fullscreenVS(uint vid [[vertex_id]])
{
    // Fullscreen triangle: (-1,-1), (3,-1), (-1,3) - both axes overscan to 3.
    float2 pos = float2(float((vid & 1u) << 2) - 1.0, float((vid & 2u) << 1) - 1.0);
    VSOut o;
    o.uv = (pos + 1.0) * 0.5;
    o.pos = float4(pos, 0.0, 1.0);
    return o;
}
)MSL";

static constexpr const char *kHqxIndex = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams;
};

constant float3 yuvThreshold = float3(48.0 / 255.0, 7.0 / 255.0, 6.0 / 255.0);
constant float3x3 yuv = float3x3(
    float3(0.299, 0.587, 0.114),
    float3(-0.169, -0.331, 0.5),
    float3(0.5, -0.419, -0.081));
constant float3 yuvOffset = float3(0.0, 0.5, 0.5);

static bool diff(float3 a, float3 b)
{
    return any(abs((a + yuvOffset) - (b + yuvOffset)) > yuvThreshold);
}

fragment float4 hqxIndexPS(float2 vUV [[user(locn0)]],
    texture2d<float> uTex [[texture(0)]],
    sampler uSamp [[sampler(0)]],
    constant FragUniform &u [[buffer(0)]])
{
    float dx = u.uSourceSize.z;
    float dy = u.uSourceSize.w;

    float3 w1 = yuv * uTex.sample(uSamp, vUV + float2(-dx, -dy)).rgb;
    float3 w2 = yuv * uTex.sample(uSamp, vUV + float2(0.0, -dy)).rgb;
    float3 w3 = yuv * uTex.sample(uSamp, vUV + float2(dx, -dy)).rgb;
    float3 w4 = yuv * uTex.sample(uSamp, vUV + float2(-dx, 0.0)).rgb;
    float3 w5 = yuv * uTex.sample(uSamp, vUV).rgb;
    float3 w6 = yuv * uTex.sample(uSamp, vUV + float2(dx, 0.0)).rgb;
    float3 w7 = yuv * uTex.sample(uSamp, vUV + float2(-dx, dy)).rgb;
    float3 w8 = yuv * uTex.sample(uSamp, vUV + float2(0.0, dy)).rgb;
    float3 w9 = yuv * uTex.sample(uSamp, vUV + float2(dx, dy)).rgb;

    float3 pattern_1 = float3(diff(w5, w1), diff(w5, w2), diff(w5, w3));
    float3 pattern_2 = float3(diff(w5, w4), false, diff(w5, w6));
    float3 pattern_3 = float3(diff(w5, w7), diff(w5, w8), diff(w5, w9));
    float4 crossPat = float4(diff(w4, w2), diff(w2, w6), diff(w8, w4), diff(w6, w8));

    float indexX = dot(pattern_1, float3(1.0, 2.0, 4.0))
        + dot(pattern_2, float3(8.0, 0.0, 16.0))
        + dot(pattern_3, float3(32.0, 64.0, 128.0));
    float indexY = dot(crossPat, float4(1.0, 2.0, 4.0, 8.0));

    return float4(indexX / 255.0, indexY / 15.0, 0.0, 1.0);
}
)MSL";

static constexpr const char *kHqxBlend = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams;
};

fragment float4 hqxBlendPS(float2 vUV [[user(locn0)]],
    texture2d<float> uIndex [[texture(0)]],
    texture2d<float> uLUT [[texture(1)]],
    texture2d<float> uRef [[texture(2)]],
    sampler uSamp0 [[sampler(0)]],
    sampler uSamp1 [[sampler(1)]],
    sampler uSamp2 [[sampler(2)]],
    constant FragUniform &u [[buffer(0)]])
{
    float S = u.uParams.x;
    float2 fp = fract(vUV * u.uSourceSize.xy);
    float2 quad = sign(-0.5 + fp);

    float dx = u.uSourceSize.z;
    float dy = u.uSourceSize.w;

    float3 pixels[4];
    pixels[0] = uRef.sample(uSamp2, vUV).rgb;
    pixels[1] = uRef.sample(uSamp2, vUV + float2(dx, dy) * quad).rgb;
    pixels[2] = uRef.sample(uSamp2, vUV + float2(dx, 0.0) * quad).rgb;
    pixels[3] = uRef.sample(uSamp2, vUV + float2(0.0, dy) * quad).rgb;

    float2 index = uIndex.sample(uSamp0, vUV).xy * float2(255.0, 15.0 * (S * S));
    index.y += dot(floor(fp * S), float2(1.0, S));

    float2 stp = 1.0 / float2(256.0, 16.0 * (S * S));
    float4 weights = uLUT.sample(uSamp1, index * stp + stp * 0.5);
    float4 w = weights / dot(weights, float4(1.0));

    return float4(w.x * pixels[0] + w.y * pixels[1] + w.z * pixels[2] + w.w * pixels[3], 1.0);
}
)MSL";

static constexpr const char *kSfxPass0 = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams;
};

// Reference: http://www.compuphase.com/cmetric.htm
static float dist(float3 A, float3 B)
{
    float r = 0.5 * (A.r + B.r);
    float3 d = A - B;
    float3 c = float3(2 + r, 4, 3 - r);

    return sqrt(dot(c * d, d)) / 3;
}

fragment float4 sfxPass0PS(float2 vUV [[user(locn0)]],
    texture2d<float> uTex [[texture(0)]],
    sampler uSamp [[sampler(0)]],
    constant FragUniform &u [[buffer(0)]])
{
    float dx = u.uSourceSize.z;
    float dy = u.uSourceSize.w;

    float3 A = uTex.sample(uSamp, vUV + float2(-dx, -dy)).rgb;
    float3 B = uTex.sample(uSamp, vUV + float2(0.0, -dy)).rgb;
    float3 C = uTex.sample(uSamp, vUV + float2(dx, -dy)).rgb;
    float3 E = uTex.sample(uSamp, vUV).rgb;
    float3 F = uTex.sample(uSamp, vUV + float2(dx, 0.0)).rgb;

    return float4(dist(E, A), dist(E, B), dist(E, C), dist(E, F));
}
)MSL";

static constexpr const char *kSfxPass1 = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams; // x = SFX_CLR, y = SFX_SAA
};

// corner strength
static float str(float d, float2 a, float2 b, float clr, float saa)
{
    float diff = a.x - a.y;
    float wght1 = max(clr - d, 0.0) / clr;
    float wght2 = clamp((1 - d) + ((min(a.x, b.x) + a.x > min(a.y, b.y) + a.y) ? diff : -diff), 0.0, 1.0);
    return (saa == 1.0 || 2.0 * d < a.x + a.y) ? (wght1 * wght2) * (a.x * a.y) : 0.0;
}

fragment float4 sfxPass1PS(float2 vUV [[user(locn0)]],
    texture2d<float> uTex [[texture(0)]],
    sampler uSamp [[sampler(0)]],
    constant FragUniform &u [[buffer(0)]])
{
    float dx = u.uSourceSize.z;
    float dy = u.uSourceSize.w;

    float4 A = uTex.sample(uSamp, vUV + float2(-dx, -dy));
    float4 B = uTex.sample(uSamp, vUV + float2(0.0, -dy));
    float4 D = uTex.sample(uSamp, vUV + float2(-dx, 0.0));
    float4 E = uTex.sample(uSamp, vUV);
    float4 F = uTex.sample(uSamp, vUV + float2(dx, 0.0));
    float4 G = uTex.sample(uSamp, vUV + float2(-dx, dy));
    float4 H = uTex.sample(uSamp, vUV + float2(0.0, dy));
    float4 I = uTex.sample(uSamp, vUV + float2(dx, dy));

    float4 res;
    res.x = str(D.z, float2(D.w, E.y), float2(A.w, D.y), u.uParams.x, u.uParams.y);
    res.y = str(F.x, float2(E.w, E.y), float2(B.w, F.y), u.uParams.x, u.uParams.y);
    res.z = str(H.z, float2(E.w, H.y), float2(H.w, I.y), u.uParams.x, u.uParams.y);
    res.w = str(H.x, float2(D.w, H.y), float2(G.w, G.y), u.uParams.x, u.uParams.y);

    return res;
}
)MSL";

static constexpr const char *kSfxPass2 = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams;
};

static float4 dom(float3 x, float3 y, float3 z, float3 w)
{
    return 2 * float4(x.y, y.y, z.y, w.y) - (float4(x.x, y.x, z.x, w.x) + float4(x.z, y.z, z.z, w.z));
}

// component-wise helpers matching the GLSL macros of the original shader
static float4 GE(float4 x, float4 y) { return select(float4(0.0), float4(1.0), x > y); }
static float4 LE(float4 x, float4 y) { return select(float4(0.0), float4(1.0), x < y); }
static float4 LEQ(float4 x, float4 y) { return select(float4(0.0), float4(1.0), x <= y); }

static float clearF(float2 crn, float2 a, float2 b)
{
    return (crn.x >= max(min(a.x, a.y), min(b.x, b.y))) && (crn.y >= max(min(a.x, b.y), min(b.x, a.y))) ? 1.0 : 0.0;
}

fragment float4 sfxPass2PS(float2 vUV [[user(locn0)]],
    texture2d<float> uStr [[texture(0)]],
    texture2d<float> uMetric [[texture(1)]],
    sampler uSamp0 [[sampler(0)]],
    sampler uSamp1 [[sampler(1)]],
    constant FragUniform &u [[buffer(0)]])
{
    float dx = u.uSourceSize.z;
    float dy = u.uSourceSize.w;

    float4 A = uMetric.sample(uSamp1, vUV + float2(-dx, -dy));
    float4 B = uMetric.sample(uSamp1, vUV + float2(0.0, -dy));
    float4 D = uMetric.sample(uSamp1, vUV + float2(-dx, 0.0));
    float4 E = uMetric.sample(uSamp1, vUV);
    float4 F = uMetric.sample(uSamp1, vUV + float2(dx, 0.0));
    float4 G = uMetric.sample(uSamp1, vUV + float2(-dx, dy));
    float4 H = uMetric.sample(uSamp1, vUV + float2(0.0, dy));
    float4 I = uMetric.sample(uSamp1, vUV + float2(dx, dy));

    float4 As = uStr.sample(uSamp0, vUV + float2(-dx, -dy));
    float4 Bs = uStr.sample(uSamp0, vUV + float2(0.0, -dy));
    float4 Cs = uStr.sample(uSamp0, vUV + float2(dx, -dy));
    float4 Ds = uStr.sample(uSamp0, vUV + float2(-dx, 0.0));
    float4 Es = uStr.sample(uSamp0, vUV);
    float4 Fs = uStr.sample(uSamp0, vUV + float2(dx, 0.0));
    float4 Gs = uStr.sample(uSamp0, vUV + float2(-dx, dy));
    float4 Hs = uStr.sample(uSamp0, vUV + float2(0.0, dy));
    float4 Is = uStr.sample(uSamp0, vUV + float2(dx, dy));

    float4 jSx = float4(As.z, Bs.w, Es.x, Ds.y), jDx = dom(As.yzw, Bs.zwx, Es.wxy, Ds.xyz);
    float4 jSy = float4(Bs.z, Cs.w, Fs.x, Es.y), jDy = dom(Bs.yzw, Cs.zwx, Fs.wxy, Es.xyz);
    float4 jSz = float4(Es.z, Fs.w, Is.x, Hs.y), jDz = dom(Es.yzw, Fs.zwx, Is.wxy, Hs.xyz);
    float4 jSw = float4(Ds.z, Es.w, Hs.x, Gs.y), jDw = dom(Ds.yzw, Es.zwx, Hs.wxy, Gs.xyz);

    float4 zero4 = float4(0.0);
    float4 jx = min(GE(jDx, zero4) * (LEQ(jDx.yzwx, zero4) * LEQ(jDx.wxyz, zero4) + GE(jDx + jDx.zwxy, jDx.yzwx + jDx.wxyz)), 1.0);
    float4 jy = min(GE(jDy, zero4) * (LEQ(jDy.yzwx, zero4) * LEQ(jDy.wxyz, zero4) + GE(jDy + jDy.zwxy, jDy.yzwx + jDy.wxyz)), 1.0);
    float4 jz = min(GE(jDz, zero4) * (LEQ(jDz.yzwx, zero4) * LEQ(jDz.wxyz, zero4) + GE(jDz + jDz.zwxy, jDz.yzwx + jDz.wxyz)), 1.0);
    float4 jw = min(GE(jDw, zero4) * (LEQ(jDw.yzwx, zero4) * LEQ(jDw.wxyz, zero4) + GE(jDw + jDw.zwxy, jDw.yzwx + jDw.wxyz)), 1.0);

    float4 res;
    res.x = min(jx.z + (1.0 - jx.y) * (1.0 - jx.w) * step(0.0, jSx.z) * (jx.x + GE(jSx.x + jSx.z, jSx.y + jSx.w)), 1.0);
    res.y = min(jy.w + (1.0 - jy.z) * (1.0 - jy.x) * step(0.0, jSy.w) * (jy.y + GE(jSy.y + jSy.w, jSy.x + jSy.z)), 1.0);
    res.z = min(jz.x + (1.0 - jz.w) * (1.0 - jz.y) * step(0.0, jSz.x) * (jz.z + GE(jSz.x + jSz.z, jSz.y + jSz.w)), 1.0);
    res.w = min(jw.y + (1.0 - jw.x) * (1.0 - jw.z) * step(0.0, jSw.y) * (jw.w + GE(jSw.y + jSw.w, jSw.x + jSw.z)), 1.0);

    res = min(res * (float4(jx.z, jy.w, jz.x, jw.y) + (1.0 - res.wxyz * res.yzwx)), 1.0);

    float4 clr;
    clr.x = clearF(float2(D.z, E.x), float2(D.w, E.y), float2(A.w, D.y));
    clr.y = clearF(float2(F.x, E.z), float2(E.w, E.y), float2(B.w, F.y));
    clr.z = clearF(float2(H.z, I.x), float2(E.w, H.y), float2(H.w, I.y));
    clr.w = clearF(float2(H.x, G.z), float2(D.w, H.y), float2(G.w, G.y));

    float4 h = float4(min(D.w, A.w), min(E.w, B.w), min(E.w, H.w), min(D.w, G.w));
    float4 v = float4(min(E.y, D.y), min(E.y, F.y), min(H.y, I.y), min(H.y, G.y));

    float4 orien = GE(h + float4(D.w, E.w, E.w, D.w), v + float4(E.y, E.y, H.y, H.y));
    float4 hori = LE(h, v) * clr;
    float4 vert = GE(h, v) * clr;

    return (res + 2 * hori + 4 * vert + 8 * orien) / 15;
}
)MSL";

static constexpr const char *kSfxPass3 = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams; // x = SFX_SCN
};

static bool4 loadCorn(float4 x) { return bool4(floor(fmod(x * 15 + 0.5, 2)) == 1.0); }
static bool4 loadHori(float4 x) { return bool4(floor(fmod(x * 7.5 + 0.25, 2)) == 1.0); }
static bool4 loadVert(float4 x) { return bool4(floor(fmod(x * 3.75 + 0.125, 2)) == 1.0); }
static bool4 loadOr(float4 x)   { return bool4(floor(fmod(x * 1.875 + 0.0625, 2)) == 1.0); }

fragment float4 sfxPass3PS(float2 vUV [[user(locn0)]],
    texture2d<float> uTex [[texture(0)]],
    sampler uSamp [[sampler(0)]],
    constant FragUniform &u [[buffer(0)]])
{
    float dx = u.uSourceSize.z;
    float dy = u.uSourceSize.w;

    float4 E = uTex.sample(uSamp, vUV);
    float4 D = uTex.sample(uSamp, vUV + float2(-dx, 0.0)), D0 = uTex.sample(uSamp, vUV + float2(-2 * dx, 0.0)), D1 = uTex.sample(uSamp, vUV + float2(-3 * dx, 0.0));
    float4 F = uTex.sample(uSamp, vUV + float2(dx, 0.0)), F0 = uTex.sample(uSamp, vUV + float2(2 * dx, 0.0)), F1 = uTex.sample(uSamp, vUV + float2(3 * dx, 0.0));
    float4 B = uTex.sample(uSamp, vUV + float2(0.0, -dy)), B0 = uTex.sample(uSamp, vUV + float2(0.0, -2 * dy)), B1 = uTex.sample(uSamp, vUV + float2(0.0, -3 * dy));
    float4 H = uTex.sample(uSamp, vUV + float2(0.0, dy)), H0 = uTex.sample(uSamp, vUV + float2(0.0, 2 * dy)), H1 = uTex.sample(uSamp, vUV + float2(0.0, 3 * dy));

    bool4 Ec = loadCorn(E), Eh = loadHori(E), Ev = loadVert(E), Eo = loadOr(E);
    bool4 Dc = loadCorn(D), Dh = loadHori(D), Do = loadOr(D), D0c = loadCorn(D0), D0h = loadHori(D0), D1h = loadHori(D1);
    bool4 Fc = loadCorn(F), Fh = loadHori(F), Fo = loadOr(F), F0c = loadCorn(F0), F0h = loadHori(F0), F1h = loadHori(F1);
    bool4 Bc = loadCorn(B), Bv = loadVert(B), Bo = loadOr(B), B0c = loadCorn(B0), B0v = loadVert(B0), B1v = loadVert(B1);
    bool4 Hc = loadCorn(H), Hv = loadVert(H), Ho = loadOr(H), H0c = loadCorn(H0), H0v = loadVert(H0), H1v = loadVert(H1);

    bool lvl1x = Ec.x && (Dc.z || Bc.z || u.uParams.x == 1);
    bool lvl1y = Ec.y && (Fc.w || Bc.w || u.uParams.x == 1);
    bool lvl1z = Ec.z && (Fc.x || Hc.x || u.uParams.x == 1);
    bool lvl1w = Ec.w && (Dc.y || Hc.y || u.uParams.x == 1);

    bool2 lvl2x = bool2((Ec.x && Eh.y) && Dc.z, (Ec.y && Eh.x) && Fc.w);
    bool2 lvl2y = bool2((Ec.y && Ev.z) && Bc.w, (Ec.z && Ev.y) && Hc.x);
    bool2 lvl2z = bool2((Ec.w && Eh.z) && Dc.y, (Ec.z && Eh.w) && Fc.x);
    bool2 lvl2w = bool2((Ec.x && Ev.w) && Bc.z, (Ec.w && Ev.x) && Hc.y);

    bool2 lvl3x = bool2(lvl2x.y && (Dh.y && Dh.x) && Fh.z, lvl2w.y && (Bv.w && Bv.x) && Hv.z);
    bool2 lvl3y = bool2(lvl2x.x && (Fh.x && Fh.y) && Dh.w, lvl2y.y && (Bv.z && Bv.y) && Hv.w);
    bool2 lvl3z = bool2(lvl2z.x && (Fh.w && Fh.z) && Dh.x, lvl2y.x && (Hv.y && Hv.z) && Bv.x);
    bool2 lvl3w = bool2(lvl2z.y && (Dh.z && Dh.w) && Fh.y, lvl2w.x && (Hv.x && Hv.w) && Bv.y);

    bool2 lvl4x = bool2((Dc.x && Dh.y && Eh.x && Eh.y && Fh.x && Fh.y) && (D0c.z && D0h.w), (Bc.x && Bv.w && Ev.x && Ev.w && Hv.x && Hv.w) && (B0c.z && B0v.y));
    bool2 lvl4y = bool2((Fc.y && Fh.x && Eh.y && Eh.x && Dh.y && Dh.x) && (F0c.w && F0h.z), (Bc.y && Bv.z && Ev.y && Ev.z && Hv.y && Hv.z) && (B0c.w && B0v.x));
    bool2 lvl4z = bool2((Fc.z && Fh.w && Eh.z && Eh.w && Dh.z && Dh.w) && (F0c.x && F0h.y), (Hc.z && Hv.y && Ev.z && Ev.y && Bv.z && Bv.y) && (H0c.x && H0v.w));
    bool2 lvl4w = bool2((Dc.w && Dh.z && Eh.w && Eh.z && Fh.w && Fh.z) && (D0c.y && D0h.x), (Hc.w && Hv.x && Ev.w && Ev.x && Bv.w && Bv.x) && (H0c.y && H0v.z));

    bool2 lvl5x = bool2(lvl4x.x && (F0h.x && F0h.y) && (D1h.z && D1h.w), lvl4y.x && (D0h.y && D0h.x) && (F1h.w && F1h.z));
    bool2 lvl5y = bool2(lvl4y.y && (H0v.y && H0v.z) && (B1v.w && B1v.x), lvl4z.y && (B0v.z && B0v.y) && (H1v.x && H1v.w));
    bool2 lvl5z = bool2(lvl4w.x && (F0h.w && F0h.z) && (D1h.y && D1h.x), lvl4z.x && (D0h.z && D0h.w) && (F1h.x && F1h.y));
    bool2 lvl5w = bool2(lvl4x.y && (H0v.x && H0v.w) && (B1v.z && B1v.y), lvl4w.y && (B0v.w && B0v.x) && (H1v.y && H1v.z));

    bool2 lvl6x = bool2(lvl5x.y && (D1h.y && D1h.x), lvl5w.y && (B1v.w && B1v.x));
    bool2 lvl6y = bool2(lvl5x.x && (F1h.x && F1h.y), lvl5y.y && (B1v.z && B1v.y));
    bool2 lvl6z = bool2(lvl5z.x && (F1h.w && F1h.z), lvl5y.x && (H1v.y && H1v.z));
    bool2 lvl6w = bool2(lvl5z.y && (D1h.z && D1h.w), lvl5w.x && (H1v.x && H1v.w));

    float4 crn;
    crn.x = (lvl1x && Eo.x || lvl3x.x && Eo.y || lvl4x.x && Do.x || lvl6x.x && Fo.y) ? 5 : (lvl1x || lvl3x.y && !Eo.w || lvl4x.y && !Bo.x || lvl6x.y && !Ho.w) ? 1 : lvl3x.x ? 3 : lvl3x.y ? 7 : lvl4x.x ? 2 : lvl4x.y ? 6 : lvl6x.x ? 4 : lvl6x.y ? 8 : 0;
    crn.y = (lvl1y && Eo.y || lvl3y.x && Eo.x || lvl4y.x && Fo.y || lvl6y.x && Do.x) ? 5 : (lvl1y || lvl3y.y && !Eo.z || lvl4y.y && !Bo.y || lvl6y.y && !Ho.z) ? 3 : lvl3y.x ? 1 : lvl3y.y ? 7 : lvl4y.x ? 4 : lvl4y.y ? 6 : lvl6y.x ? 2 : lvl6y.y ? 8 : 0;
    crn.z = (lvl1z && Eo.z || lvl3z.x && Eo.w || lvl4z.x && Fo.z || lvl6z.x && Do.w) ? 7 : (lvl1z || lvl3z.y && !Eo.y || lvl4z.y && !Ho.z || lvl6z.y && !Bo.y) ? 3 : lvl3z.x ? 1 : lvl3z.y ? 5 : lvl4z.x ? 4 : lvl4z.y ? 8 : lvl6z.x ? 2 : lvl6z.y ? 6 : 0;
    crn.w = (lvl1w && Eo.w || lvl3w.x && Eo.z || lvl4w.x && Do.w || lvl6w.x && Fo.z) ? 7 : (lvl1w || lvl3w.y && !Eo.x || lvl4w.y && !Ho.w || lvl6w.y && !Bo.x) ? 1 : lvl3w.x ? 3 : lvl3w.y ? 5 : lvl4w.x ? 2 : lvl4w.y ? 8 : lvl6w.x ? 4 : lvl6w.y ? 6 : 0;

    float4 mid;
    mid.x = (lvl2x.x && Eo.x || lvl2x.y && Eo.y || lvl5x.x && Do.x || lvl5x.y && Fo.y) ? 5 : lvl2x.x ? 1 : lvl2x.y ? 3 : lvl5x.x ? 2 : lvl5x.y ? 4 : (Ec.x && Dc.z && Ec.y && Fc.w) ? (Eo.x ? Eo.y ? 5 : 3 : 1) : 0;
    mid.y = (lvl2y.x && !Eo.y || lvl2y.y && !Eo.z || lvl5y.x && !Bo.y || lvl5y.y && !Ho.z) ? 3 : lvl2y.x ? 5 : lvl2y.y ? 7 : lvl5y.x ? 6 : lvl5y.y ? 8 : (Ec.y && Bc.w && Ec.z && Hc.x) ? (!Eo.y ? !Eo.z ? 3 : 7 : 5) : 0;
    mid.z = (lvl2z.x && Eo.w || lvl2z.y && Eo.z || lvl5z.x && Do.w || lvl5z.y && Fo.z) ? 7 : lvl2z.x ? 1 : lvl2z.y ? 3 : lvl5z.x ? 2 : lvl5z.y ? 4 : (Ec.z && Fc.x && Ec.w && Dc.y) ? (Eo.z ? Eo.w ? 7 : 1 : 3) : 0;
    mid.w = (lvl2w.x && !Eo.x || lvl2w.y && !Eo.w || lvl5w.x && !Bo.x || lvl5w.y && !Ho.w) ? 1 : lvl2w.x ? 5 : lvl2w.y ? 7 : lvl5w.x ? 6 : lvl5w.y ? 8 : (Ec.w && Hc.y && Ec.x && Bc.z) ? (!Eo.w ? !Eo.x ? 1 : 5 : 7) : 0;

    return (crn + 9 * mid) / 80;
}
)MSL";

static constexpr const char *kSfxPass4 = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams; // x = S
};

static float4 loadCrn(float4 x) { return floor(fmod(x * 80 + 0.5, 9)); }
static float4 loadMid(float4 x) { return floor(fmod(x * 8.888888 + 0.055555, 9)); }

fragment float4 sfxPass4PS(float2 vUV [[user(locn0)]],
    texture2d<float> uTex [[texture(0)]],
    texture2d<float> uRef [[texture(1)]],
    sampler uSamp0 [[sampler(0)]],
    sampler uSamp1 [[sampler(1)]],
    constant FragUniform &u [[buffer(0)]])
{
    float4 E = uTex.sample(uSamp0, vUV);

    float4 crn = loadCrn(E);
    float4 mid = loadMid(E);

    float2 fp = floor(u.uParams.x * fract(vUV * u.uSourceSize.xy));
    int2 g = min(int2(fp), int2(2));
    float sp = g.y == 0 ? (g.x == 0 ? crn.x : g.x == 1 ? mid.x : crn.y) : g.y == 1 ? (g.x == 0 ? mid.w : g.x == 1 ? 0.0 : mid.y) : (g.x == 0 ? crn.w : g.x == 1 ? mid.z : crn.z);

    int2 res = sp == 0.0 ? int2(0, 0) : sp == 1.0 ? int2(-1, 0) : sp == 2.0 ? int2(-2, 0) : sp == 3.0 ? int2(1, 0) : sp == 4.0 ? int2(2, 0) : sp == 5.0 ? int2(0, -1) : sp == 6.0 ? int2(0, -2) : sp == 7.0 ? int2(0, 1) : int2(0, 2);

    return uRef.sample(uSamp1, vUV + float2(res) * u.uSourceSize.zw);
}
)MSL";

static constexpr const char *kBlit = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct FragUniform {
    float4 uSourceSize;
    float4 uParams;
};

fragment float4 blitPS(float2 vUV [[user(locn0)]],
    texture2d<float> uTex [[texture(0)]],
    sampler uSamp [[sampler(0)]],
    constant FragUniform &u [[buffer(0)]])
{
    return uTex.sample(uSamp, vUV);
}
)MSL";

} // namespace devilution::gpu::msl
