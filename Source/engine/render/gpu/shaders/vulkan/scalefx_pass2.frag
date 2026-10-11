#version 450

// DevilutionX GPU scaler - ScaleFX pass 2 (edge flags).
// Port of the libretro slang scalefx shaders:
//   Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de
//   License: MIT
// Pass 2 resolves corner dominance and packs edge flags.

layout(set = 2, binding = 0) uniform sampler2D uStr;    // pass 1 output (strengths)
layout(set = 2, binding = 1) uniform sampler2D uMetric; // pass 0 output (metrics)
layout(set = 3, binding = 0) uniform FragUniform {
	vec4 uSourceSize; // (w, h, 1/w, 1/h)
	vec4 uParams;
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

#define LE(x, y) (1 - step(y, x))
#define GE(x, y) (1 - step(x, y))
#define LEQ(x, y) step(x, y)
#define GEQ(x, y) step(y, x)
#define NOT(x) (1 - (x))

// corner dominance at junctions
vec4 dom(vec3 x, vec3 y, vec3 z, vec3 w)
{
	return 2 * vec4(x.y, y.y, z.y, w.y) - (vec4(x.x, y.x, z.x, w.x) + vec4(x.z, y.z, z.z, w.z));
}

// necessary but not sufficient junction condition for orthogonal edges
float clear(vec2 crn, vec2 a, vec2 b)
{
	return (crn.x >= max(min(a.x, a.y), min(b.x, b.y))) && (crn.y >= max(min(a.x, b.y), min(b.x, a.y))) ? 1. : 0.;
}

void main()
{
	/*      grid            metric          pattern

	        A B C           x y z           x y
	      D E F               o w           w z
	      G H I
	*/
	float dx = uSourceSize.z;
	float dy = uSourceSize.w;

	// metric data
	vec4 A = texture(uMetric, vUV + vec2(-dx, -dy));
	vec4 B = texture(uMetric, vUV + vec2(0.0, -dy));
	vec4 D = texture(uMetric, vUV + vec2(-dx, 0.0));
	vec4 E = texture(uMetric, vUV);
	vec4 F = texture(uMetric, vUV + vec2(dx, 0.0));
	vec4 G = texture(uMetric, vUV + vec2(-dx, dy));
	vec4 H = texture(uMetric, vUV + vec2(0.0, dy));
	vec4 I = texture(uMetric, vUV + vec2(dx, dy));

	// strength data
	vec4 As = texture(uStr, vUV + vec2(-dx, -dy));
	vec4 Bs = texture(uStr, vUV + vec2(0.0, -dy));
	vec4 Cs = texture(uStr, vUV + vec2(dx, -dy));
	vec4 Ds = texture(uStr, vUV + vec2(-dx, 0.0));
	vec4 Es = texture(uStr, vUV);
	vec4 Fs = texture(uStr, vUV + vec2(dx, 0.0));
	vec4 Gs = texture(uStr, vUV + vec2(-dx, dy));
	vec4 Hs = texture(uStr, vUV + vec2(0.0, dy));
	vec4 Is = texture(uStr, vUV + vec2(dx, dy));

	// strength & dominance junctions
	vec4 jSx = vec4(As.z, Bs.w, Es.x, Ds.y), jDx = dom(As.yzw, Bs.zwx, Es.wxy, Ds.xyz);
	vec4 jSy = vec4(Bs.z, Cs.w, Fs.x, Es.y), jDy = dom(Bs.yzw, Cs.zwx, Fs.wxy, Es.xyz);
	vec4 jSz = vec4(Es.z, Fs.w, Is.x, Hs.y), jDz = dom(Es.yzw, Fs.zwx, Is.wxy, Hs.xyz);
	vec4 jSw = vec4(Ds.z, Es.w, Hs.x, Gs.y), jDw = dom(Ds.yzw, Es.zwx, Hs.wxy, Gs.xyz);

	// majority vote for ambiguous dominance junctions
	vec4 zero4 = vec4(0);
	vec4 jx = min(GE(jDx, zero4) * (LEQ(jDx.yzwx, zero4) * LEQ(jDx.wxyz, zero4) + GE(jDx + jDx.zwxy, jDx.yzwx + jDx.wxyz)), 1);
	vec4 jy = min(GE(jDy, zero4) * (LEQ(jDy.yzwx, zero4) * LEQ(jDy.wxyz, zero4) + GE(jDy + jDy.zwxy, jDy.yzwx + jDy.wxyz)), 1);
	vec4 jz = min(GE(jDz, zero4) * (LEQ(jDz.yzwx, zero4) * LEQ(jDz.wxyz, zero4) + GE(jDz + jDz.zwxy, jDz.yzwx + jDz.wxyz)), 1);
	vec4 jw = min(GE(jDw, zero4) * (LEQ(jDw.yzwx, zero4) * LEQ(jDw.wxyz, zero4) + GE(jDw + jDw.zwxy, jDw.yzwx + jDw.wxyz)), 1);

	// inject strength without creating new contradictions
	vec4 res;
	res.x = min(jx.z + NOT(jx.y) * NOT(jx.w) * GE(jSx.z, 0) * (jx.x + GE(jSx.x + jSx.z, jSx.y + jSx.w)), 1);
	res.y = min(jy.w + NOT(jy.z) * NOT(jy.x) * GE(jSy.w, 0) * (jy.y + GE(jSy.y + jSy.w, jSy.x + jSy.z)), 1);
	res.z = min(jz.x + NOT(jz.w) * NOT(jz.y) * GE(jSz.x, 0) * (jz.z + GE(jSz.x + jSz.z, jSz.y + jSz.w)), 1);
	res.w = min(jw.y + NOT(jw.x) * NOT(jw.z) * GE(jSw.y, 0) * (jw.w + GE(jSw.y + jSw.w, jSw.x + jSw.z)), 1);

	// single pixel & end of line detection
	res = min(res * (vec4(jx.z, jy.w, jz.x, jw.y) + NOT(res.wxyz * res.yzwx)), 1);

	// output

	vec4 clr;
	clr.x = clear(vec2(D.z, E.x), vec2(D.w, E.y), vec2(A.w, D.y));
	clr.y = clear(vec2(F.x, E.z), vec2(E.w, E.y), vec2(B.w, F.y));
	clr.z = clear(vec2(H.z, I.x), vec2(E.w, H.y), vec2(H.w, I.y));
	clr.w = clear(vec2(H.x, G.z), vec2(D.w, H.y), vec2(G.w, G.y));

	vec4 h = vec4(min(D.w, A.w), min(E.w, B.w), min(E.w, H.w), min(D.w, G.w));
	vec4 v = vec4(min(E.y, D.y), min(E.y, F.y), min(H.y, I.y), min(H.y, G.y));

	vec4 orien = GE(h + vec4(D.w, E.w, E.w, D.w), v + vec4(E.y, E.y, H.y, H.y)); // orientation
	vec4 hori = LE(h, v) * clr;                                                 // horizontal edges
	vec4 vert = GE(h, v) * clr;                                                 // vertical edges

	FragColor = (res + 2 * hori + 4 * vert + 8 * orien) / 15;
}
