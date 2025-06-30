#pragma once
#include <vector>
#include <cstdint>

#include "UUID.hpp"

namespace dk {
enum VertexAttribute : uint16_t
{
    Position  = 0,
    Normal    = 1,
    Tangent   = 2,
    Texcoord0 = 16,
    Color0    = 32,
    Joints0   = 48,
    Weights0  = 64,
};

constexpr VertexAttribute texCoordN(const int n) { return static_cast<VertexAttribute>(16 + n); }
constexpr VertexAttribute colorN(const int n) { return static_cast<VertexAttribute>(32 + n); }
constexpr VertexAttribute jointsN(const int n) { return static_cast<VertexAttribute>(48 + n); }
constexpr VertexAttribute weightsN(const int n) { return static_cast<VertexAttribute>(64 + n); }

struct RawMeshHeader
{
    uint32_t vertex_count;
    uint32_t index_count;
    uint16_t attr_count;
    uint16_t reserved = 0;
};

struct AttrDesc
{
    uint16_t        semantic;      // VertexSemantic
    uint8_t         elem_count;     // 1–4
    uint8_t         comp_type;      // 0=float32, 1=uint16, 2=uint8_norm …
    uint32_t        offset_bytes;   // 相对文件起始
    uint32_t        byte_size;      // 属性数据大小
};

struct RawImage
{
    uint32_t             width, height, channels;    // channels = 4 (RGBA8) after importer
    std::vector<uint8_t> pixels;
};

struct RawMaterial
{
    float   metallicFactor  = 0.0f;
    float   roughnessFactor = 1.0f;
    uint8_t baseColor[4]    = {255, 255, 255, 255};  // sRGBA
    UUID    baseColorTexture{};              // 无则 invalid()
};
}
