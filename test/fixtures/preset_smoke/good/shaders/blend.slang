// TEST-01 corpus: blends the chain input with the untouched game frame.
#pragma name blend

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

void main()
{
	vec3 c = mix(texture(Source, vTexCoord).rgb, texture(Original, vTexCoord).rgb, 0.5);
	FragColor = vec4(c, 1.0);
}
