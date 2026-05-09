#pragma once

#include <cstdint>

#include <function_ref.hpp>

#include "engine/point.hpp"

namespace devilution {

void DoVision(Point position, uint8_t radius,
    tl::function_ref<void(Point)> markVisibleFn,
    tl::function_ref<void(Point)> markTransparentFn,
    tl::function_ref<bool(Point)> passesLightFn,
    tl::function_ref<bool(Point)> inBoundsFn);

} // namespace devilution
