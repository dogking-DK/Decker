#pragma once

#include <cstdint>

#include "Macros.h"

namespace dk {

static uint32_t align_offset(uint32_t offset, uint32_t alignment)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

} // dk