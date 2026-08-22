/**
 * @file renderer.cpp
 *
 * Runtime renderer dispatch.
 */
#include "engine/render/renderer.h"

namespace devilution {

namespace {

/** Owns the instance that CurrentRenderer points at. */
std::unique_ptr<Renderer> OwnedRenderer;

} // namespace

Renderer *CurrentRenderer = nullptr;

void SetRenderer(std::unique_ptr<Renderer> renderer)
{
	OwnedRenderer = std::move(renderer);
	CurrentRenderer = OwnedRenderer.get();
}

} // namespace devilution
