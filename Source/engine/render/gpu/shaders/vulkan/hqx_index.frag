#version 450

// DevilutionX GPU scaler - hqx edge index pass (shared by HQ2x/HQ3x/HQ4x).
// Port of the libretro slang hqx pass1 shader:
//   Copyright (C) 2003 Maxim Stepin
//   Copyright (C) 2010 Cameron Zemek
//   Copyright (C) 2014 Jules Blok
//   License: GNU Lesser General Public License, version 2.1 or later.
// Outputs: R = 8-bit edge pattern, G = 4-bit "cross" pattern.

layout(set = 2, binding = 0) uniform sampler2D uTex;
layout(set = 3, binding = 0) uniform FragUniform {
	vec4 uSourceSize; // (w, h, 1/w, 1/h)
	vec4 uParams;
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

const vec3 yuvThreshold = vec3(48.0 / 255.0, 7.0 / 255.0, 6.0 / 255.0);
const mat3 yuv = mat3(0.299, -0.169, 0.5, 0.587, -0.331, -0.419, 0.114, 0.5, -0.081);
const vec3 yuvOffset = vec3(0.0, 0.5, 0.5);

bool diff(vec3 a, vec3 b)
{
	bvec3 res = greaterThan(abs((a + yuvOffset) - (b + yuvOffset)), yuvThreshold);
	return res.x || res.y || res.z;
}

void main()
{
	float dx = uSourceSize.z;
	float dy = uSourceSize.w;

	vec3 w1 = yuv * texture(uTex, vUV + vec2(-dx, -dy)).rgb;
	vec3 w2 = yuv * texture(uTex, vUV + vec2(0.0, -dy)).rgb;
	vec3 w3 = yuv * texture(uTex, vUV + vec2(dx, -dy)).rgb;
	vec3 w4 = yuv * texture(uTex, vUV + vec2(-dx, 0.0)).rgb;
	vec3 w5 = yuv * texture(uTex, vUV).rgb;
	vec3 w6 = yuv * texture(uTex, vUV + vec2(dx, 0.0)).rgb;
	vec3 w7 = yuv * texture(uTex, vUV + vec2(-dx, dy)).rgb;
	vec3 w8 = yuv * texture(uTex, vUV + vec2(0.0, dy)).rgb;
	vec3 w9 = yuv * texture(uTex, vUV + vec2(dx, dy)).rgb;

	vec3 pattern_1 = vec3(diff(w5, w1), diff(w5, w2), diff(w5, w3));
	vec3 pattern_2 = vec3(diff(w5, w4), false, diff(w5, w6));
	vec3 pattern_3 = vec3(diff(w5, w7), diff(w5, w8), diff(w5, w9));
	vec4 cross = vec4(diff(w4, w2), diff(w2, w6), diff(w8, w4), diff(w6, w8));

	float indexX = dot(pattern_1, vec3(1.0, 2.0, 4.0))
	    + dot(pattern_2, vec3(8.0, 0.0, 16.0))
	    + dot(pattern_3, vec3(32.0, 64.0, 128.0));
	float indexY = dot(cross, vec4(1.0, 2.0, 4.0, 8.0));

	FragColor = vec4(indexX / 255.0, indexY / 15.0, 0.0, 1.0);
}
