#version 450

// DevilutionX GPU scaler - blit pass.
// Draws the scaled framebuffer to the swapchain with letterboxing handled by
// the viewport. Linear filtering; when the blit is 1:1 the result is exact.

layout(set = 2, binding = 0) uniform sampler2D uTex;
layout(set = 3, binding = 0) uniform FragUniform {
	vec4 uSourceSize;
	vec4 uParams;
};

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 FragColor;

void main()
{
	FragColor = texture(uTex, vUV);
}
