#version 450

// DevilutionX GPU scaler - fullscreen triangle vertex shader.
// No vertex buffers; the quad is generated from the vertex index.
// UV convention: vUV (0,0) at the top-left of the target, matching the
// row-major upload order of the game framebuffer.

layout(location = 0) out vec2 vUV;

void main()
{
	vec2 pos = vec2(float((gl_VertexIndex & 1) << 2) - 1.0, float(gl_VertexIndex & 2) - 1.0);
	vUV = (pos + 1.0) * 0.5;
	gl_Position = vec4(pos, 0.0, 1.0);
}
