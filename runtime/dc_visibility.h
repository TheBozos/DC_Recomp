#pragma once
#include "bozo/runtime.h"
#include <functional>

namespace dc::native
{
    // Pixel coordinates in the logical 640x224 frame, returning native 24-bit depth.
    using DepthSampler = std::function<uint32_t(int32_t, int32_t)>;
    void resolveVisibility(Runtime &, R5900Context &, const DepthSampler &);
    void queryVisibility(Runtime &, R5900Context &);
}
