// TEST-01 corpus: samples the chain input plus the pass aliased
// "blur" (multipass.slangp pass 1) - alias-feedback composition.
#pragma name alias_final

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
uniform sampler2D blur;

void main()
{
	vec3 c = texture(Source, vTexCoord).rgb + texture(blur, vTexCoord).rgb * 0.5;
	FragColor = vec4(c, 1.0);
}
