// TEST-01 corpus: taps the history ring at indexes 1 and 2 with
// histories = 4 declared by the preset.
#pragma name history_tap

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
uniform sampler2D Original;
uniform sampler2D OriginalHistory1;
uniform sampler2D OriginalHistory2;

void main()
{
	vec3 c0 = texture(Source, vTexCoord).rgb;
	vec3 c1 = texture(Original, vTexCoord).rgb;
	vec3 c2 = texture(OriginalHistory1, vTexCoord).rgb;
	vec3 c3 = texture(OriginalHistory2, vTexCoord).rgb;
	FragColor = vec4((c0 + c1 + c2 + c3) / 4.0, 1.0);
}
