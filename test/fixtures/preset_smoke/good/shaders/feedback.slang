// TEST-01 corpus: reads the previous pass's output (PassOutput0,
// causal from pass 1 on) - explicit feedback composition.
#pragma name feedback_mix

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
uniform sampler2D Source;
uniform sampler2D PassOutput0;

void main()
{
	vec3 now = texture(Source, vTexCoord).rgb;
	vec3 prev = texture(PassOutput0, vTexCoord).rgb;
	FragColor = vec4(mix(now, prev, 0.5), 1.0);
}
