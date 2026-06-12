/**
 * @file renderer_backend.h
 *
 * Renderer backend selection.
 */
#pragma once

namespace devilution {

enum class RendererBackend {
	Auto,
	Software,
#ifdef DEVILUTIONX_GL1_RENDERER
	OpenGL1,
#endif
};

#ifdef DEVILUTIONX_GL1_RENDERER
bool IsGl1RendererActive();
void SetGl1RendererActive(bool active);
#endif

} // namespace devilution