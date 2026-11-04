// TEST-01 corpus: single pass, one #pragma parameter consumed from
// the fragment UBO (SLANG_SPEC 4.6: parameters live in blocks).
#pragma name passthrough

#pragma parameter intensity "Intensity" 1.0 0.0 2.0 0.05

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
layout(std140, set = 0, binding = 0) uniform UBO
{
	mat4 MVP;
} global;
void main() {
	gl_Position = global.MVP * Position;
	vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(std140, set = 0, binding = 1) uniform UBO
{
	vec4 SourceSize;
	float intensity;
} params;

uniform sampler2D Source;

void main()
{
	FragColor = texture(Source, vTexCoord) * params.intensity;
}
