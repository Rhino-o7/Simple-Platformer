#pragma once

#include <cstdint>

namespace vpg::ecs {
    using Entity = int32_t;
    constexpr Entity MaxEntities = 8192;
    constexpr Entity NullEntity = -1;
}