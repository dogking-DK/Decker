#include <cstdint>

#include "Macros.h"

DECKER_START

static uint32_t align_offset(uint32_t offset, uint32_t alignment)
{
    return (offset + alignment - 1) & ~(alignment - 1);
}

DECKER_END