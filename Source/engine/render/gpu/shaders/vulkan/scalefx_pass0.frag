#version 450

// DevilutionX GPU scaler - ScaleFX pass 0 (metrics).
// Port of the libretro slang scalefx shaders:
//   Copyright (c) 2016 Sp00kyFox - ScaleFX@web.de
//   License: MIT
// Pass 0 prepares metric data for the next pass.

layout(set = 2, binding = 0) uniform sampler2D uTex;
layout(set = 3, binding = 0) uniform FragUniform {
	vec4 uSourceSize; // (w, h, 1/w, 1/h)
	vec4 uParams;
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

// Reference: http://www.compuphase.com/cmetric.htm
float dist(vec3 A, vec3 B)
{
	float r = 0.5 * (A.r + B.r);
	vec3 d = A - B;
	vec3 c = vec3(2 + r, 4, 3 - r);

	return sqrt(dot(c * d, d)) / 3;
}

void main()
{
	/*      grid            metric

	        A B C           x y z
	          E F             o w
	*/
	float dx = uSourceSize.z;
	float dy = uSourceSize.w;

	// read texels
	vec3 A = texture(uTex, vUV + vec2(-dx, -dy)).rgb;
	vec3 B = texture(uTex, vUV + vec2(0.0, -dy)).rgb;
	vec3 C = texture(uTex, vUV + vec2(dx, -dy)).rgb;
	vec3 E = texture(uTex, vUV).rgb;
	vec3 F = texture(uTex, vUV + vec2(dx, 0.0)).rgb;

	// output
	FragColor = vec4(dist(E, A), dist(E, B), dist(E, C), dist(E, F));
}
