/**
 * @file renderer.cpp
 *
 * Runtime renderer dispatch.
 */
#include "engine/render/renderer.h"

#include <cassert>

namespace devilution {

namespace {

std::unique_ptr<Renderer> CurrentRenderer;

} // namespace

Renderer &GetRenderer()
{
	assert(CurrentRenderer != nullptr);
	return *CurrentRenderer;
}

void SetRenderer(std::unique_ptr<Renderer> renderer)
{
	CurrentRenderer = std::move(renderer);
}

} // namespace devilution
