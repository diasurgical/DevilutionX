#version 450

// DevilutionX GPU scaler - hqx blend pass (shared by HQ2x/HQ3x/HQ4x).
// Port of the libretro slang hqx pass2 shader:
//   Copyright (C) 2003 Maxim Stepin
//   Copyright (C) 2010 Cameron Zemek
//   Copyright (C) 2014 Jules Blok
//   License: GNU Lesser General Public License, version 2.1 or later.
// uParams.x = integer scale factor S (2, 3 or 4). The LUT texture is the
// 256 x (16 * S * S) weight table for the corresponding scale.

layout(set = 2, binding = 0) uniform sampler2D uIndex; // hqx index pass output
layout(set = 2, binding = 1) uniform sampler2D uLUT;   // weight lookup table
layout(set = 2, binding = 2) uniform sampler2D uRef;   // original frame
layout(set = 3, binding = 0) uniform FragUniform {
	vec4 uSourceSize; // (w, h, 1/w, 1/h) of the game frame
	vec4 uParams;     // x = S
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

void main()
{
	float S = uParams.x;
	vec2 fp = fract(vUV * uSourceSize.xy);
	vec2 quad = sign(-0.5 + fp);

	float dx = uSourceSize.z;
	float dy = uSourceSize.w;

	vec3 pixels[4];
	pixels[0] = texture(uRef, vUV).rgb;
	pixels[1] = texture(uRef, vUV + vec2(dx, dy) * quad).rgb;
	pixels[2] = texture(uRef, vUV + vec2(dx, 0.0) * quad).rgb;
	pixels[3] = texture(uRef, vUV + vec2(0.0, dy) * quad).rgb;

	vec2 index = texture(uIndex, vUV).xy * vec2(255.0, 15.0 * (S * S));
	index.y += dot(floor(fp * S), vec2(1.0, S));

	vec2 step = 1.0 / vec2(256.0, 16.0 * (S * S));
	vec4 weights = texture(uLUT, index * step + step * 0.5);
	vec4 w = weights / dot(weights, vec4(1.0));

	FragColor = vec4(w.x * pixels[0] + w.y * pixels[1] + w.z * pixels[2] + w.w * pixels[3], 1.0);
}
