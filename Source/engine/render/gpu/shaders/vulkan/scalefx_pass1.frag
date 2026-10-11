#version 450

// DevilutionX GPU scaler - ScaleFX pass 1 (corner strength).
// Port of the libretro slang scalefx shaders:
//   Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de
//   License: MIT
// Pass 1 calculates the strength of interpolation candidates.
// uParams = (SFX_CLR, SFX_SAA, -, -), defaults (0.5, 1.0).

layout(set = 2, binding = 0) uniform sampler2D uTex; // pass 0 output (metrics)
layout(set = 3, binding = 0) uniform FragUniform {
	vec4 uSourceSize; // (w, h, 1/w, 1/h)
	vec4 uParams;     // x = SFX_CLR, y = SFX_SAA
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

// corner strength
float str(float d, vec2 a, vec2 b)
{
	float diff = a.x - a.y;
	float wght1 = max(uParams.x - d, 0) / uParams.x;
	float wght2 = clamp((1 - d) + (min(a.x, b.x) + a.x > min(a.y, b.y) + a.y ? diff : -diff), 0., 1.);
	return (uParams.y == 1. || 2. * d < a.x + a.y) ? (wght1 * wght2) * (a.x * a.y) : 0.;
}

void main()
{
	/*      grid            metric          pattern

	        A B             x y z           x y
	      D E F               o w           w z
	      G H I
	*/
	float dx = uSourceSize.z;
	float dy = uSourceSize.w;

	// metric data
	vec4 A = texture(uTex, vUV + vec2(-dx, -dy));
	vec4 B = texture(uTex, vUV + vec2(0.0, -dy));
	vec4 D = texture(uTex, vUV + vec2(-dx, 0.0));
	vec4 E = texture(uTex, vUV);
	vec4 F = texture(uTex, vUV + vec2(dx, 0.0));
	vec4 G = texture(uTex, vUV + vec2(-dx, dy));
	vec4 H = texture(uTex, vUV + vec2(0.0, dy));
	vec4 I = texture(uTex, vUV + vec2(dx, dy));

	// corner strength
	vec4 res;
	res.x = str(D.z, vec2(D.w, E.y), vec2(A.w, D.y));
	res.y = str(F.x, vec2(E.w, E.y), vec2(B.w, F.y));
	res.z = str(H.z, vec2(E.w, H.y), vec2(H.w, I.y));
	res.w = str(H.x, vec2(D.w, H.y), vec2(G.w, G.y));

	FragColor = res;
}
