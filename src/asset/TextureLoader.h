#pragma once
#include "../asset/RawTypes.hpp"
#include <memory>
#include <filesystem>
#include <vector>

namespace dk {
struct TextureData
{
    uint32_t             width{}, height{}, depth{}, channels{};
    std::vector<uint8_t> pixels;          // Always RGBA8
};

}
