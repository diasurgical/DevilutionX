#version 450

// DevilutionX GPU scaler - ScaleFX pass 4 (final render).
// Port of the libretro slang scalefx shaders:
//   Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de
//   License: MIT
// Pass 4 renders the upscaled result. uParams.x = integer scale S.
// At scales other than 3 the 3x3 subpixel grid collapses towards the
// top-left, matching the behaviour of the original ScaleFX.

layout(set = 2, binding = 0) uniform sampler2D uTex; // pass 3 output (subpixel map)
layout(set = 2, binding = 1) uniform sampler2D uRef; // original frame
layout(set = 3, binding = 0) uniform FragUniform {
	vec4 uSourceSize; // (w, h, 1/w, 1/h) of the game frame
	vec4 uParams;     // x = S
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

// extract corners
vec4 loadCrn(vec4 x)
{
	return floor(mod(x * 80 + 0.5, 9));
}

// extract mids
vec4 loadMid(vec4 x)
{
	return floor(mod(x * 8.888888 + 0.055555, 9));
}

void main()
{
	/*      grid            corners         mids

	          B             x   y             x
	        D E F                           w   y
	          H             w   z             z
	*/

	// read data
	vec4 E = texture(uTex, vUV);

	// extract data
	vec4 crn = loadCrn(E);
	vec4 mid = loadMid(E);

	// determine subpixel
	vec2 fp = floor(uParams.x * fract(vUV * uSourceSize.xy));
	ivec2 g = min(ivec2(fp), ivec2(2));
	float sp = g.y == 0 ? (g.x == 0 ? crn.x : g.x == 1 ? mid.x : crn.y) : g.y == 1 ? (g.x == 0 ? mid.w : g.x == 1 ? 0. : mid.y) : (g.x == 0 ? crn.w : g.x == 1 ? mid.z : crn.z);

	// output coordinate - 0 = E, 1 = D, 2 = D0, 3 = F, 4 = F0, 5 = B, 6 = B0, 7 = H, 8 = H0
	ivec2 res = sp == 0. ? ivec2(0, 0) : sp == 1. ? ivec2(-1, 0) : sp == 2. ? ivec2(-2, 0) : sp == 3. ? ivec2(1, 0) : sp == 4. ? ivec2(2, 0) : sp == 5. ? ivec2(0, -1) : sp == 6. ? ivec2(0, -2) : sp == 7. ? ivec2(0, 1) : ivec2(0, 2);

	// output
	FragColor = texture(uRef, vUV + vec2(res) * uSourceSize.zw);
}
