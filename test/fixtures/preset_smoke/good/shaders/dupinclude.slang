// ROB-21 corpus: the fragment stage re-includes the guarded header, so the
// fragment TU carries the "dup" sampler declaration twice. First binding
// wins; the duplicate must be dropped with a warning, not re-bound.
#pragma name dupinclude

#include "guarded_texture_header.slang"

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
} params;

#include "guarded_texture_header.slang"

void main()
{
	FragColor = texture(dup, vTexCoord);
}
