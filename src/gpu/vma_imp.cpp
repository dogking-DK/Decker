#include <fmt/core.h>
#include <fmt/color.h>



#define VMA_IMPLEMENTATION
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1
//#define ENABLE_VMA_DEBUG
#ifdef ENABLE_VMA_DEBUG
#define VMA_DEBUG_LOG(...)                                                   \
    do {                                                                     \
        fmt::print(fmt::fg(fmt::color::light_green), "[VMA] ");              \
        fmt::print(fmt::fg(fmt::color::light_green), __VA_ARGS__);           \
        fmt::print("\n");                                                    \
    } while (0)
#endif




#include <vk_mem_alloc.h>